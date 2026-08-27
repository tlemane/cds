#pragma once

// Zhou, Andersen & Kaminsky
// "Space-Efficient, High-Performance Rank & Select Structures on Uncompressed Bit Sequences", SEA
// 2013. https://www.cs.cmu.edu/~dga/papers/zhou-sea2013.pdf

#include <cds/bit/interface.hpp>
#include <cds/core/broadword.hpp>
#include <cds/core/debug.hpp>
#include <cds/io/byte.hpp>
#include <cds/rank/concepts.hpp>

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace cds::detail {

    inline constexpr std::size_t poppy_basic_block_bits = 512;
    inline constexpr std::size_t poppy_lower_block_bits = 2048;    // 4 basic blocks
    inline constexpr std::size_t poppy_basic_blocks_per_lower = 4;
    inline constexpr std::uint64_t poppy_l2_mask = 0x3FFull;       // 10 bits

    template <typename Word>
    inline constexpr std::size_t poppy_words_per_basic_block =
        poppy_basic_block_bits / broadword::word_digits<Word>;

    // combo (64 bits): [ L1:32 | pad:2 | L2[2]:10 | L2[1]:10 | L2[0]:10 ]
    [[nodiscard]] constexpr std::uint64_t poppy_pack_combo(std::uint32_t l1, std::uint32_t l2_0,
                                                           std::uint32_t l2_1,
                                                           std::uint32_t l2_2) noexcept {
        return (static_cast<std::uint64_t>(l1) << 32) |
               ((static_cast<std::uint64_t>(l2_2) & poppy_l2_mask) << 20) |
               ((static_cast<std::uint64_t>(l2_1) & poppy_l2_mask) << 10) |
               (static_cast<std::uint64_t>(l2_0) & poppy_l2_mask);
    }

    [[nodiscard]] constexpr std::uint32_t poppy_l1(std::uint64_t combo) noexcept {
        return static_cast<std::uint32_t>(combo >> 32);
    }

    // idx in [0, 2], block 3 count is derived from the sum of these three.
    [[nodiscard]] constexpr std::uint32_t poppy_l2(std::uint64_t combo, std::size_t idx) noexcept {
        return static_cast<std::uint32_t>((combo >> (10 * idx)) & poppy_l2_mask);
    }

    template <typename Word, pack_endian Endian>
    inline void poppy_build(const Word* data, std::size_t size, std::uint64_t upper_block_bits,
                            std::vector<std::uint64_t>& l0, std::vector<std::uint64_t>& l1l2) {
        constexpr std::size_t digits = broadword::word_digits<Word>;
        constexpr std::size_t words_per_bb = poppy_words_per_basic_block<Word>;
        constexpr std::size_t bb_per_lower = poppy_basic_blocks_per_lower;

        const std::size_t num_lower_blocks =
            (size + poppy_lower_block_bits - 1) / poppy_lower_block_bits;
        const std::size_t num_upper_blocks = (size == 0) ? 0 : ((size - 1) / upper_block_bits) + 1;

        l0.assign(num_upper_blocks + 1, 0); // +1 sentinel
        // +1 sentinel lower block: read by rank1(size) when size is an exact
        // multiple of poppy_lower_block_bits. Allows to keep the query branch-free
        l1l2.assign(num_lower_blocks + 1, 0);

        std::uint64_t absolute = 0;
        std::uint64_t upper_running = 0;
        std::size_t current_upper = 0;

        for (std::size_t lb = 0; lb < num_lower_blocks; ++lb) {
            const std::size_t lower_bit_start = lb * poppy_lower_block_bits;
            const std::size_t this_upper = lower_bit_start / upper_block_bits;

            // Normally one upper-block boundary is crossed per lower block.
            // (only happens with tiny synthetic test sizes)
            if (this_upper != current_upper) {
                for (std::size_t u = current_upper + 1; u <= this_upper; ++u)
                    l0[u] = absolute;
                current_upper = this_upper;
                upper_running = 0;
            }

            std::uint32_t l2[3] = {0, 0, 0};
            std::uint64_t lower_total = 0;

            for (std::size_t bb = 0; bb < bb_per_lower; ++bb) {
                std::uint64_t bb_count = 0;
                const std::size_t bb_word_start = (lower_bit_start / digits) + bb * words_per_bb;

                for (std::size_t w = 0; w < words_per_bb; ++w) {
                    const std::size_t word_idx = bb_word_start + w;
                    const std::size_t word_bit_start = word_idx * digits;
                    if (word_bit_start >= size)
                        break;

                    Word word = data[word_idx];
                    const std::size_t valid_bits = size - word_bit_start;
                    if (valid_bits < digits) {
                        // Mask the padding bits of the final partial word before
                        // popcounting: build sums complete basic blocks, so it
                        // must not count past size. rank() query-time reads
                        // need no such mask (every full word touched is within
                        // [0, x) given the x <= size precondition).
                        if constexpr (Endian == pack_endian::lsb)
                            word = static_cast<Word>(
                                word & static_cast<Word>((Word{1} << valid_bits) - 1));
                        else
                            word = static_cast<Word>(
                                word &
                                static_cast<Word>(~((Word{1} << (digits - valid_bits)) - 1)));
                    }
                    bb_count += broadword::popcount(word);
                }

                if (bb < 3)
                    l2[bb] = static_cast<std::uint32_t>(bb_count);
                lower_total += bb_count;
            }

            l1l2[lb] =
                poppy_pack_combo(static_cast<std::uint32_t>(upper_running), l2[0], l2[1], l2[2]);

            upper_running += lower_total;
            absolute += lower_total;
        }

        if (size != 0) {
            l0[current_upper + 1] = absolute; // sentinel for the final upper block

            // Sentinel lower block: its L1 holds the tail count relative to the
            // upper block `size` lands in, so rank1(size) == total in every
            // UpperBlockBits config with no query-time special case
            const std::size_t upper_at_size = size / upper_block_bits;
            l1l2[num_lower_blocks] =
                poppy_pack_combo(static_cast<std::uint32_t>(absolute - l0[upper_at_size]), 0, 0, 0);
        }
    }

    // Precondition: x <= size.
    template <typename Word, pack_endian Endian>
    [[nodiscard]] inline std::size_t
    poppy_rank1(const Word* data, const std::uint64_t* l0, const std::uint64_t* l1l2,
                std::uint64_t upper_block_bits, std::size_t x) noexcept {
        constexpr std::size_t digits = broadword::word_digits<Word>;

        const std::size_t upper_idx = x / upper_block_bits;
        const std::uint64_t p = l0[upper_idx];

        const std::size_t x_in_upper = x % upper_block_bits;
        const std::size_t lower_idx_local = x_in_upper / poppy_lower_block_bits;
        const std::size_t lower_idx_global =
            (upper_idx * upper_block_bits) / poppy_lower_block_bits + lower_idx_local;

        const std::uint64_t combo = l1l2[lower_idx_global];
        const std::uint32_t q = poppy_l1(combo);

        const std::size_t x_in_lower = x_in_upper % poppy_lower_block_bits;
        const std::size_t bb_idx = x_in_lower / poppy_basic_block_bits;

        std::uint32_t l2_sum = 0;
        if (bb_idx >= 1)
            l2_sum += poppy_l2(combo, 0);
        if (bb_idx >= 2)
            l2_sum += poppy_l2(combo, 1);
        if (bb_idx >= 3)
            l2_sum += poppy_l2(combo, 2);

        const std::size_t x_in_bb = x_in_lower % poppy_basic_block_bits;
        const std::size_t bb_word_start = (x - x_in_bb) / digits;
        const std::size_t word_in_bb = x_in_bb / digits;
        const std::size_t bit_in_word = x_in_bb % digits;

        std::size_t r = 0;
        for (std::size_t w = 0; w < word_in_bb; ++w)
            r += static_cast<std::size_t>(broadword::popcount(data[bb_word_start + w]));

        if (bit_in_word > 0) {
            if constexpr (std::same_as<Word, std::uint64_t>)
                r += broadword::popcount_below<Endian>(data[bb_word_start + word_in_bb],
                                                       bit_in_word);
            else
                r += broadword::popcount_below<Endian, Word>(data[bb_word_start + word_in_bb],
                                                             bit_in_word);
        }

        return static_cast<std::size_t>(p) + q + l2_sum + r;
    }

    // Fused poppy rank1(x) + bit at x. rank1 already reads the word holding bit
    // x for its partial popcount, so the bit comes free.
    // Precondition: x < size (the word is always dereferenced, unlike rank1 at a word boundary)
    template <typename Word, pack_endian Endian>
    [[nodiscard]] inline rank_bit
    poppy_rank1_bit(const Word* data, const std::uint64_t* l0, const std::uint64_t* l1l2,
                    std::uint64_t upper_block_bits, std::size_t x) noexcept {
        constexpr std::size_t digits = broadword::word_digits<Word>;

        const std::size_t upper_idx = x / upper_block_bits;
        const std::uint64_t p = l0[upper_idx];

        const std::size_t x_in_upper = x % upper_block_bits;
        const std::size_t lower_idx_local = x_in_upper / poppy_lower_block_bits;
        const std::size_t lower_idx_global =
            (upper_idx * upper_block_bits) / poppy_lower_block_bits + lower_idx_local;

        const std::uint64_t combo = l1l2[lower_idx_global];
        const std::uint32_t q = poppy_l1(combo);

        const std::size_t x_in_lower = x_in_upper % poppy_lower_block_bits;
        const std::size_t bb_idx = x_in_lower / poppy_basic_block_bits;

        std::uint32_t l2_sum = 0;
        if (bb_idx >= 1)
            l2_sum += poppy_l2(combo, 0);
        if (bb_idx >= 2)
            l2_sum += poppy_l2(combo, 1);
        if (bb_idx >= 3)
            l2_sum += poppy_l2(combo, 2);

        const std::size_t x_in_bb = x_in_lower % poppy_basic_block_bits;
        const std::size_t bb_word_start = (x - x_in_bb) / digits;
        const std::size_t word_in_bb = x_in_bb / digits;
        const std::size_t bit_in_word = x_in_bb % digits;

        std::size_t r = 0;
        for (std::size_t w = 0; w < word_in_bb; ++w)
            r += static_cast<std::size_t>(broadword::popcount(data[bb_word_start + w]));

        const Word word = data[bb_word_start + word_in_bb]; // word holding bit x
        if (bit_in_word > 0) {
            if constexpr (std::same_as<Word, std::uint64_t>)
                r += broadword::popcount_below<Endian>(word, bit_in_word);
            else
                r += broadword::popcount_below<Endian, Word>(word, bit_in_word);
        }

        const std::size_t shift =
            (Endian == pack_endian::lsb) ? bit_in_word : (digits - 1 - bit_in_word);
        const bool b = ((word >> shift) & Word{1}) != 0;

        return {static_cast<std::size_t>(p) + q + l2_sum + r, b};
    }

    struct poppy_header {
        std::uint32_t magic;
        std::uint32_t reserved{0};
        std::uint64_t size;
        std::uint64_t upper_block_bits;
        std::uint64_t l0_n;
        std::uint64_t l1l2_n;
    };
    static_assert(io::mmap_aligned_header<poppy_header>);

    inline constexpr std::uint32_t poppy_magic = io::cds_magic(io::format_id::rank_poppy);

}

namespace cds {

    template <bit_source Source, std::uint64_t UpperBlockBits> class rank_poppy_view;

    // rank_poppy: owning poppy rank index (constant-time rank, ~3.5% space).
    // Reference-holding, with the same precondition as rank9: the source words
    // must outlive it and must not be reallocated while it is alive.
    //
    // UpperBlockBits defaults to 2^32
    template <bit_source Source, std::uint64_t UpperBlockBits = (std::uint64_t{1} << 32)>
    class rank_poppy {
    public:
        using word_type = typename bit_source_traits<Source>::word_type;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;

        explicit rank_poppy(const Source& source) noexcept
            : m_data(bit_source_traits<Source>::data(source)),
              m_size(bit_source_traits<Source>::size(source)) {
            CDS_ASSERT(
                bit_source_traits<Source>::offset(source) == 0,
                "rank_poppy: sliced/offset bit_view sources are not supported yet (offset={})",
                bit_source_traits<Source>::offset(source));
            detail::poppy_build<word_type, endian>(m_data, m_size, UpperBlockBits, m_l0, m_l1l2);
        }

        [[nodiscard]] std::size_t rank1(std::size_t i) const noexcept {
            CDS_ASSERT(i <= m_size, "rank_poppy::rank1: index {} exceeds size {}", i, m_size);
            return detail::poppy_rank1<word_type, endian>(m_data, m_l0.data(), m_l1l2.data(),
                                                          UpperBlockBits, i);
        }

        // rank1(i) and the bit at i from the words rank1 already reads. Precondition: i < size.
        [[nodiscard]] rank_bit rank1_bit(std::size_t i) const noexcept {
            CDS_ASSERT(i < m_size, "rank_poppy::rank1_bit: index {} exceeds size {}", i, m_size);
            return detail::poppy_rank1_bit<word_type, endian>(m_data, m_l0.data(), m_l1l2.data(),
                                                              UpperBlockBits, i);
        }

        [[nodiscard]] std::size_t rank0(std::size_t i) const noexcept {
            return i - rank1(i);
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }
        [[nodiscard]] const word_type* data() const noexcept {
            return m_data;
        }

        [[nodiscard]] std::span<const std::uint64_t> l0() const noexcept {
            return m_l0;
        }
        [[nodiscard]] std::span<const std::uint64_t> l1l2() const noexcept {
            return m_l1l2;
        }

        // Non-owning view over this index and the same source words. Valid only
        // while this rank_poppy (and its source) stay alive.
        [[nodiscard]] rank_poppy_view<Source, UpperBlockBits> as_view() const noexcept;

        [[nodiscard]] std::size_t memory_size() const noexcept {
            return sizeof(*this) + m_l0.size() * sizeof(std::uint64_t) +
                   m_l1l2.size() * sizeof(std::uint64_t);
        }

        template <typename Sink> [[nodiscard]] bool save(Sink& sink) const noexcept {
            if (!io::write_cds_version(sink))
                return false;

            const detail::poppy_header h{detail::poppy_magic,
                                         0,
                                         static_cast<std::uint64_t>(m_size),
                                         UpperBlockBits,
                                         static_cast<std::uint64_t>(m_l0.size()),
                                         static_cast<std::uint64_t>(m_l1l2.size())};
            if (!sink.write(&h, sizeof(h)))
                return false;

            if (!m_l0.empty() && !sink.write(m_l0.data(), m_l0.size() * sizeof(std::uint64_t)))
                return false;
            if (!m_l1l2.empty() &&
                !sink.write(m_l1l2.data(), m_l1l2.size() * sizeof(std::uint64_t)))
                return false;

            return true;
        }

        template <typename ByteSource>
        [[nodiscard]] static std::expected<rank_poppy, io::load_error> load(ByteSource& reader,
                                                                            const Source& source) {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::poppy_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::poppy_magic)
                return std::unexpected(io::load_error::bad_magic);
            if (h.upper_block_bits != UpperBlockBits)
                return std::unexpected(io::load_error::type_mismatch);

            const std::size_t source_size = bit_source_traits<Source>::size(source);
            if (h.size != source_size)
                return std::unexpected(io::load_error::size_mismatch);

            rank_poppy r(deserialize_tag{}, source);
            r.m_l0.resize(static_cast<std::size_t>(h.l0_n));
            r.m_l1l2.resize(static_cast<std::size_t>(h.l1l2_n));

            if (h.l0_n > 0 && !reader.read(r.m_l0.data(), static_cast<std::size_t>(h.l0_n) *
                                                              sizeof(std::uint64_t)))
                return std::unexpected(io::load_error::io_failure);
            if (h.l1l2_n > 0 && !reader.read(r.m_l1l2.data(), static_cast<std::size_t>(h.l1l2_n) *
                                                                  sizeof(std::uint64_t)))
                return std::unexpected(io::load_error::io_failure);

            return r;
        }

    private:
        struct deserialize_tag {};

        rank_poppy(deserialize_tag, const Source& source) noexcept
            : m_data(bit_source_traits<Source>::data(source)),
              m_size(bit_source_traits<Source>::size(source)) {}

        const word_type* m_data;
        std::size_t m_size;
        std::vector<std::uint64_t> m_l0;
        std::vector<std::uint64_t> m_l1l2;
    };

    // rank_poppy_view: non-owning. Binds to a pre-built index (e.g. mmap'd, or
    // another rank_poppy's l0()/l1l2()) with no build pass and no copy. The
    // caller keeps l0, l1l2 and the source words valid for the view lifetime.
    template <bit_source Source, std::uint64_t UpperBlockBits = (std::uint64_t{1} << 32)>
    class rank_poppy_view {
    public:
        using word_type = typename bit_source_traits<Source>::word_type;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;

        rank_poppy_view(std::span<const std::uint64_t> l0, std::span<const std::uint64_t> l1l2,
                        const Source& source) noexcept
            : m_data(bit_source_traits<Source>::data(source)),
              m_size(bit_source_traits<Source>::size(source)), m_l0(l0), m_l1l2(l1l2) {
            CDS_ASSERT(
                bit_source_traits<Source>::offset(source) == 0,
                "rank_poppy_view: sliced/offset bit_view sources are not supported yet (offset={})",
                bit_source_traits<Source>::offset(source));
        }

        rank_poppy_view(const word_type* data, std::size_t size, std::span<const std::uint64_t> l0,
                        std::span<const std::uint64_t> l1l2) noexcept
            : m_data(data), m_size(size), m_l0(l0), m_l1l2(l1l2) {}

        [[nodiscard]] std::size_t rank1(std::size_t i) const noexcept {
            CDS_ASSERT(i <= m_size, "rank_poppy_view::rank1: index {} exceeds size {}", i, m_size);
            return detail::poppy_rank1<word_type, endian>(m_data, m_l0.data(), m_l1l2.data(),
                                                          UpperBlockBits, i);
        }

        // rank1(i) and the bit at i from the words rank1 already reads. Precondition: i < size.
        [[nodiscard]] rank_bit rank1_bit(std::size_t i) const noexcept {
            CDS_ASSERT(i < m_size, "rank_poppy_view::rank1_bit: index {} exceeds size {}", i,
                       m_size);
            return detail::poppy_rank1_bit<word_type, endian>(m_data, m_l0.data(), m_l1l2.data(),
                                                              UpperBlockBits, i);
        }

        [[nodiscard]] std::size_t rank0(std::size_t i) const noexcept {
            return i - rank1(i);
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }
        [[nodiscard]] const word_type* data() const noexcept {
            return m_data;
        }

        [[nodiscard]] std::span<const std::uint64_t> l0() const noexcept {
            return m_l0;
        }
        [[nodiscard]] std::span<const std::uint64_t> l1l2() const noexcept {
            return m_l1l2;
        }

        template <typename ByteSource>
            requires io::span_source<ByteSource>
        [[nodiscard]] static std::expected<rank_poppy_view, io::load_error>
        load(ByteSource& reader, const Source& source) noexcept {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::poppy_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::poppy_magic)
                return std::unexpected(io::load_error::bad_magic);
            if (h.upper_block_bits != UpperBlockBits)
                return std::unexpected(io::load_error::type_mismatch);

            const std::size_t source_size = bit_source_traits<Source>::size(source);
            if (h.size != source_size)
                return std::unexpected(io::load_error::size_mismatch);

            const std::size_t l0_byte_len =
                static_cast<std::size_t>(h.l0_n) * sizeof(std::uint64_t);
            const std::span<const std::byte> l0_bytes = reader.view(l0_byte_len);
            if (l0_bytes.size() != l0_byte_len)
                return std::unexpected(io::load_error::io_failure);

            const std::size_t l1l2_byte_len =
                static_cast<std::size_t>(h.l1l2_n) * sizeof(std::uint64_t);
            const std::span<const std::byte> l1l2_bytes = reader.view(l1l2_byte_len);
            if (l1l2_bytes.size() != l1l2_byte_len)
                return std::unexpected(io::load_error::io_failure);

            const std::span<const std::uint64_t> l0(
                reinterpret_cast<const std::uint64_t*>(l0_bytes.data()),
                static_cast<std::size_t>(h.l0_n));
            const std::span<const std::uint64_t> l1l2(
                reinterpret_cast<const std::uint64_t*>(l1l2_bytes.data()),
                static_cast<std::size_t>(h.l1l2_n));

            return rank_poppy_view(l0, l1l2, source);
        }

    private:
        const word_type* m_data;
        std::size_t m_size;
        std::span<const std::uint64_t> m_l0;
        std::span<const std::uint64_t> m_l1l2;
    };

    template <bit_source Source, std::uint64_t UpperBlockBits>
    rank_poppy_view<Source, UpperBlockBits>
    rank_poppy<Source, UpperBlockBits>::as_view() const noexcept {
        return rank_poppy_view<Source, UpperBlockBits>(m_data, m_size, m_l0, m_l1l2);
    }

} // namespace cds
