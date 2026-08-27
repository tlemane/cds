#pragma once

// Okanohara & Sadakane
// "Practical Entropy-Compressed Rank/Select Dictionary", ALENEX 2007.

#include <cds/core/attributes.hpp>
#include <cds/core/debug.hpp>
#include <cds/core/broadword.hpp>
#include <cds/bit/interface.hpp>
#include <cds/io/byte.hpp>
#include <cds/select/concepts.hpp>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>
#include <vector>

namespace cds::detail {

    inline constexpr std::uint64_t darray_dense_threshold = std::uint64_t{1} << 16;

    // One side (ones or zeros).
    // blocks[b] >= 0: dense, holds the block's absolute start position.
    // blocks[b] < 0: sparse, encodes
    // -(overflow index) - 1 (the -1 so overflow index 0 does not look like dense start position 0).
    struct darray_arrays {
        std::vector<std::int64_t> blocks;
        std::vector<std::uint16_t> subblocks;
        std::vector<std::uint64_t> overflows;
        std::uint64_t num_positions = 0;
    };

    // Non-owning counterpart of darray_arrays (spans, not vectors)
    struct darray_arrays_view {
        std::span<const std::int64_t> blocks;
        std::span<const std::uint16_t> subblocks;
        std::span<const std::uint64_t> overflows;
        std::uint64_t num_positions = 0;
    };

    template <std::size_t BlockSize, std::size_t SubblockSize>
    inline void darray_flush_block(std::vector<std::uint64_t>& cur, darray_arrays& out) {
        if (cur.empty())
            return;

        if (cur.back() - cur.front() < darray_dense_threshold) {
            out.blocks.push_back(static_cast<std::int64_t>(cur.front()));
            for (std::size_t i = 0; i < cur.size(); i += SubblockSize)
                out.subblocks.push_back(static_cast<std::uint16_t>(cur[i] - cur.front()));
        } else {
            out.blocks.push_back(-static_cast<std::int64_t>(out.overflows.size()) - 1);
            for (const std::uint64_t p : cur)
                out.overflows.push_back(p);
            for (std::size_t i = 0; i < cur.size(); i += SubblockSize)
                out.subblocks.push_back(0xFFFFu); // placeholder: keeps subblocks globally indexed,
                                                  // never read for a sparse block
        }

        cur.clear();
    }

    template <typename Word, pack_endian Endian, bool Ones, std::size_t BlockSize,
              std::size_t SubblockSize>
    [[nodiscard]] inline darray_arrays darray_build(const Word* data, std::size_t size) {
        constexpr std::size_t digits = broadword::word_digits<Word>;

        darray_arrays out;
        std::vector<std::uint64_t> cur;
        cur.reserve(BlockSize);

        const std::size_t num_words = (size + digits - 1) / digits;

        for (std::size_t word_idx = 0; word_idx < num_words; ++word_idx) {
            Word word = data[word_idx];
            if constexpr (!Ones)
                word = static_cast<Word>(~word);

            const std::size_t base_pos = word_idx * digits;

            while (word != 0) {
                std::size_t l;
                if constexpr (Endian == pack_endian::lsb)
                    l = static_cast<std::size_t>(std::countr_zero(word));
                else
                    l = static_cast<std::size_t>(std::countl_zero(word));

                const std::size_t pos = base_pos + l;
                if (pos >= size)
                    break; // padding bit past size (only the last word), outer loop ends next

                cur.push_back(pos);
                ++out.num_positions;
                if (cur.size() == BlockSize)
                    darray_flush_block<BlockSize, SubblockSize>(cur, out);

                if constexpr (Endian == pack_endian::lsb)
                    word =
                        static_cast<Word>(word & (word - 1)); // clear the lowest set bit, continue
                else
                    word = static_cast<Word>(
                        word &
                        ~(Word{1} << (digits - 1 - l))); // clear the bit just found, continue
            }
        }

        darray_flush_block<BlockSize, SubblockSize>(cur, out); // trailing partial block

        return out;
    }

    // Precondition: r < arr.num_positions. Arrays is darray_arrays (owning) or darray_arrays_view
    template <typename Word, pack_endian Endian, bool Ones, std::size_t BlockSize,
              std::size_t SubblockSize, typename Arrays>
    [[nodiscard]] inline std::size_t darray_select(const Word* data, const Arrays& arr,
                                                   std::size_t r) noexcept {
        constexpr std::size_t digits = broadword::word_digits<Word>;

        CDS_ASSERT(r < arr.num_positions, "darray: rank {} out of range (total = {})", r,
                   arr.num_positions);

        const std::size_t block = r / BlockSize;
        const std::int64_t block_pos = arr.blocks[block];

        if (block_pos < 0) // sparse: exact, no scan
        {
            const auto overflow_pos = static_cast<std::size_t>(-block_pos - 1);
            return static_cast<std::size_t>(arr.overflows[overflow_pos + (r % BlockSize)]);
        }

        // dense
        const std::size_t subblock =
            r / SubblockSize; // global index (kept aligned across blocks by the flush)
        const auto start_pos = static_cast<std::size_t>(block_pos) + arr.subblocks[subblock];
        const std::size_t need = r % SubblockSize;
        if (need == 0)
            return start_pos;

        std::size_t w = start_pos / digits;
        const std::size_t bit_in_word = start_pos % digits;

        auto effective_word = [&](std::size_t word_index) -> Word {
            Word word = data[word_index];
            if constexpr (!Ones)
                word = static_cast<Word>(~word);
            return word;
        };

        // Clear the bits before bit_in_word in the first word, then treat
        // every word uniformly in one loop (one popcount + one branch each).
        Word word = effective_word(w);
        // Cast all-ones to Word BEFORE shifting, `~Word{0}` promotes to a signed int(-1)
        const Word all_ones = static_cast<Word>(~Word{0});
        if constexpr (Endian == pack_endian::lsb)
            word = static_cast<Word>(word & static_cast<Word>(all_ones << bit_in_word));
        else
            word = static_cast<Word>(word & static_cast<Word>(all_ones >> bit_in_word));

        std::size_t remaining = need; // need >= 1 (need == 0 returned above)
        while (true) {
            std::size_t popcnt;
            if constexpr (std::same_as<Word, std::uint64_t>)
                popcnt = broadword::popcount(word);
            else
                popcnt = broadword::popcount<Word>(word);

            if (remaining < popcnt) {
                std::size_t bit;
                if constexpr (std::same_as<Word, std::uint64_t>)
                    bit = broadword::select_in_word<Endian>(word, remaining);
                else
                    bit = broadword::select_in_word<Endian, Word>(word, remaining);
                return w * digits + bit;
            }
            remaining -= popcnt;
            ++w;
            word = effective_word(w);
        }
    }

    struct darray_empty_arrays {};

    struct darray_header {
        std::uint32_t magic;
        std::uint32_t reserved{0};
        std::uint64_t size;
        std::uint64_t block_size;
        std::uint64_t subblock_size;
        std::uint64_t num_positions; // TODO: remove
        std::uint64_t blocks_n;
        std::uint64_t subblocks_n;
        std::uint64_t overflows_n;
    };
    static_assert(io::mmap_aligned_header<darray_header>);

    inline constexpr std::uint32_t darray_magic = 0x64615232u; // "daR2"-ish

    // Bytes of zero padding needed after a side's subblocks (si
    // uint16 entries) to round the whole side up to a multiple of 8 bytes.
    [[nodiscard]] inline constexpr std::size_t darray_side_pad(std::uint64_t si) noexcept {
        const std::size_t si_bytes = static_cast<std::size_t>(si) * sizeof(std::uint16_t);
        return (8 - (si_bytes & 7)) & 7;
    }

    template <typename Sink>
    [[nodiscard]] inline bool darray_write_side(Sink& sink, const darray_arrays& a) noexcept {
        const std::uint64_t bi = a.blocks.size();
        const std::uint64_t si = a.subblocks.size();
        const std::uint64_t op = a.overflows.size();

        if (!sink.write(&a.num_positions, sizeof(a.num_positions)))
            return false;
        if (!sink.write(&bi, sizeof(bi)))
            return false;
        if (!sink.write(&si, sizeof(si)))
            return false;
        if (!sink.write(&op, sizeof(op)))
            return false;

        // overflows (uint64, needs 8-byte alignment) is written BEFORE
        // subblocks (uint16), so an odd subblock count can't misalign
        // it (which would be UB for the zero-copy view's reinterpret_cast).
        if (bi > 0 &&
            !sink.write(a.blocks.data(), static_cast<std::size_t>(bi) * sizeof(std::int64_t)))
            return false;
        if (op > 0 &&
            !sink.write(a.overflows.data(), static_cast<std::size_t>(op) * sizeof(std::uint64_t)))
            return false;
        if (si > 0 &&
            !sink.write(a.subblocks.data(), static_cast<std::size_t>(si) * sizeof(std::uint16_t)))
            return false;

        // Pad the side to a multiple of 8 bytes so the next side (target=both
        // writes two) or section stays aligned: only subblocks (si*2)
        // can leave an odd tail.
        const std::size_t si_pad = darray_side_pad(si);
        if (si_pad > 0) {
            static constexpr unsigned char zeros[8] = {};
            if (!sink.write(zeros, si_pad))
                return false;
        }

        return true;
    }

    template <typename ByteSource>
    [[nodiscard]] inline std::expected<darray_arrays, io::load_error>
    darray_read_side(ByteSource& reader) noexcept {
        darray_arrays a;
        std::uint64_t bi = 0, si = 0, op = 0;
        if (!reader.read(&a.num_positions, sizeof(a.num_positions)))
            return std::unexpected(io::load_error::io_failure);
        if (!reader.read(&bi, sizeof(bi)))
            return std::unexpected(io::load_error::io_failure);
        if (!reader.read(&si, sizeof(si)))
            return std::unexpected(io::load_error::io_failure);
        if (!reader.read(&op, sizeof(op)))
            return std::unexpected(io::load_error::io_failure);

        a.blocks.resize(static_cast<std::size_t>(bi));
        a.overflows.resize(static_cast<std::size_t>(op));
        a.subblocks.resize(static_cast<std::size_t>(si));

        if (bi > 0 &&
            !reader.read(a.blocks.data(), static_cast<std::size_t>(bi) * sizeof(std::int64_t)))
            return std::unexpected(io::load_error::io_failure);
        if (op > 0 &&
            !reader.read(a.overflows.data(), static_cast<std::size_t>(op) * sizeof(std::uint64_t)))
            return std::unexpected(io::load_error::io_failure);
        if (si > 0 &&
            !reader.read(a.subblocks.data(), static_cast<std::size_t>(si) * sizeof(std::uint16_t)))
            return std::unexpected(io::load_error::io_failure);

        if (const std::size_t si_pad = darray_side_pad(si); si_pad > 0) {
            unsigned char scratch[8];
            if (!reader.read(scratch, si_pad))
                return std::unexpected(io::load_error::io_failure);
        }

        return a;
    }

    // Zero-copy version of darray_read_side
    template <typename ByteSource>
        requires io::span_source<ByteSource>
    [[nodiscard]] inline std::expected<darray_arrays_view, io::load_error>
    darray_view_read_side(ByteSource& reader) noexcept {
        std::uint64_t num_positions = 0, bi = 0, si = 0, op = 0;
        if (!reader.read(&num_positions, sizeof(num_positions)))
            return std::unexpected(io::load_error::io_failure);
        if (!reader.read(&bi, sizeof(bi)))
            return std::unexpected(io::load_error::io_failure);
        if (!reader.read(&si, sizeof(si)))
            return std::unexpected(io::load_error::io_failure);
        if (!reader.read(&op, sizeof(op)))
            return std::unexpected(io::load_error::io_failure);

        const std::size_t bi_bytes = static_cast<std::size_t>(bi) * sizeof(std::int64_t);
        const std::span<const std::byte> bi_span = reader.view(bi_bytes);
        if (bi_span.size() != bi_bytes)
            return std::unexpected(io::load_error::io_failure);

        const std::size_t op_bytes = static_cast<std::size_t>(op) * sizeof(std::uint64_t);
        const std::span<const std::byte> op_span = reader.view(op_bytes);
        if (op_span.size() != op_bytes)
            return std::unexpected(io::load_error::io_failure);

        const std::size_t si_bytes = static_cast<std::size_t>(si) * sizeof(std::uint16_t);
        const std::span<const std::byte> si_span = reader.view(si_bytes);
        if (si_span.size() != si_bytes)
            return std::unexpected(io::load_error::io_failure);

        if (const std::size_t si_pad = darray_side_pad(si); si_pad > 0) {
            if (reader.view(si_pad).size() != si_pad)
                return std::unexpected(io::load_error::io_failure);
        }

        return darray_arrays_view{
            std::span<const std::int64_t>(reinterpret_cast<const std::int64_t*>(bi_span.data()),
                                          static_cast<std::size_t>(bi)),
            std::span<const std::uint16_t>(reinterpret_cast<const std::uint16_t*>(si_span.data()),
                                           static_cast<std::size_t>(si)),
            std::span<const std::uint64_t>(reinterpret_cast<const std::uint64_t*>(op_span.data()),
                                           static_cast<std::size_t>(op)),
            num_positions};
    }

    // Advances the reader past one side's section without materializing it,
    // for a view that doesn't want this side.
    template <typename ByteSource>
    [[nodiscard]] inline bool darray_view_skip_side(ByteSource& reader) noexcept {
        std::uint64_t num_positions = 0, bi = 0, si = 0, op = 0;
        if (!reader.read(&num_positions, sizeof(num_positions)))
            return false;
        if (!reader.read(&bi, sizeof(bi)))
            return false;
        if (!reader.read(&si, sizeof(si)))
            return false;
        if (!reader.read(&op, sizeof(op)))
            return false;

        if (!reader.skip(static_cast<std::size_t>(bi) * sizeof(std::int64_t)))
            return false;
        if (!reader.skip(static_cast<std::size_t>(op) * sizeof(std::uint64_t)))
            return false;
        if (!reader.skip(static_cast<std::size_t>(si) * sizeof(std::uint16_t)))
            return false;
        if (const std::size_t si_pad = darray_side_pad(si); si_pad > 0 && !reader.skip(si_pad))
            return false;

        return true;
    }

} // namespace cds::detail

namespace cds {

    template <bit_source Source, select_target Target, std::size_t BlockSize,
              std::size_t SubblockSize>
    class darray_view;

    // darray: owning select index (adaptive dense/sparse), select1 and/or
    // select0 per Target.
    //
    // Reference-holding: dense blocks scan the source's raw words at query
    // time, so it keeps a pointer into Source (same precondition as rank9: the
    // source words must outlive it and must not be reallocated).
    //
    // BlockSize/SubblockSize default to 1024/32.
    template <bit_source Source, select_target Target = select_target::ones,
              std::size_t BlockSize = 1024, std::size_t SubblockSize = 32>
    class darray {
        static_assert(BlockSize % SubblockSize == 0,
                      "darray: BlockSize must be an exact multiple of SubblockSize. "
                      "subblocks is indexed globally (r / SubblockSize) and relies on "
                      "subblocks nesting exactly within blocks, never spanning a block boundary.");

    public:
        using word_type = typename bit_source_traits<Source>::word_type;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;
        static constexpr bool has_ones =
            (Target == select_target::ones || Target == select_target::both);
        static constexpr bool has_zeros =
            (Target == select_target::zeros || Target == select_target::both);

        explicit darray(const Source& source) noexcept
            : m_data(bit_source_traits<Source>::data(source)),
              m_size(bit_source_traits<Source>::size(source)) {
            CDS_ASSERT(bit_source_traits<Source>::offset(source) == 0,
                       "darray: sliced/offset bit_view sources are not supported yet (offset={})",
                       bit_source_traits<Source>::offset(source));
            if constexpr (has_ones)
                m_ones = detail::darray_build<word_type, endian, true, BlockSize, SubblockSize>(
                    m_data, m_size);
            if constexpr (has_zeros)
                m_zeros = detail::darray_build<word_type, endian, false, BlockSize, SubblockSize>(
                    m_data, m_size);
        }

        [[nodiscard]] std::size_t select1(std::size_t r) const noexcept
            requires has_ones
        {
            return detail::darray_select<word_type, endian, true, BlockSize, SubblockSize>(
                m_data, m_ones, r);
        }

        [[nodiscard]] std::size_t select0(std::size_t r) const noexcept
            requires has_zeros
        {
            return detail::darray_select<word_type, endian, false, BlockSize, SubblockSize>(
                m_data, m_zeros, r);
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }
        [[nodiscard]] const word_type* data() const noexcept {
            return m_data;
        }

        [[nodiscard]] detail::darray_arrays_view ones() const noexcept
            requires has_ones
        {
            return {m_ones.blocks, m_ones.subblocks, m_ones.overflows, m_ones.num_positions};
        }

        [[nodiscard]] detail::darray_arrays_view zeros() const noexcept
            requires has_zeros
        {
            return {m_zeros.blocks, m_zeros.subblocks, m_zeros.overflows, m_zeros.num_positions};
        }

        // Non-owning view over this index and the same source words. Valid only
        // while this darray (and its source) stay alive.
        [[nodiscard]] darray_view<Source, Target, BlockSize, SubblockSize> as_view() const noexcept;

        [[nodiscard]] std::size_t memory_size() const noexcept {
            std::size_t bytes = sizeof(*this);
            if constexpr (has_ones) {
                bytes += m_ones.blocks.size() * 8 + m_ones.subblocks.size() * 2 +
                         m_ones.overflows.size() * 8;
            }
            if constexpr (has_zeros) {
                bytes += m_zeros.blocks.size() * 8 + m_zeros.subblocks.size() * 2 +
                         m_zeros.overflows.size() * 8;
            }
            return bytes;
        }

        [[nodiscard]] std::size_t overhead_bits() const noexcept {
            // heap-only, excludes sizeof(*this) (for bits/element metrics)
            return (memory_size() - sizeof(*this)) * 8;
        }

        template <typename Sink> [[nodiscard]] bool save(Sink& sink) const noexcept {
            if (!io::write_cds_version(sink))
                return false;

            detail::darray_header h{};
            h.magic = detail::darray_magic;
            h.size = m_size;
            h.block_size = BlockSize;
            h.subblock_size = SubblockSize;

            if (!sink.write(&h, sizeof(h)))
                return false;

            if constexpr (has_ones) {
                if (!detail::darray_write_side(sink, m_ones))
                    return false;
            } else {
                if (!detail::darray_write_side(sink, detail::darray_arrays{}))
                    return false;
            }

            if constexpr (has_zeros) {
                if (!detail::darray_write_side(sink, m_zeros))
                    return false;
            } else {
                if (!detail::darray_write_side(sink, detail::darray_arrays{}))
                    return false;
            }

            return true;
        }

        // darray's format always writes both sides (even empty ones) regardless
        // of Target, so load() reads both sections and keeps whichever this
        // instantiation wants.
        template <typename ByteSource>
        [[nodiscard]] static std::expected<darray, io::load_error> load(ByteSource& reader,
                                                                        const Source& source) {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::darray_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::darray_magic)
                return std::unexpected(io::load_error::bad_magic);
            if (h.block_size != BlockSize || h.subblock_size != SubblockSize)
                return std::unexpected(io::load_error::type_mismatch);

            const std::size_t source_size = bit_source_traits<Source>::size(source);
            if (h.size != source_size)
                return std::unexpected(io::load_error::size_mismatch);

            darray d(deserialize_tag{}, source);

            auto ones_side = detail::darray_read_side(reader);
            if (!ones_side)
                return std::unexpected(ones_side.error());

            auto zeros_side = detail::darray_read_side(reader);
            if (!zeros_side)
                return std::unexpected(zeros_side.error());

            if constexpr (has_ones)
                d.m_ones = std::move(*ones_side);
            if constexpr (has_zeros)
                d.m_zeros = std::move(*zeros_side);

            return d;
        }

    private:
        struct deserialize_tag {};

        darray(deserialize_tag, const Source& source) noexcept
            : m_data(bit_source_traits<Source>::data(source)),
              m_size(bit_source_traits<Source>::size(source)) {
            CDS_ASSERT(bit_source_traits<Source>::offset(source) == 0,
                       "darray: sliced/offset bit_view sources are not supported yet (offset={})",
                       bit_source_traits<Source>::offset(source));
        }

        const word_type* m_data;
        std::size_t m_size;
        CDS_NO_UNIQUE_ADDRESS
        std::conditional_t<has_ones, detail::darray_arrays, detail::darray_empty_arrays> m_ones;
        CDS_NO_UNIQUE_ADDRESS
        std::conditional_t<has_zeros, detail::darray_arrays, detail::darray_empty_arrays> m_zeros;
    };

    // darray_view: non-owning
    template <bit_source Source, select_target Target = select_target::ones,
              std::size_t BlockSize = 1024, std::size_t SubblockSize = 32>
    class darray_view;

    template <bit_source Source, std::size_t BlockSize, std::size_t SubblockSize>
    class darray_view<Source, select_target::ones, BlockSize, SubblockSize> {
    public:
        using word_type = typename bit_source_traits<Source>::word_type;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;

        darray_view(detail::darray_arrays_view ones, const Source& source) noexcept
            : m_data(bit_source_traits<Source>::data(source)),
              m_size(bit_source_traits<Source>::size(source)), m_ones(ones) {
            CDS_ASSERT(
                bit_source_traits<Source>::offset(source) == 0,
                "darray_view: sliced/offset bit_view sources are not supported yet (offset={})",
                bit_source_traits<Source>::offset(source));
        }

        darray_view(const word_type* data, std::size_t size,
                    detail::darray_arrays_view ones) noexcept
            : m_data(data), m_size(size), m_ones(ones) {}

        [[nodiscard]] std::size_t select1(std::size_t r) const noexcept {
            return detail::darray_select<word_type, endian, true, BlockSize, SubblockSize>(
                m_data, m_ones, r);
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }
        [[nodiscard]] const word_type* data() const noexcept {
            return m_data;
        }

        // reads the ones section, skips the zeros section (darray always writes both).
        template <typename ByteSource>
            requires io::span_source<ByteSource>
        [[nodiscard]] static std::expected<darray_view, io::load_error>
        load(ByteSource& reader, const Source& source) noexcept {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::darray_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::darray_magic)
                return std::unexpected(io::load_error::bad_magic);
            if (h.block_size != BlockSize || h.subblock_size != SubblockSize)
                return std::unexpected(io::load_error::type_mismatch);

            const std::size_t source_size = bit_source_traits<Source>::size(source);
            if (h.size != source_size)
                return std::unexpected(io::load_error::size_mismatch);

            auto ones = detail::darray_view_read_side(reader);
            if (!ones)
                return std::unexpected(ones.error());

            if (!detail::darray_view_skip_side(reader))
                return std::unexpected(io::load_error::io_failure);

            return darray_view(*ones, source);
        }

    private:
        const word_type* m_data;
        std::size_t m_size;
        detail::darray_arrays_view m_ones;
    };

    template <bit_source Source, std::size_t BlockSize, std::size_t SubblockSize>
    class darray_view<Source, select_target::zeros, BlockSize, SubblockSize> {
    public:
        using word_type = typename bit_source_traits<Source>::word_type;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;

        darray_view(detail::darray_arrays_view zeros, const Source& source) noexcept
            : m_data(bit_source_traits<Source>::data(source)),
              m_size(bit_source_traits<Source>::size(source)), m_zeros(zeros) {
            CDS_ASSERT(
                bit_source_traits<Source>::offset(source) == 0,
                "darray_view: sliced/offset bit_view sources are not supported yet (offset={})",
                bit_source_traits<Source>::offset(source));
        }

        darray_view(const word_type* data, std::size_t size,
                    detail::darray_arrays_view zeros) noexcept
            : m_data(data), m_size(size), m_zeros(zeros) {}

        [[nodiscard]] std::size_t select0(std::size_t r) const noexcept {
            return detail::darray_select<word_type, endian, false, BlockSize, SubblockSize>(
                m_data, m_zeros, r);
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }
        [[nodiscard]] const word_type* data() const noexcept {
            return m_data;
        }

        // skips the ones section, then reads the zeros section.
        template <typename ByteSource>
            requires io::span_source<ByteSource>
        [[nodiscard]] static std::expected<darray_view, io::load_error>
        load(ByteSource& reader, const Source& source) noexcept {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::darray_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::darray_magic)
                return std::unexpected(io::load_error::bad_magic);
            if (h.block_size != BlockSize || h.subblock_size != SubblockSize)
                return std::unexpected(io::load_error::type_mismatch);

            const std::size_t source_size = bit_source_traits<Source>::size(source);
            if (h.size != source_size)
                return std::unexpected(io::load_error::size_mismatch);

            if (!detail::darray_view_skip_side(reader))
                return std::unexpected(io::load_error::io_failure);

            auto zeros = detail::darray_view_read_side(reader);
            if (!zeros)
                return std::unexpected(zeros.error());

            return darray_view(*zeros, source);
        }

    private:
        const word_type* m_data;
        std::size_t m_size;
        detail::darray_arrays_view m_zeros;
    };

    template <bit_source Source, std::size_t BlockSize, std::size_t SubblockSize>
    class darray_view<Source, select_target::both, BlockSize, SubblockSize> {
    public:
        using word_type = typename bit_source_traits<Source>::word_type;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;

        darray_view(detail::darray_arrays_view ones, detail::darray_arrays_view zeros,
                    const Source& source) noexcept
            : m_data(bit_source_traits<Source>::data(source)),
              m_size(bit_source_traits<Source>::size(source)), m_ones(ones), m_zeros(zeros) {
            CDS_ASSERT(
                bit_source_traits<Source>::offset(source) == 0,
                "darray_view: sliced/offset bit_view sources are not supported yet (offset={})",
                bit_source_traits<Source>::offset(source));
        }

        darray_view(const word_type* data, std::size_t size, detail::darray_arrays_view ones,
                    detail::darray_arrays_view zeros) noexcept
            : m_data(data), m_size(size), m_ones(ones), m_zeros(zeros) {}

        [[nodiscard]] std::size_t select1(std::size_t r) const noexcept {
            return detail::darray_select<word_type, endian, true, BlockSize, SubblockSize>(
                m_data, m_ones, r);
        }

        [[nodiscard]] std::size_t select0(std::size_t r) const noexcept {
            return detail::darray_select<word_type, endian, false, BlockSize, SubblockSize>(
                m_data, m_zeros, r);
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }
        [[nodiscard]] const word_type* data() const noexcept {
            return m_data;
        }

        template <typename ByteSource>
            requires io::span_source<ByteSource>
        [[nodiscard]] static std::expected<darray_view, io::load_error>
        load(ByteSource& reader, const Source& source) noexcept {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::darray_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::darray_magic)
                return std::unexpected(io::load_error::bad_magic);
            if (h.block_size != BlockSize || h.subblock_size != SubblockSize)
                return std::unexpected(io::load_error::type_mismatch);

            const std::size_t source_size = bit_source_traits<Source>::size(source);
            if (h.size != source_size)
                return std::unexpected(io::load_error::size_mismatch);

            auto ones = detail::darray_view_read_side(reader);
            if (!ones)
                return std::unexpected(ones.error());

            auto zeros = detail::darray_view_read_side(reader);
            if (!zeros)
                return std::unexpected(zeros.error());

            return darray_view(*ones, *zeros, source);
        }

    private:
        const word_type* m_data;
        std::size_t m_size;
        detail::darray_arrays_view m_ones;
        detail::darray_arrays_view m_zeros;
    };

    template <bit_source Source, select_target Target, std::size_t BlockSize,
              std::size_t SubblockSize>
    darray_view<Source, Target, BlockSize, SubblockSize>
    darray<Source, Target, BlockSize, SubblockSize>::as_view() const noexcept {
        if constexpr (Target == select_target::ones)
            return darray_view<Source, Target, BlockSize, SubblockSize>(m_data, m_size, ones());
        else if constexpr (Target == select_target::zeros)
            return darray_view<Source, Target, BlockSize, SubblockSize>(m_data, m_size, zeros());
        else
            return darray_view<Source, Target, BlockSize, SubblockSize>(m_data, m_size, ones(),
                                                                        zeros());
    }

} // namespace cds
