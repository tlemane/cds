#pragma once

// Vigna
// "Broadword Implementation of Rank/Select Queries", WEA 2008.

#include <cstdint>
#include <expected>
#include <span>
#include <utility>
#include <vector>

#include <cds/bit/interface.hpp>
#include <cds/core/debug.hpp>
#include <cds/core/broadword.hpp>
#include <cds/io/byte.hpp>
#include <cds/rank/concepts.hpp>

namespace cds::detail {

    // One index entry per 8-word superblock (512 bits for uint64_t words).
    struct rank9_superblock {
        std::uint64_t absolute;
        std::uint64_t packed_relative;
    };

    inline constexpr std::size_t rank9_words_per_superblock = 8;

    template <typename Word>
    inline constexpr std::size_t rank9_superblock_bits =
        rank9_words_per_superblock * broadword::word_digits<Word>;

    template <typename Word, pack_endian Endian>
    [[nodiscard]] constexpr std::size_t
    rank9_rank1(const Word* data, const rank9_superblock* superblocks, std::size_t i) noexcept {
        constexpr std::size_t digits = broadword::word_digits<Word>;
        constexpr std::size_t sb_bits = rank9_superblock_bits<Word>;

        const std::size_t sb = i / sb_bits;
        const rank9_superblock& s = superblocks[sb];

        const std::size_t pos_in_sb = i % sb_bits;
        const std::size_t w = pos_in_sb / digits;
        const std::size_t bit = pos_in_sb % digits;

        // Branchless read (see rank9_build): w == 0 shifts onto the zero bit.
        const std::size_t relative =
            static_cast<std::size_t>((s.packed_relative >> (63 - 9 * w)) & 0x1FFull);

        std::size_t within = 0;
        if (bit > 0) {
            // bit > 0 and i <= size force word_index < word count, so no bounds check.
            const std::size_t word_index = i / digits;
            if constexpr (std::same_as<Word, std::uint64_t>)
                within = broadword::popcount_prefix<Endian>(data[word_index], bit);
            else
                within = broadword::popcount_below<Endian>(data[word_index], bit);
        }

        return static_cast<std::size_t>(s.absolute) + relative + within;
    }

    // Fused rank1(i) + bit at i from a single load of the word holding bit i.
    // rank1 already reads that word to popcount it, so returning the bit too
    // saves a second dependent load (the hot path in wavelet access).
    // Precondition: i < size (unlike rank1, the word is always dereferenced).
    template <typename Word, pack_endian Endian>
    [[nodiscard]] constexpr rank_bit
    rank9_rank1_bit(const Word* data, const rank9_superblock* superblocks, std::size_t i) noexcept {
        constexpr std::size_t digits = broadword::word_digits<Word>;
        constexpr std::size_t sb_bits = rank9_superblock_bits<Word>;

        const std::size_t sb = i / sb_bits;
        const rank9_superblock& s = superblocks[sb];

        const std::size_t pos_in_sb = i % sb_bits;
        const std::size_t w = pos_in_sb / digits;
        const std::size_t bit = pos_in_sb % digits;

        const std::size_t relative =
            static_cast<std::size_t>((s.packed_relative >> (63 - 9 * w)) & 0x1FFull);

        const std::size_t word_index = i / digits;
        const Word word = data[word_index];
        std::size_t within;
        if constexpr (std::same_as<Word, std::uint64_t>)
            within = broadword::popcount_prefix<Endian>(word, bit);
        else
            within = broadword::popcount_below<Endian>(word, bit);

        const std::size_t shift = (Endian == pack_endian::lsb) ? bit : (digits - 1 - bit);
        const bool b = ((word >> shift) & Word{1}) != 0;

        return {static_cast<std::size_t>(s.absolute) + relative + within, b};
    }

    // Builds the superblock index plus a trailing sentinel entry holding the
    // total popcount, so rank1(size) resolves in-bounds with no special case.
    template <typename Word>
    inline void rank9_build(const Word* data, std::size_t size,
                            std::vector<rank9_superblock>& out) {
        constexpr std::size_t digits = broadword::word_digits<Word>;
        const std::size_t nw = (size + digits - 1) / digits;
        const std::size_t nsb = (nw + rank9_words_per_superblock - 1) / rank9_words_per_superblock;

        out.assign(nsb + 1, rank9_superblock{0, 0});

        std::uint64_t absolute = 0;
        for (std::size_t sb = 0; sb < nsb; ++sb) {
            out[sb].absolute = absolute;

            std::uint64_t packed = 0;
            std::uint64_t block_total = 0;
            for (std::size_t w = 0; w < rank9_words_per_superblock; ++w) {
                if (w > 0)
                    packed |= (block_total & 0x1FFull) << (63 - 9 * w);

                const std::size_t word_index = sb * rank9_words_per_superblock + w;
                if (word_index < nw)
                    block_total += broadword::popcount(data[word_index]);
            }

            out[sb].packed_relative = packed;
            absolute += block_total;
        }

        out[nsb].absolute = absolute; // trailing sentinel = total popcount
    }

    struct rank9_header {
        std::uint32_t magic;
        std::uint32_t reserved{0};
        std::uint64_t size;
        std::uint64_t nsuperblocks; // includes the trailing sentinel
    };
    static_assert(io::mmap_aligned_header<rank9_header>);

    inline constexpr std::uint32_t rank9_magic = 0x396B6E72u;

} // namespace cds::detail

namespace cds {

    template <bit_source Source> class rank9_view;

    // rank9: constant-time rank1/rank0 over any bit_source, at ~25% space.
    //
    // Precondition: the source's words must outlive this rank9 and must not be
    // reallocated while it is alive
    // Sliced/offset bit_view sources are not supported yet.
    template <bit_source Source> class rank9 {
    public:
        using word_type = typename bit_source_traits<Source>::word_type;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;

        explicit rank9(const Source& source) noexcept
            : m_data(bit_source_traits<Source>::data(source)),
              m_size(bit_source_traits<Source>::size(source)) {
            CDS_ASSERT(bit_source_traits<Source>::offset(source) == 0,
                       "rank9: sliced/offset bit_view sources are not supported yet (offset={})",
                       bit_source_traits<Source>::offset(source));
            detail::rank9_build<word_type>(m_data, m_size, m_superblocks);
        }

        [[nodiscard]] std::size_t rank1(std::size_t i) const noexcept {
            CDS_ASSERT(i <= m_size, "rank9::rank1: index {} exceeds size {}", i, m_size);
            return detail::rank9_rank1<word_type, endian>(m_data, m_superblocks.data(), i);
        }

        // rank1(i) and the bit at i from one word load. Precondition: i < size.
        [[nodiscard]] rank_bit rank1_bit(std::size_t i) const noexcept {
            CDS_ASSERT(i < m_size, "rank9::rank1_bit: index {} exceeds size {}", i, m_size);
            return detail::rank9_rank1_bit<word_type, endian>(m_data, m_superblocks.data(), i);
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

        [[nodiscard]] std::span<const detail::rank9_superblock> superblocks() const noexcept {
            return m_superblocks;
        }

        // Non-owning view over this index and the same source words. Valid only
        // while this rank9 (and its source) stay alive.
        [[nodiscard]] rank9_view<Source> as_view() const noexcept;

        template <typename Sink> [[nodiscard]] bool save(Sink& sink) const noexcept {
            if (!io::write_cds_version(sink))
                return false;

            const detail::rank9_header h{detail::rank9_magic, 0, static_cast<std::uint64_t>(m_size),
                                         static_cast<std::uint64_t>(m_superblocks.size())};
            if (!sink.write(&h, sizeof(h)))
                return false;

            if (!m_superblocks.empty() &&
                !sink.write(m_superblocks.data(),
                            m_superblocks.size() * sizeof(detail::rank9_superblock)))
                return false;

            return true;
        }

        // TODO: does not record the Word width it was built with,
        // so loading against a Source with a different word_type silently
        // misinterprets the superblock bit-width.
        template <typename ByteSource>
        [[nodiscard]] static std::expected<rank9, io::load_error> load(ByteSource& reader,
                                                                       const Source& source) {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::rank9_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::rank9_magic)
                return std::unexpected(io::load_error::bad_magic);

            const std::size_t source_size = bit_source_traits<Source>::size(source);
            if (h.size != source_size)
                return std::unexpected(io::load_error::size_mismatch);

            rank9 r(deserialize_tag{}, source);
            r.m_superblocks.resize(static_cast<std::size_t>(h.nsuperblocks));
            if (h.nsuperblocks > 0 &&
                !reader.read(r.m_superblocks.data(), static_cast<std::size_t>(h.nsuperblocks) *
                                                         sizeof(detail::rank9_superblock)))
                return std::unexpected(io::load_error::io_failure);

            return r;
        }

        [[nodiscard]] std::size_t memory_size() const noexcept {
            return sizeof(*this) + m_superblocks.size() * sizeof(detail::rank9_superblock);
        }

    private:
        struct deserialize_tag {};

        rank9(deserialize_tag, const Source& source) noexcept
            : m_data(bit_source_traits<Source>::data(source)),
              m_size(bit_source_traits<Source>::size(source)) {
            CDS_ASSERT(bit_source_traits<Source>::offset(source) == 0,
                       "rank9: sliced/offset bit_view sources are not supported yet (offset={})",
                       bit_source_traits<Source>::offset(source));
            // skips rank9_build; load() fills m_superblocks from the stream
        }

        const word_type* m_data;
        std::size_t m_size;
        std::vector<detail::rank9_superblock> m_superblocks;
    };

    // rank9_view: non-owning. Binds to a pre-built index (e.g. mmap'd, or
    // another rank9's superblocks()) with no build pass and no copy. The caller
    // keeps both the index and the source words valid for the view's lifetime.
    template <bit_source Source> class rank9_view {
    public:
        using word_type = typename bit_source_traits<Source>::word_type;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;

        rank9_view(std::span<const detail::rank9_superblock> superblocks,
                   const Source& source) noexcept
            : m_data(bit_source_traits<Source>::data(source)),
              m_size(bit_source_traits<Source>::size(source)), m_superblocks(superblocks) {
            CDS_ASSERT(
                bit_source_traits<Source>::offset(source) == 0,
                "rank9_view: sliced/offset bit_view sources are not supported yet (offset={})",
                bit_source_traits<Source>::offset(source));
        }

        rank9_view(const word_type* data, std::size_t size,
                   std::span<const detail::rank9_superblock> superblocks) noexcept
            : m_data(data), m_size(size), m_superblocks(superblocks) {}

        template <typename ByteSource>
            requires io::span_source<ByteSource>
        [[nodiscard]] static std::expected<rank9_view, io::load_error>
        load(ByteSource& reader, const Source& source) noexcept {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::rank9_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::rank9_magic)
                return std::unexpected(io::load_error::bad_magic);

            const std::size_t source_size = bit_source_traits<Source>::size(source);
            if (h.size != source_size)
                return std::unexpected(io::load_error::size_mismatch);

            const std::size_t byte_len =
                static_cast<std::size_t>(h.nsuperblocks) * sizeof(detail::rank9_superblock);
            const std::span<const std::byte> bytes = reader.view(byte_len);
            if (bytes.size() != byte_len)
                return std::unexpected(io::load_error::io_failure);

            const auto* superblocks_ptr =
                reinterpret_cast<const detail::rank9_superblock*>(bytes.data());
            const std::span<const detail::rank9_superblock> superblocks(
                superblocks_ptr, static_cast<std::size_t>(h.nsuperblocks));

            return rank9_view(superblocks, source);
        }

        [[nodiscard]] std::size_t rank1(std::size_t i) const noexcept {
            CDS_ASSERT(i <= m_size, "rank9_view::rank1: index {} exceeds size {}", i, m_size);
            return detail::rank9_rank1<word_type, endian>(m_data, m_superblocks.data(), i);
        }

        // rank1(i) and the bit at i from one word load. Precondition: i < size.
        [[nodiscard]] rank_bit rank1_bit(std::size_t i) const noexcept {
            CDS_ASSERT(i < m_size, "rank9_view::rank1_bit: index {} exceeds size {}", i, m_size);
            return detail::rank9_rank1_bit<word_type, endian>(m_data, m_superblocks.data(), i);
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

        [[nodiscard]] std::span<const detail::rank9_superblock> superblocks() const noexcept {
            return m_superblocks;
        }

    private:
        const word_type* m_data;
        std::size_t m_size;
        std::span<const detail::rank9_superblock> m_superblocks;
    };

    template <bit_source Source> rank9_view<Source> rank9<Source>::as_view() const noexcept {
        return rank9_view<Source>(m_data, m_size, m_superblocks);
    }

} // namespace cds
