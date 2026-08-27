#pragma once

// Zhou, Andersen & Kaminsky
// "Space-Efficient, High-Performance Rank & Select Structures on Uncompressed Bit Sequences", SEA
// 2013.

#include <cds/core/attributes.hpp>
#include <cds/core/broadword.hpp>
#include <cds/core/debug.hpp>
#include <cds/io/byte.hpp>
#include <cds/rank/poppy.hpp>
#include <cds/select/concepts.hpp>

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace cds::detail {

    inline constexpr std::size_t poppy_select_sample_rate = 8192;

    // Bits of `size` that fall within [block_bit_start, +block_bits):
    // block_bits if fully real, less if it straddles `size`, 0 if all padding.
    // Needed because a partial last block padding bits would otherwise be
    // miscounted as real zeros on the select0 side.
    [[nodiscard]] constexpr std::uint64_t poppy_actual_bits(std::uint64_t block_bit_start,
                                                            std::uint64_t block_bits,
                                                            std::uint64_t size) noexcept {
        if (block_bit_start >= size)
            return 0;
        const std::uint64_t remaining = size - block_bit_start;
        return remaining < block_bits ? remaining : block_bits;
    }

    // Builds the flat sample array + per-upper-block offsets for one side
    template <typename Word, pack_endian Endian, bool Ones>
    inline void poppy_select_build(const Word* data, std::size_t size, const std::uint64_t* l0,
                                   std::size_t num_upper_blocks, std::uint64_t upper_block_bits,
                                   std::size_t sample_rate, std::vector<std::uint32_t>& samples,
                                   std::vector<std::uint64_t>& sample_offsets) {
        constexpr std::size_t digits = broadword::word_digits<Word>;

        sample_offsets.assign(num_upper_blocks + 1, 0);
        samples.clear();

        for (std::size_t u = 0; u < num_upper_blocks; ++u) {
            sample_offsets[u] = samples.size();

            const std::uint64_t upper_actual =
                poppy_actual_bits(u * upper_block_bits, upper_block_bits, size);
            const std::uint64_t ones_in_u = l0[u + 1] - l0[u];
            const std::uint64_t target_count_in_u = Ones ? ones_in_u : (upper_actual - ones_in_u);
            if (target_count_in_u == 0)
                continue;

            const std::size_t upper_bit_start = u * upper_block_bits;
            const std::size_t upper_bit_end =
                upper_bit_start + static_cast<std::size_t>(upper_actual);

            std::size_t word_idx = upper_bit_start / digits;
            std::uint64_t seen = 0;

            while (word_idx * digits < upper_bit_end) {
                Word word = data[word_idx];
                if constexpr (!Ones)
                    word = static_cast<Word>(~word);

                const std::size_t word_bit_start = word_idx * digits;

                if (word_bit_start < upper_bit_start) {
                    const std::size_t skip = upper_bit_start - word_bit_start;
                    if constexpr (Endian == pack_endian::lsb)
                        word = static_cast<Word>(word & static_cast<Word>(~Word{0} << skip));
                    else
                        word = static_cast<Word>(word & static_cast<Word>(~Word{0} >> skip));
                }

                while (word != 0) {
                    std::size_t l;
                    if constexpr (Endian == pack_endian::lsb)
                        l = static_cast<std::size_t>(std::countr_zero(word));
                    else
                        l = static_cast<std::size_t>(std::countl_zero(word));

                    const std::size_t pos = word_bit_start + l;
                    if (pos >= upper_bit_end)
                        break;

                    if (seen % sample_rate == 0)
                        samples.push_back(static_cast<std::uint32_t>(pos - upper_bit_start));
                    ++seen;

                    if constexpr (Endian == pack_endian::lsb)
                        word = static_cast<Word>(word & (word - 1));
                    else
                        word = static_cast<Word>(word & ~(Word{1} << (digits - 1 - l)));
                }

                ++word_idx;
            }
        }

        sample_offsets[num_upper_blocks] = samples.size();
    }

    // Precondition: r < total count of the target side
    template <typename Word, pack_endian Endian, bool Ones>
    [[nodiscard]] inline std::size_t
    poppy_select(const Word* data, std::size_t size, const std::uint64_t* l0,
                 std::size_t num_upper_blocks, const std::uint64_t* l1l2,
                 std::size_t num_lower_blocks_total, std::uint64_t upper_block_bits,
                 const std::uint32_t* samples, const std::uint64_t* sample_offsets,
                 std::size_t sample_rate, std::size_t r) noexcept {
        constexpr std::size_t digits = broadword::word_digits<Word>;
        constexpr std::size_t words_per_bb = poppy_words_per_basic_block<Word>;

        auto ones_before_upper = [&](std::size_t u) noexcept { return l0[u]; };
        auto count_before_upper = [&](std::size_t u) noexcept -> std::uint64_t {
            const std::uint64_t ones = ones_before_upper(u);
            if constexpr (Ones)
                return ones;
            else
                return static_cast<std::uint64_t>(u) * upper_block_bits -
                       ones; // always safe, no clamp needed
        };

        // binary search for the upper block
        std::size_t lo = 0, hi = num_upper_blocks;
        while (lo < hi) {
            const std::size_t mid = lo + (hi - lo) / 2;
            if (count_before_upper(mid + 1) <= r)
                lo = mid + 1;
            else
                hi = mid;
        }
        const std::size_t upper_idx = lo;
        const std::uint64_t r_local = r - count_before_upper(upper_idx);

        // sample lookup
        const std::size_t k = static_cast<std::size_t>(r_local / sample_rate);
        const std::uint32_t sample_pos = samples[sample_offsets[upper_idx] + k];
        std::size_t lower_idx_local = sample_pos / poppy_lower_block_bits;
        std::size_t lower_idx_global =
            (upper_idx * upper_block_bits) / poppy_lower_block_bits + lower_idx_local;

        auto count_before_lower_in_upper = [&](std::size_t lb_local,
                                               std::uint64_t combo_at) noexcept -> std::uint64_t {
            const std::uint64_t ones = poppy_l1(combo_at);
            if constexpr (Ones)
                return ones;
            else
                return static_cast<std::uint64_t>(lb_local) * poppy_lower_block_bits -
                       ones; // always safe
        };

        // linear scan forward through lower blocks
        std::uint64_t combo = l1l2[lower_idx_global];
        while (true) {
            // last = the end of the lower-block array, or the next lower
            // block belongs to a later upper block.
            const bool is_last =
                (lower_idx_global + 1 >= num_lower_blocks_total) ||
                (static_cast<std::uint64_t>(lower_idx_global + 1) * poppy_lower_block_bits >=
                 static_cast<std::uint64_t>(upper_idx + 1) * upper_block_bits);

            std::uint64_t next_before;
            if (is_last) {
                // within a possibly-partial upper block: needs the actual-bits
                // clamp, unlike the cumulative counts above.
                const std::uint64_t upper_actual =
                    poppy_actual_bits(static_cast<std::uint64_t>(upper_idx) * upper_block_bits,
                                      upper_block_bits, size);
                const std::uint64_t ones_in_upper = l0[upper_idx + 1] - l0[upper_idx];
                next_before = Ones ? ones_in_upper : (upper_actual - ones_in_upper);
            } else {
                next_before =
                    count_before_lower_in_upper(lower_idx_local + 1, l1l2[lower_idx_global + 1]);
            }

            if (r_local < next_before)
                break;

            ++lower_idx_local;
            ++lower_idx_global;
            combo = l1l2[lower_idx_global];
        }

        const std::uint64_t rel = r_local - count_before_lower_in_upper(lower_idx_local, combo);

        // L2: find the exact basic block
        const std::uint64_t l2_one_0 = poppy_l2(combo, 0);
        const std::uint64_t l2_one_1 = poppy_l2(combo, 1);
        const std::uint64_t l2_one_2 = poppy_l2(combo, 2);

        const std::uint64_t lower_bit_start =
            static_cast<std::uint64_t>(lower_idx_global) * poppy_lower_block_bits;
        auto l2_target = [&](std::size_t bb) noexcept -> std::uint64_t {
            const std::uint64_t one_count = (bb == 0) ? l2_one_0 : (bb == 1) ? l2_one_1 : l2_one_2;
            if constexpr (Ones)
                return one_count;
            else {
                const std::uint64_t bb_bit_start = lower_bit_start + bb * poppy_basic_block_bits;
                const std::uint64_t actual =
                    poppy_actual_bits(bb_bit_start, poppy_basic_block_bits, size);
                return actual - one_count;
            }
        };

        const std::uint64_t t0 = l2_target(0);
        const std::uint64_t t1 = l2_target(1);
        const std::uint64_t t2 = l2_target(2);

        std::size_t bb_idx = 0;
        std::uint64_t bb_base = 0;
        if (rel >= t0) {
            bb_idx = 1;
            bb_base = t0;
        }
        if (rel >= t0 + t1) {
            bb_idx = 2;
            bb_base = t0 + t1;
        }
        if (rel >= t0 + t1 + t2) {
            bb_idx = 3;
            bb_base = t0 + t1 + t2;
        }

        std::size_t within_bb = static_cast<std::size_t>(rel - bb_base);

        // popcount through words, then broadword select in the target word
        const std::size_t bb_word_start =
            (lower_idx_global * poppy_lower_block_bits + bb_idx * poppy_basic_block_bits) / digits;

        for (std::size_t w = 0; w < words_per_bb; ++w) {
            const std::size_t word_bit_start = (bb_word_start + w) * digits;
            Word word = data[bb_word_start + w];
            if constexpr (!Ones)
                word = static_cast<Word>(~word);

            const std::uint64_t actual = poppy_actual_bits(word_bit_start, digits, size);
            if (actual < digits) {
                // mask padding of the last word (~word turns padding into
                // fake 1s on the zero side).
                if constexpr (Endian == pack_endian::lsb)
                    word = static_cast<Word>(word & static_cast<Word>((Word{1} << actual) - 1));
                else
                    word = static_cast<Word>(
                        word & static_cast<Word>(~((Word{1} << (digits - actual)) - 1)));
            }

            const auto c = static_cast<std::size_t>(broadword::popcount(word));
            if (within_bb < c) {
                std::size_t bit;
                if constexpr (std::same_as<Word, std::uint64_t>)
                    bit = broadword::select_in_word<Endian>(word, within_bb);
                else
                    bit = broadword::select_in_word<Endian, Word>(word, within_bb);
                return (bb_word_start + w) * digits + bit;
            }
            within_bb -= c;
        }

        return static_cast<std::size_t>(-1); // unreachable given the precondition
    }

    struct poppy_select_header {
        std::uint32_t magic;
        std::uint32_t reserved{0};
        std::uint64_t sample_rate;
        std::uint64_t samples_n; // 0 if the ones side was not built
        std::uint64_t offsets_n;
        std::uint64_t samples0_n; // 0 if the zeros side was not built
        std::uint64_t offsets0_n;
    };
    static_assert(io::mmap_aligned_header<poppy_select_header>);

    inline constexpr std::uint32_t poppy_select_magic = io::cds_magic(io::format_id::select_poppy);
    [[nodiscard]] inline constexpr std::size_t
    poppy_select_samples_pad(std::uint64_t samples_n) noexcept {
        const std::size_t bytes = static_cast<std::size_t>(samples_n) * sizeof(std::uint32_t);
        return (8 - (bytes & 7)) & 7;
    }

} // namespace cds::detail

namespace cds {

    template <bit_source Source, select_target Target, std::uint64_t UpperBlockBits>
    class select_poppy_view;

    // select_poppy: owning select index built on an existing rank_poppy,
    // adding only a small per-target sample array. select1 and/or select0 per
    // Target.
    //
    // Precondition: the rank_poppy (and its source words) must outlive this.
    template <bit_source Source, select_target Target = select_target::ones,
              std::uint64_t UpperBlockBits = (std::uint64_t{1} << 32)>
    class select_poppy {
    public:
        using word_type = typename bit_source_traits<Source>::word_type;
        using rank_type = rank_poppy<Source, UpperBlockBits>;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;
        static constexpr bool has_ones =
            (Target == select_target::ones || Target == select_target::both);
        static constexpr bool has_zeros =
            (Target == select_target::zeros || Target == select_target::both);

        explicit select_poppy(const rank_type& rank) noexcept
            : m_data(rank.data()), m_size(rank.size()), m_l0(rank.l0()), m_l1l2(rank.l1l2()) {
            const std::size_t num_upper_blocks = m_l0.empty() ? 0 : m_l0.size() - 1;
            if constexpr (has_ones)
                detail::poppy_select_build<word_type, endian, true>(
                    m_data, m_size, m_l0.data(), num_upper_blocks, UpperBlockBits,
                    detail::poppy_select_sample_rate, m_samples, m_sample_offsets);
            if constexpr (has_zeros)
                detail::poppy_select_build<word_type, endian, false>(
                    m_data, m_size, m_l0.data(), num_upper_blocks, UpperBlockBits,
                    detail::poppy_select_sample_rate, m_samples0, m_sample_offsets0);
        }

        [[nodiscard]] std::size_t select1(std::size_t r) const noexcept
            requires has_ones
        {
            const std::size_t num_upper_blocks = m_l0.empty() ? 0 : m_l0.size() - 1;
            CDS_ASSERT(!m_l0.empty() && r < m_l0.back(),
                       "select_poppy::select1: rank {} out of range (total = {})", r,
                       m_l0.empty() ? 0 : m_l0.back());
            return detail::poppy_select<word_type, endian, true>(
                m_data, m_size, m_l0.data(), num_upper_blocks, m_l1l2.data(), m_l1l2.size(),
                UpperBlockBits, m_samples.data(), m_sample_offsets.data(),
                detail::poppy_select_sample_rate, r);
        }

        [[nodiscard]] std::size_t select0(std::size_t r) const noexcept
            requires has_zeros
        {
            const std::size_t num_upper_blocks = m_l0.empty() ? 0 : m_l0.size() - 1;
            [[maybe_unused]] const std::uint64_t total_zeros =
                m_size - (m_l0.empty() ? 0 : m_l0.back());
            CDS_ASSERT(r < total_zeros, "select_poppy::select0: rank {} out of range (total = {})",
                       r, total_zeros);
            return detail::poppy_select<word_type, endian, false>(
                m_data, m_size, m_l0.data(), num_upper_blocks, m_l1l2.data(), m_l1l2.size(),
                UpperBlockBits, m_samples0.data(), m_sample_offsets0.data(),
                detail::poppy_select_sample_rate, r);
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }

        [[nodiscard]] select_poppy_view<Source, Target, UpperBlockBits> as_view() const noexcept;

        [[nodiscard]] std::size_t memory_size() const noexcept {
            std::size_t bytes = sizeof(*this);
            if constexpr (has_ones)
                bytes += m_samples.size() * sizeof(std::uint32_t) +
                         m_sample_offsets.size() * sizeof(std::uint64_t);
            if constexpr (has_zeros)
                bytes += m_samples0.size() * sizeof(std::uint32_t) +
                         m_sample_offsets0.size() * sizeof(std::uint64_t);
            return bytes;
        }

        template <typename Sink> [[nodiscard]] bool save(Sink& sink) const noexcept {
            if (!io::write_cds_version(sink))
                return false;

            detail::poppy_select_header h{};
            h.magic = detail::poppy_select_magic;
            h.sample_rate = detail::poppy_select_sample_rate;
            if constexpr (has_ones) {
                h.samples_n = m_samples.size();
                h.offsets_n = m_sample_offsets.size();
            }
            if constexpr (has_zeros) {
                h.samples0_n = m_samples0.size();
                h.offsets0_n = m_sample_offsets0.size();
            }

            if (!sink.write(&h, sizeof(h)))
                return false;

            static constexpr unsigned char zeros[8] = {};
            if constexpr (has_ones) {
                if (!m_samples.empty() &&
                    !sink.write(m_samples.data(), m_samples.size() * sizeof(std::uint32_t)))
                    return false;
                if (const std::size_t pad = detail::poppy_select_samples_pad(m_samples.size());
                    pad > 0 && !sink.write(zeros, pad))
                    return false;
                if (!m_sample_offsets.empty() &&
                    !sink.write(m_sample_offsets.data(),
                                m_sample_offsets.size() * sizeof(std::uint64_t)))
                    return false;
            }
            if constexpr (has_zeros) {
                if (!m_samples0.empty() &&
                    !sink.write(m_samples0.data(), m_samples0.size() * sizeof(std::uint32_t)))
                    return false;
                if (const std::size_t pad = detail::poppy_select_samples_pad(m_samples0.size());
                    pad > 0 && !sink.write(zeros, pad))
                    return false;
                if (!m_sample_offsets0.empty() &&
                    !sink.write(m_sample_offsets0.data(),
                                m_sample_offsets0.size() * sizeof(std::uint64_t)))
                    return false;
            }

            return true;
        }

        template <typename ByteSource>
        [[nodiscard]] static std::expected<select_poppy, io::load_error>
        load(ByteSource& reader, const rank_type& rank) {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::poppy_select_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::poppy_select_magic)
                return std::unexpected(io::load_error::bad_magic);
            if (h.sample_rate != detail::poppy_select_sample_rate)
                return std::unexpected(io::load_error::type_mismatch);

            select_poppy s(deserialize_tag{}, rank);

            if constexpr (has_ones) {
                s.m_samples.resize(static_cast<std::size_t>(h.samples_n));
                s.m_sample_offsets.resize(static_cast<std::size_t>(h.offsets_n));
                if (h.samples_n > 0 &&
                    !reader.read(s.m_samples.data(),
                                 static_cast<std::size_t>(h.samples_n) * sizeof(std::uint32_t)))
                    return std::unexpected(io::load_error::io_failure);
                if (const std::size_t pad = detail::poppy_select_samples_pad(h.samples_n);
                    pad > 0 && !reader.skip(pad))
                    return std::unexpected(io::load_error::io_failure);
                if (h.offsets_n > 0 &&
                    !reader.read(s.m_sample_offsets.data(),
                                 static_cast<std::size_t>(h.offsets_n) * sizeof(std::uint64_t)))
                    return std::unexpected(io::load_error::io_failure);
            } else {
                if (!reader.skip(static_cast<std::size_t>(h.samples_n) * sizeof(std::uint32_t)))
                    return std::unexpected(io::load_error::io_failure);
                if (const std::size_t pad = detail::poppy_select_samples_pad(h.samples_n);
                    pad > 0 && !reader.skip(pad))
                    return std::unexpected(io::load_error::io_failure);
                if (!reader.skip(static_cast<std::size_t>(h.offsets_n) * sizeof(std::uint64_t)))
                    return std::unexpected(io::load_error::io_failure);
            }

            if constexpr (has_zeros) {
                s.m_samples0.resize(static_cast<std::size_t>(h.samples0_n));
                s.m_sample_offsets0.resize(static_cast<std::size_t>(h.offsets0_n));
                if (h.samples0_n > 0 &&
                    !reader.read(s.m_samples0.data(),
                                 static_cast<std::size_t>(h.samples0_n) * sizeof(std::uint32_t)))
                    return std::unexpected(io::load_error::io_failure);
                if (const std::size_t pad = detail::poppy_select_samples_pad(h.samples0_n);
                    pad > 0 && !reader.skip(pad))
                    return std::unexpected(io::load_error::io_failure);
                if (h.offsets0_n > 0 &&
                    !reader.read(s.m_sample_offsets0.data(),
                                 static_cast<std::size_t>(h.offsets0_n) * sizeof(std::uint64_t)))
                    return std::unexpected(io::load_error::io_failure);
            } else {
                if (!reader.skip(static_cast<std::size_t>(h.samples0_n) * sizeof(std::uint32_t)))
                    return std::unexpected(io::load_error::io_failure);
                if (const std::size_t pad = detail::poppy_select_samples_pad(h.samples0_n);
                    pad > 0 && !reader.skip(pad))
                    return std::unexpected(io::load_error::io_failure);
                if (!reader.skip(static_cast<std::size_t>(h.offsets0_n) * sizeof(std::uint64_t)))
                    return std::unexpected(io::load_error::io_failure);
            }

            return s;
        }

    private:
        struct deserialize_tag {};

        select_poppy(deserialize_tag, const rank_type& rank) noexcept
            : m_data(rank.data()), m_size(rank.size()), m_l0(rank.l0()), m_l1l2(rank.l1l2()) {}

        const word_type* m_data;
        std::size_t m_size;
        std::span<const std::uint64_t> m_l0;
        std::span<const std::uint64_t> m_l1l2;

        CDS_NO_UNIQUE_ADDRESS
        std::conditional_t<has_ones, std::vector<std::uint32_t>, detail::empty_storage> m_samples;
        CDS_NO_UNIQUE_ADDRESS
        std::conditional_t<has_ones, std::vector<std::uint64_t>, detail::empty_storage>
            m_sample_offsets;
        CDS_NO_UNIQUE_ADDRESS
        std::conditional_t<has_zeros, std::vector<std::uint32_t>, detail::empty_storage> m_samples0;
        CDS_NO_UNIQUE_ADDRESS
        std::conditional_t<has_zeros, std::vector<std::uint64_t>, detail::empty_storage>
            m_sample_offsets0;
    };

    // select_poppy_view: non-owning
    // Binds to a pre-built rank_poppy index and pre-built sample arrays
    // The caller keeps the rank index, the samples, and the source words
    // valid for the view lifetime.
    template <bit_source Source, select_target Target = select_target::ones,
              std::uint64_t UpperBlockBits = (std::uint64_t{1} << 32)>
    class select_poppy_view {
    public:
        using word_type = typename bit_source_traits<Source>::word_type;
        using rank_view_type = rank_poppy_view<Source, UpperBlockBits>;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;
        static constexpr bool has_ones =
            (Target == select_target::ones || Target == select_target::both);
        static constexpr bool has_zeros =
            (Target == select_target::zeros || Target == select_target::both);

        select_poppy_view(const word_type* data, std::size_t size,
                          std::span<const std::uint64_t> l0, std::span<const std::uint64_t> l1l2,
                          std::span<const std::uint32_t> samples,
                          std::span<const std::uint64_t> sample_offsets,
                          std::span<const std::uint32_t> samples0,
                          std::span<const std::uint64_t> sample_offsets0) noexcept
            : m_data(data), m_size(size), m_l0(l0), m_l1l2(l1l2), m_samples(samples),
              m_sample_offsets(sample_offsets), m_samples0(samples0),
              m_sample_offsets0(sample_offsets0) {}

        [[nodiscard]] std::size_t select1(std::size_t r) const noexcept
            requires has_ones
        {
            const std::size_t num_upper_blocks = m_l0.empty() ? 0 : m_l0.size() - 1;
            CDS_ASSERT(!m_l0.empty() && r < m_l0.back(),
                       "select_poppy_view::select1: rank {} out of range (total = {})", r,
                       m_l0.empty() ? 0 : m_l0.back());
            return detail::poppy_select<word_type, endian, true>(
                m_data, m_size, m_l0.data(), num_upper_blocks, m_l1l2.data(), m_l1l2.size(),
                UpperBlockBits, m_samples.data(), m_sample_offsets.data(),
                detail::poppy_select_sample_rate, r);
        }

        [[nodiscard]] std::size_t select0(std::size_t r) const noexcept
            requires has_zeros
        {
            const std::size_t num_upper_blocks = m_l0.empty() ? 0 : m_l0.size() - 1;
            [[maybe_unused]] const std::uint64_t total_zeros =
                m_size - (m_l0.empty() ? 0 : m_l0.back());
            CDS_ASSERT(r < total_zeros,
                       "select_poppy_view::select0: rank {} out of range (total = {})", r,
                       total_zeros);
            return detail::poppy_select<word_type, endian, false>(
                m_data, m_size, m_l0.data(), num_upper_blocks, m_l1l2.data(), m_l1l2.size(),
                UpperBlockBits, m_samples0.data(), m_sample_offsets0.data(),
                detail::poppy_select_sample_rate, r);
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }
        [[nodiscard]] const word_type* data() const noexcept {
            return m_data;
        }

        template <typename ByteSource>
            requires io::span_source<ByteSource>
        [[nodiscard]] static std::expected<select_poppy_view, io::load_error>
        load(ByteSource& reader, const rank_view_type& rank) noexcept {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::poppy_select_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::poppy_select_magic)
                return std::unexpected(io::load_error::bad_magic);
            if (h.sample_rate != detail::poppy_select_sample_rate)
                return std::unexpected(io::load_error::type_mismatch);

            std::span<const std::uint32_t> samples, samples0;
            std::span<const std::uint64_t> offsets, offsets0;

            auto map_side = [&](std::uint64_t samples_n, std::uint64_t offsets_n, bool keep,
                                std::span<const std::uint32_t>& s,
                                std::span<const std::uint64_t>& o) -> bool {
                const std::size_t s_bytes =
                    static_cast<std::size_t>(samples_n) * sizeof(std::uint32_t);
                const std::span<const std::byte> s_span = reader.view(s_bytes);
                if (s_span.size() != s_bytes)
                    return false;
                if (const std::size_t pad = detail::poppy_select_samples_pad(samples_n);
                    pad > 0 && reader.view(pad).size() != pad)
                    return false;
                const std::size_t o_bytes =
                    static_cast<std::size_t>(offsets_n) * sizeof(std::uint64_t);
                const std::span<const std::byte> o_span = reader.view(o_bytes);
                if (o_span.size() != o_bytes)
                    return false;
                if (keep) {
                    s = std::span<const std::uint32_t>(
                        reinterpret_cast<const std::uint32_t*>(s_span.data()),
                        static_cast<std::size_t>(samples_n));
                    o = std::span<const std::uint64_t>(
                        reinterpret_cast<const std::uint64_t*>(o_span.data()),
                        static_cast<std::size_t>(offsets_n));
                }
                return true;
            };

            if (!map_side(h.samples_n, h.offsets_n, has_ones, samples, offsets))
                return std::unexpected(io::load_error::io_failure);
            if (!map_side(h.samples0_n, h.offsets0_n, has_zeros, samples0, offsets0))
                return std::unexpected(io::load_error::io_failure);

            return select_poppy_view(rank.data(), rank.size(), rank.l0(), rank.l1l2(), samples,
                                     offsets, samples0, offsets0);
        }

    private:
        const word_type* m_data;
        std::size_t m_size;
        std::span<const std::uint64_t> m_l0;
        std::span<const std::uint64_t> m_l1l2;
        std::span<const std::uint32_t> m_samples;
        std::span<const std::uint64_t> m_sample_offsets;
        std::span<const std::uint32_t> m_samples0;
        std::span<const std::uint64_t> m_sample_offsets0;
    };

    template <bit_source Source, select_target Target, std::uint64_t UpperBlockBits>
    select_poppy_view<Source, Target, UpperBlockBits>
    select_poppy<Source, Target, UpperBlockBits>::as_view() const noexcept {
        std::span<const std::uint32_t> s1{}, s01{};
        std::span<const std::uint64_t> o1{}, o01{};
        if constexpr (has_ones) {
            s1 = std::span<const std::uint32_t>(m_samples);
            o1 = std::span<const std::uint64_t>(m_sample_offsets);
        }
        if constexpr (has_zeros) {
            s01 = std::span<const std::uint32_t>(m_samples0);
            o01 = std::span<const std::uint64_t>(m_sample_offsets0);
        }
        return select_poppy_view<Source, Target, UpperBlockBits>(m_data, m_size, m_l0, m_l1l2, s1,
                                                                 o1, s01, o01);
    }

} // namespace cds
