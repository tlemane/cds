#pragma once

// Vigna
// "Broadword Implementation of Rank/Select Queries", WEA 2008.

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include <cds/core/attributes.hpp>
#include <cds/core/debug.hpp>
#include <cds/core/broadword.hpp>
#include <cds/io/byte.hpp>
#include <cds/select/concepts.hpp>
#include <cds/rank/rank9.hpp>

namespace cds::detail {

    // Hint sampling interval
    inline constexpr std::size_t select9_hint_spacing = 8192;

    template <typename Word, bool Ones>
    [[nodiscard]] constexpr std::uint64_t select9_count_before(const rank9_superblock& entry,
                                                               std::size_t sb) noexcept {
        if constexpr (Ones)
            return entry.absolute;
        else
            return static_cast<std::uint64_t>(sb) * rank9_superblock_bits<Word> - entry.absolute;
    }

    template <bool Ones>
    [[nodiscard]] constexpr std::uint64_t select9_total(std::uint64_t size,
                                                        std::uint64_t total_ones) noexcept {
        if constexpr (Ones)
            return total_ones;
        else
            return size - total_ones;
    }

    // Builds the hints array (Ones: select1 side, else select0) from a rank9 superblock array.
    template <typename Word, bool Ones>
    inline void select9_build(const rank9_superblock* superblocks, std::size_t real_nsb,
                              std::uint64_t size, std::vector<std::uint64_t>& hints) {
        const std::uint64_t total_ones = superblocks[real_nsb].absolute;
        const std::uint64_t total = select9_total<Ones>(size, total_ones);

        if (total == 0) {
            hints.clear();
            return;
        }

        const auto nhints = static_cast<std::size_t>((total - 1) / select9_hint_spacing) + 1;
        hints.assign(nhints, 0);

        std::size_t sb = 0;
        for (std::size_t k = 0; k < nhints; ++k) {
            const auto target = static_cast<std::uint64_t>(k) * select9_hint_spacing;
            while (sb + 1 < real_nsb &&
                   select9_count_before<Word, Ones>(superblocks[sb + 1], sb + 1) <= target)
                ++sb;
            hints[k] = sb;
        }
    }

    // Precondition: r < select9_total<Ones>(size, total_ones).
    template <typename Word, pack_endian Endian, bool Ones>
    [[nodiscard]] constexpr std::size_t
    select9_select(const Word* data, const rank9_superblock* superblocks, std::size_t real_nsb,
                   [[maybe_unused]] std::uint64_t size, const std::uint64_t* hints,
                   std::size_t nhints, std::size_t r) noexcept {
        constexpr std::size_t digits = broadword::word_digits<Word>;

        [[maybe_unused]] const std::uint64_t total_ones = superblocks[real_nsb].absolute;
        CDS_ASSERT(r < select9_total<Ones>(size, total_ones),
                   "select9: rank {} out of range (total = {})", r,
                   select9_total<Ones>(size, total_ones));

        const std::size_t k = r / select9_hint_spacing;

        std::size_t lo = static_cast<std::size_t>(hints[k]);
        std::size_t hi = (k + 1 < nhints) ? static_cast<std::size_t>(hints[k + 1]) : (real_nsb - 1);

        while (lo < hi) {
            const std::size_t mid = lo + (hi - lo + 1) / 2;
            if (select9_count_before<Word, Ones>(superblocks[mid], mid) <= r)
                lo = mid;
            else
                hi = mid - 1;
        }
        const std::size_t sb = lo;

        const auto rel =
            r - static_cast<std::size_t>(select9_count_before<Word, Ones>(superblocks[sb], sb));
        const std::uint64_t packed = superblocks[sb].packed_relative;

        std::size_t w = 0;
        std::size_t word_rel = 0;
        for (std::size_t ww = 1; ww <= 7; ++ww) {
            const auto ones_c = static_cast<std::size_t>((packed >> (63 - 9 * ww)) & 0x1FFull);
            const std::size_t c = Ones ? ones_c : (ww * digits - ones_c);
            if (c > rel)
                break;
            w = ww;
            word_rel = c;
        }

        const std::size_t within_word_rank = rel - word_rel;
        const std::size_t word_index = sb * rank9_words_per_superblock + w;

        Word word = data[word_index];
        if constexpr (!Ones)
            word = static_cast<Word>(~word);

        std::size_t bit;
        if constexpr (std::same_as<Word, std::uint64_t>)
            bit = broadword::select_in_word<Endian>(word, within_word_rank);
        else
            bit = broadword::select_in_word<Endian, Word>(word, within_word_rank);

        return sb * rank9_superblock_bits<Word> + w * digits + bit;
    }

    struct select9_header {
        std::uint32_t magic;
        std::uint32_t reserved{0};
        std::uint64_t total_ones;
        std::uint64_t total_zeros;
        std::uint64_t nhints;  // 0 if the select1 side was not built
        std::uint64_t nhints0; // 0 if the select0 side was not built
    };
    static_assert(io::mmap_aligned_header<select9_header>);

    inline constexpr std::uint32_t select9_magic = io::cds_magic(io::format_id::select9);

} // namespace cds::detail

namespace cds {

    template <bit_source Source, select_target Target> class select9_view;

    // select9: owning select index built on an existing rank9, adding only a
    // small per-target hints array. select1 and/or select0 per Target
    //
    // Precondition: the rank9 (and the words of and the source) must
    // outlive this select9.
    template <bit_source Source, select_target Target = select_target::ones> class select9 {
    public:
        using word_type = typename bit_source_traits<Source>::word_type;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;
        static constexpr bool has_ones =
            (Target == select_target::ones || Target == select_target::both);
        static constexpr bool has_zeros =
            (Target == select_target::zeros || Target == select_target::both);

        explicit select9(const rank9<Source>& rank) noexcept
            : m_data(rank.data()), m_size(rank.size()), m_superblocks(rank.superblocks()) {
            const std::size_t real_nsb = m_superblocks.size() - 1;
            if constexpr (has_ones)
                detail::select9_build<word_type, true>(m_superblocks.data(), real_nsb, m_size,
                                                       m_hints);
            if constexpr (has_zeros)
                detail::select9_build<word_type, false>(m_superblocks.data(), real_nsb, m_size,
                                                        m_hints0);
        }

        // Position of the r-th set bit (0-indexed).
        // Precondition: r < number of set bits.
        [[nodiscard]] std::size_t select1(std::size_t r) const noexcept
            requires has_ones
        {
            return detail::select9_select<word_type, endian, true>(
                m_data, m_superblocks.data(), m_superblocks.size() - 1, m_size, m_hints.data(),
                m_hints.size(), r);
        }

        [[nodiscard]] std::size_t select0(std::size_t r) const noexcept
            requires has_zeros
        {
            return detail::select9_select<word_type, endian, false>(
                m_data, m_superblocks.data(), m_superblocks.size() - 1, m_size, m_hints0.data(),
                m_hints0.size(), r);
        }

        [[nodiscard]] std::span<const std::uint64_t> hints() const noexcept
            requires has_ones
        {
            return m_hints;
        }

        [[nodiscard]] std::span<const std::uint64_t> hints0() const noexcept
            requires has_zeros
        {
            return m_hints0;
        }

        [[nodiscard]] select9_view<Source, Target> as_view() const noexcept;

        template <typename Sink> [[nodiscard]] bool save(Sink& sink) const noexcept {
            if (!io::write_cds_version(sink))
                return false;

            detail::select9_header h{};
            h.magic = detail::select9_magic;
            h.total_ones = m_superblocks.back().absolute;
            h.total_zeros = m_size - h.total_ones;
            if constexpr (has_ones)
                h.nhints = m_hints.size();
            if constexpr (has_zeros)
                h.nhints0 = m_hints0.size();

            if (!sink.write(&h, sizeof(h)))
                return false;

            if constexpr (has_ones) {
                if (!m_hints.empty() &&
                    !sink.write(m_hints.data(), m_hints.size() * sizeof(std::uint64_t)))
                    return false;
            }
            if constexpr (has_zeros) {
                if (!m_hints0.empty() &&
                    !sink.write(m_hints0.data(), m_hints0.size() * sizeof(std::uint64_t)))
                    return false;
            }

            return true;
        }

        // TODO: like rank9::load, the format does not record the Word width the
        // hints were built for, so loading against a different word_type
        // misinterprets them.
        template <typename ByteSource>
        [[nodiscard]] static std::expected<select9, io::load_error>
        load(ByteSource& reader, const rank9<Source>& rank) {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::select9_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::select9_magic)
                return std::unexpected(io::load_error::bad_magic);

            const auto superblocks = rank.superblocks();
            const std::uint64_t rank_total_ones =
                superblocks.empty() ? 0 : superblocks.back().absolute;
            const std::uint64_t rank_total_zeros = rank.size() - rank_total_ones;
            if (h.total_ones != rank_total_ones || h.total_zeros != rank_total_zeros)
                return std::unexpected(io::load_error::size_mismatch);

            select9 s(deserialize_tag{}, rank);

            if constexpr (has_ones) {
                if (h.nhints == 0 && h.total_ones != 0)
                    return std::unexpected(io::load_error::type_mismatch);
                s.m_hints.resize(static_cast<std::size_t>(h.nhints));
                if (h.nhints > 0 &&
                    !reader.read(s.m_hints.data(),
                                 static_cast<std::size_t>(h.nhints) * sizeof(std::uint64_t)))
                    return std::unexpected(io::load_error::io_failure);
            } else {
                if (!reader.skip(static_cast<std::size_t>(h.nhints) * sizeof(std::uint64_t)))
                    return std::unexpected(io::load_error::io_failure);
            }

            if constexpr (has_zeros) {
                if (h.nhints0 == 0 && h.total_zeros != 0)
                    return std::unexpected(io::load_error::type_mismatch);
                s.m_hints0.resize(static_cast<std::size_t>(h.nhints0));
                if (h.nhints0 > 0 &&
                    !reader.read(s.m_hints0.data(),
                                 static_cast<std::size_t>(h.nhints0) * sizeof(std::uint64_t)))
                    return std::unexpected(io::load_error::io_failure);
            } else {
                if (!reader.skip(static_cast<std::size_t>(h.nhints0) * sizeof(std::uint64_t)))
                    return std::unexpected(io::load_error::io_failure);
            }

            return s;
        }

        [[nodiscard]] std::size_t memory_size() const noexcept {
            std::size_t bytes = sizeof(*this);
            if constexpr (has_ones)
                bytes += m_hints.size() * sizeof(std::uint64_t);
            if constexpr (has_zeros)
                bytes += m_hints0.size() * sizeof(std::uint64_t);
            return bytes;
        }

    private:
        struct deserialize_tag {};

        select9(deserialize_tag, const rank9<Source>& rank) noexcept
            : m_data(rank.data()), m_size(rank.size()), m_superblocks(rank.superblocks()) {}

        const word_type* m_data;
        std::size_t m_size;
        std::span<const detail::rank9_superblock> m_superblocks;

        CDS_NO_UNIQUE_ADDRESS
        std::conditional_t<has_ones, std::vector<std::uint64_t>, detail::empty_storage> m_hints;
        CDS_NO_UNIQUE_ADDRESS
        std::conditional_t<has_zeros, std::vector<std::uint64_t>, detail::empty_storage> m_hints0;
    };

    // select9_view: non-owning, same Target gating. Specialized
    // per Target so an ones-only view has no hints0 parameter.
    template <bit_source Source, select_target Target = select_target::ones> class select9_view;

    template <bit_source Source> class select9_view<Source, select_target::ones> {
    public:
        using word_type = typename bit_source_traits<Source>::word_type;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;

        select9_view(std::span<const std::uint64_t> hints, const rank9_view<Source>& rank) noexcept
            : m_data(rank.data()), m_size(rank.size()), m_superblocks(rank.superblocks()),
              m_hints(hints) {}

        select9_view(const word_type* data, std::size_t size,
                     std::span<const detail::rank9_superblock> superblocks,
                     std::span<const std::uint64_t> hints) noexcept
            : m_data(data), m_size(size), m_superblocks(superblocks), m_hints(hints) {}

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }
        [[nodiscard]] const word_type* data() const noexcept {
            return m_data;
        }

        [[nodiscard]] std::size_t select1(std::size_t r) const noexcept {
            return detail::select9_select<word_type, endian, true>(
                m_data, m_superblocks.data(), m_superblocks.size() - 1, m_size, m_hints.data(),
                m_hints.size(), r);
        }

        template <typename ByteSource>
            requires io::span_source<ByteSource>
        [[nodiscard]] static std::expected<select9_view, io::load_error>
        load(ByteSource& reader, const rank9_view<Source>& rank) noexcept {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::select9_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::select9_magic)
                return std::unexpected(io::load_error::bad_magic);

            const auto superblocks = rank.superblocks();
            const std::uint64_t rank_total_ones =
                superblocks.empty() ? 0 : superblocks.back().absolute;
            const std::uint64_t rank_total_zeros = rank.size() - rank_total_ones;
            if (h.total_ones != rank_total_ones || h.total_zeros != rank_total_zeros)
                return std::unexpected(io::load_error::size_mismatch);

            if (h.nhints == 0 && h.total_ones != 0)
                return std::unexpected(io::load_error::type_mismatch);

            const std::size_t hints_byte_len =
                static_cast<std::size_t>(h.nhints) * sizeof(std::uint64_t);
            const std::span<const std::byte> hints_bytes = reader.view(hints_byte_len);
            if (hints_bytes.size() != hints_byte_len)
                return std::unexpected(io::load_error::io_failure);

            if (!reader.skip(static_cast<std::size_t>(h.nhints0) * sizeof(std::uint64_t)))
                return std::unexpected(io::load_error::io_failure);

            const std::span<const std::uint64_t> hints(
                reinterpret_cast<const std::uint64_t*>(hints_bytes.data()),
                static_cast<std::size_t>(h.nhints));

            return select9_view(hints, rank);
        }

    private:
        const word_type* m_data;
        std::size_t m_size;
        std::span<const detail::rank9_superblock> m_superblocks;
        std::span<const std::uint64_t> m_hints;
    };

    template <bit_source Source> class select9_view<Source, select_target::zeros> {
    public:
        using word_type = typename bit_source_traits<Source>::word_type;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;

        select9_view(std::span<const std::uint64_t> hints0, const rank9_view<Source>& rank) noexcept
            : m_data(rank.data()), m_size(rank.size()), m_superblocks(rank.superblocks()),
              m_hints0(hints0) {}

        select9_view(const word_type* data, std::size_t size,
                     std::span<const detail::rank9_superblock> superblocks,
                     std::span<const std::uint64_t> hints0) noexcept
            : m_data(data), m_size(size), m_superblocks(superblocks), m_hints0(hints0) {}

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }
        [[nodiscard]] const word_type* data() const noexcept {
            return m_data;
        }

        [[nodiscard]] std::size_t select0(std::size_t r) const noexcept {
            return detail::select9_select<word_type, endian, false>(
                m_data, m_superblocks.data(), m_superblocks.size() - 1, m_size, m_hints0.data(),
                m_hints0.size(), r);
        }

        template <typename ByteSource>
            requires io::span_source<ByteSource>
        [[nodiscard]] static std::expected<select9_view, io::load_error>
        load(ByteSource& reader, const rank9_view<Source>& rank) noexcept {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::select9_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::select9_magic)
                return std::unexpected(io::load_error::bad_magic);

            const auto superblocks = rank.superblocks();
            const std::uint64_t rank_total_ones =
                superblocks.empty() ? 0 : superblocks.back().absolute;
            const std::uint64_t rank_total_zeros = rank.size() - rank_total_ones;
            if (h.total_ones != rank_total_ones || h.total_zeros != rank_total_zeros)
                return std::unexpected(io::load_error::size_mismatch);

            if (!reader.skip(static_cast<std::size_t>(h.nhints) * sizeof(std::uint64_t)))
                return std::unexpected(io::load_error::io_failure);

            if (h.nhints0 == 0 && h.total_zeros != 0)
                return std::unexpected(io::load_error::type_mismatch);

            const std::size_t hints0_byte_len =
                static_cast<std::size_t>(h.nhints0) * sizeof(std::uint64_t);
            const std::span<const std::byte> hints0_bytes = reader.view(hints0_byte_len);
            if (hints0_bytes.size() != hints0_byte_len)
                return std::unexpected(io::load_error::io_failure);

            const std::span<const std::uint64_t> hints0(
                reinterpret_cast<const std::uint64_t*>(hints0_bytes.data()),
                static_cast<std::size_t>(h.nhints0));

            return select9_view(hints0, rank);
        }

    private:
        const word_type* m_data;
        std::size_t m_size;
        std::span<const detail::rank9_superblock> m_superblocks;
        std::span<const std::uint64_t> m_hints0;
    };

    template <bit_source Source> class select9_view<Source, select_target::both> {
    public:
        using word_type = typename bit_source_traits<Source>::word_type;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;

        select9_view(std::span<const std::uint64_t> hints, std::span<const std::uint64_t> hints0,
                     const rank9_view<Source>& rank) noexcept
            : m_data(rank.data()), m_size(rank.size()), m_superblocks(rank.superblocks()),
              m_hints(hints), m_hints0(hints0) {}

        select9_view(const word_type* data, std::size_t size,
                     std::span<const detail::rank9_superblock> superblocks,
                     std::span<const std::uint64_t> hints,
                     std::span<const std::uint64_t> hints0) noexcept
            : m_data(data), m_size(size), m_superblocks(superblocks), m_hints(hints),
              m_hints0(hints0) {}

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }
        [[nodiscard]] const word_type* data() const noexcept {
            return m_data;
        }

        [[nodiscard]] std::size_t select1(std::size_t r) const noexcept {
            return detail::select9_select<word_type, endian, true>(
                m_data, m_superblocks.data(), m_superblocks.size() - 1, m_size, m_hints.data(),
                m_hints.size(), r);
        }

        [[nodiscard]] std::size_t select0(std::size_t r) const noexcept {
            return detail::select9_select<word_type, endian, false>(
                m_data, m_superblocks.data(), m_superblocks.size() - 1, m_size, m_hints0.data(),
                m_hints0.size(), r);
        }

        template <typename ByteSource>
            requires io::span_source<ByteSource>
        [[nodiscard]] static std::expected<select9_view, io::load_error>
        load(ByteSource& reader, const rank9_view<Source>& rank) noexcept {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::select9_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::select9_magic)
                return std::unexpected(io::load_error::bad_magic);

            const auto superblocks = rank.superblocks();
            const std::uint64_t rank_total_ones =
                superblocks.empty() ? 0 : superblocks.back().absolute;
            const std::uint64_t rank_total_zeros = rank.size() - rank_total_ones;
            if (h.total_ones != rank_total_ones || h.total_zeros != rank_total_zeros)
                return std::unexpected(io::load_error::size_mismatch);

            if (h.nhints == 0 && h.total_ones != 0)
                return std::unexpected(io::load_error::type_mismatch);
            if (h.nhints0 == 0 && h.total_zeros != 0)
                return std::unexpected(io::load_error::type_mismatch);

            const std::size_t hints_byte_len =
                static_cast<std::size_t>(h.nhints) * sizeof(std::uint64_t);
            const std::span<const std::byte> hints_bytes = reader.view(hints_byte_len);
            if (hints_bytes.size() != hints_byte_len)
                return std::unexpected(io::load_error::io_failure);

            const std::size_t hints0_byte_len =
                static_cast<std::size_t>(h.nhints0) * sizeof(std::uint64_t);
            const std::span<const std::byte> hints0_bytes = reader.view(hints0_byte_len);
            if (hints0_bytes.size() != hints0_byte_len)
                return std::unexpected(io::load_error::io_failure);

            const std::span<const std::uint64_t> hints(
                reinterpret_cast<const std::uint64_t*>(hints_bytes.data()),
                static_cast<std::size_t>(h.nhints));
            const std::span<const std::uint64_t> hints0(
                reinterpret_cast<const std::uint64_t*>(hints0_bytes.data()),
                static_cast<std::size_t>(h.nhints0));

            return select9_view(hints, hints0, rank);
        }

    private:
        const word_type* m_data;
        std::size_t m_size;
        std::span<const detail::rank9_superblock> m_superblocks;
        std::span<const std::uint64_t> m_hints;
        std::span<const std::uint64_t> m_hints0;
    };

    template <bit_source Source, select_target Target>
    select9_view<Source, Target> select9<Source, Target>::as_view() const noexcept {
        if constexpr (Target == select_target::ones)
            return select9_view<Source, Target>(m_data, m_size, m_superblocks, m_hints);
        else if constexpr (Target == select_target::zeros)
            return select9_view<Source, Target>(m_data, m_size, m_superblocks, m_hints0);
        else
            return select9_view<Source, Target>(m_data, m_size, m_superblocks, m_hints, m_hints0);
    }

} // namespace cds
