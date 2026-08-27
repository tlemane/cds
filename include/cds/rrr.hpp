#pragma once

// Raman, Raman & Rao
// "Succinct Indexable Dictionaries with Applications to Encoding k-ary Trees
// and Multisets", SODA 2002.

#include <algorithm>
#include <bit>
#include <cstdint>
#include <expected>
#include <utility>
#include <vector>

#include <cds/bit/interface.hpp>
#include <cds/core/debug.hpp>
#include <cds/core/packed/ops.hpp>
#include <cds/io/byte.hpp>
#include <cds/packed/vector.hpp>
#include <cds/packed/view.hpp>

#include <span>

namespace cds::detail {

    template <pack_endian Endian>
    [[nodiscard]] inline bool rrr_test_bit(const std::uint64_t* data, std::size_t i) noexcept {
        const std::size_t w = i / 64;
        const std::size_t b = i % 64;
        if constexpr (Endian == pack_endian::lsb)
            return static_cast<bool>((data[w] >> b) & std::uint64_t{1});
        else
            return static_cast<bool>((data[w] >> (63 - b)) & std::uint64_t{1});
    }

    // Read a block's `len` bits (len <= 63) at logical bit `start` as an
    // lsb-order pattern. lsb sources use a single unaligned word extract.
    // msb falls back to the bit loop. The read never runs past the source's last
    // word: non-final blocks have a next word, and the final partial block
    // satisfies start%64 + len <= 64.
    template <pack_endian SourceEndian>
    [[nodiscard]] inline std::uint64_t rrr_read_block(const std::uint64_t* data, std::size_t start,
                                                      std::size_t len) noexcept {
        if constexpr (SourceEndian == pack_endian::lsb) {
            return bit_ops<std::uint64_t, pack_endian::lsb>::extract(
                data, start, static_cast<std::uint8_t>(len));
        } else {
            std::uint64_t pattern = 0;
            for (std::size_t p = 0; p < len; ++p)
                if (rrr_test_bit<SourceEndian>(data, start + p))
                    pattern |= (std::uint64_t{1} << p);
            return pattern;
        }
    }

    // Binomial coefficient table for a fixed BlockSize
    template <std::size_t BlockSize> class rrr_binomial {
    public:
        rrr_binomial() noexcept {
            for (std::size_t n = 0; n <= BlockSize; ++n) {
                m_c[n][0] = 1;
                for (std::size_t k = 1; k <= n; ++k)
                    m_c[n][k] = m_c[n - 1][k - 1] + ((k <= n - 1) ? m_c[n - 1][k] : 0);
            }
            for (std::size_t k = 0; k <= BlockSize; ++k)
                m_full_width[k] = static_cast<std::uint8_t>(width(BlockSize, k));
        }

        [[nodiscard]] std::uint64_t choose(std::size_t n, std::size_t k) const noexcept {
            if (k > n)
                return 0;
            return m_c[n][k];
        }

        // Bits needed for [0, C(n,k)): 0 when C(n,k) <= 1 (k==0 or k==n, a
        // uniform block with nothing to store).
        [[nodiscard]] std::size_t width(std::size_t n, std::size_t k) const noexcept {
            const std::uint64_t c = choose(n, k);
            if (c <= 1)
                return 0;
            return static_cast<std::size_t>(std::bit_width(c - 1));
        }

        // Precomputed offset width of a FULL block of class k. The hot query
        // walk visits only full blocks (the partial last block is never walked
        // past), so this one lookup replaces a block_len() + choose() +
        // bit_width().
        [[nodiscard]] std::size_t full_width(std::size_t k) const noexcept {
            return m_full_width[k];
        }

    private:
        std::uint64_t m_c[BlockSize + 1][BlockSize + 1] = {};
        std::uint8_t m_full_width[BlockSize + 1] = {};
    };

    template <std::size_t BlockSize>
    [[nodiscard]] inline const rrr_binomial<BlockSize>& rrr_binomial_table() {
        static const rrr_binomial<BlockSize> table;
        return table;
    }

    // Combinatorial rank: bit pattern (len bits, k set) -> index in [0, C(len,k))
    // via rank = sum(C(p_i, i)) over set positions p_1 < ... < p_k (1-indexed i).
    template <std::size_t BlockSize>
    [[nodiscard]] inline std::uint64_t rrr_rank_of_block(std::uint64_t pattern,
                                                         std::size_t len) noexcept {
        const auto& table = rrr_binomial_table<BlockSize>();
        std::uint64_t rank = 0;
        std::size_t seen = 0;
        for (std::size_t p = 0; p < len; ++p) {
            if ((pattern >> p) & std::uint64_t{1}) {
                ++seen;
                rank += table.choose(p, seen);
            }
        }
        return rank;
    }

    // Inverse: combinatorial index -> block bit pattern
    template <std::size_t BlockSize>
    [[nodiscard]] inline std::uint64_t rrr_unrank_to_block(std::uint64_t rank, std::size_t len,
                                                           std::size_t k) noexcept {
        const auto& table = rrr_binomial_table<BlockSize>();
        std::uint64_t block = 0;
        std::uint64_t n = rank;

        for (std::size_t i = k; i >= 1; --i) {
            std::size_t c = len - 1;
            while (c >= i && table.choose(c, i) > n)
                --c;
            block |= (std::uint64_t{1} << c);
            n -= table.choose(c, i);
            if (i == 1)
                break; // avoid unsigned underflow on the loop decrement
        }
        return block;
    }

    template <std::size_t BlockSize> [[nodiscard]] constexpr std::uint8_t rrr_bt_width() noexcept {
        return static_cast<std::uint8_t>(std::bit_width(static_cast<std::uint64_t>(BlockSize)));
    }

    inline constexpr pack_endian rrr_internal_endian = pack_endian::lsb;

    inline constexpr std::uint32_t rrr_magic = 0x72727231u; // "rrr1"

    struct rrr_header {
        std::uint32_t magic;
        std::uint32_t reserved;
        std::uint64_t size;
        std::uint64_t block_size;
        std::uint64_t sample_rate;
        std::uint64_t nblocks;
        std::uint64_t nsamples;
        std::uint64_t offsets_words;
        std::uint64_t rank_width;
        std::uint64_t offset_pos_width;
    };
    static_assert(io::mmap_aligned_header<rrr_header>);

    // Partial-decode helpers: answer a per-block query
    //
    // Enumeration invariant: after the i-th step, `c` is the position of the
    // i-th one (1-indexed from the top), strictly decreasing as i falls, so `c`
    // is threaded across steps with no reset.
    template <std::size_t BlockSize>
    [[nodiscard]] inline bool rrr_decode_bit(std::uint64_t nr, std::size_t len, std::size_t k,
                                             std::size_t pos) noexcept {
        if (k == 0)
            return false;
        if (k == len)
            return true;
        const auto& t = rrr_binomial_table<BlockSize>();
        std::uint64_t n = nr;
        std::size_t c = len;
        for (std::size_t i = k; i >= 1; --i) {
            do {
                --c;
            } while (t.choose(c, i) > n);
            if (c == pos)
                return true;
            if (c < pos)
                return false; // positions only shrink from here
            n -= t.choose(c, i);
            if (i == 1)
                break;
        }
        return false;
    }

    template <std::size_t BlockSize>
    [[nodiscard]] inline std::size_t rrr_decode_prefix_rank(std::uint64_t nr, std::size_t len,
                                                            std::size_t k,
                                                            std::size_t off) noexcept {
        if (k == 0)
            return 0;
        if (k == len)
            return off;
        const auto& t = rrr_binomial_table<BlockSize>();
        std::uint64_t n = nr;
        std::size_t c = len;
        std::size_t hi = 0; // ones at position >= off
        for (std::size_t i = k; i >= 1; --i) {
            do {
                --c;
            } while (t.choose(c, i) > n);
            if (c < off)
                break; // this one and all lower ones are < off
            ++hi;
            n -= t.choose(c, i);
            if (i == 1)
                break;
        }
        return k - hi; // ones in [0, off)
    }

    // Position of the `need`-th (0-indexed, from the low end) one in the block.
    // Precondition: need < k.
    template <std::size_t BlockSize>
    [[nodiscard]] inline std::size_t rrr_decode_select(std::uint64_t nr, std::size_t len,
                                                       std::size_t k, std::size_t need) noexcept {
        if (k == len)
            return need; // all ones
        const auto& t = rrr_binomial_table<BlockSize>();
        // High-to-low enumeration yields the (k-1)-th, (k-2)-th, ... one, so the
        // need-th one from the low end is the (k-1-need)-th enumerated.
        const std::size_t target = k - 1 - need;
        std::uint64_t n = nr;
        std::size_t c = len;
        for (std::size_t idx = 0, i = k; i >= 1; --i, ++idx) {
            do {
                --c;
            } while (t.choose(c, i) > n);
            if (idx == target)
                return c;
            n -= t.choose(c, i);
            if (i == 1)
                break;
        }
        return len; // unreachable when need < k
    }

    template <typename Derived, std::size_t BlockSize, std::size_t SampleRate> class rrr_ops {
    public:
        [[nodiscard]] bool operator[](std::size_t i) const noexcept {
            const std::size_t b = i / BlockSize;
            const std::size_t p = i % BlockSize;
            const std::size_t k = derived().bt(b);
            const std::size_t len = block_len(b);
            if (k == 0)
                return false;
            if (k == len)
                return true;
            const auto& tbl = rrr_binomial_table<BlockSize>();
            const std::uint64_t obits = offset_bits_of_block(b);
            const std::uint64_t nr =
                derived().offset_extract(obits, static_cast<std::uint8_t>(tbl.width(len, k)));
            return rrr_decode_bit<BlockSize>(nr, len, k, p);
        }

        [[nodiscard]] bool at(std::size_t i) const noexcept {
            return (*this)[i];
        }

        [[nodiscard]] std::size_t rank1(std::size_t i) const noexcept {
            const auto& tbl = rrr_binomial_table<BlockSize>();
            const std::size_t b = i / BlockSize;
            const std::size_t off = i % BlockSize;
            const std::size_t s = b / SampleRate;

            std::uint64_t rank = derived().sample_rank(s);
            std::uint64_t obits = derived().sample_offset(s);
            for (std::size_t j = s * SampleRate; j < b; ++j) // walk visits full blocks only
            {
                const std::size_t kj = derived().bt(j);
                rank += kj;
                obits += tbl.full_width(kj);
            }

            if (off == 0)
                return static_cast<std::size_t>(rank);

            const std::size_t k = derived().bt(b);
            const std::size_t len = block_len(b);
            if (k == 0)
                return static_cast<std::size_t>(rank);
            if (k == len)
                return static_cast<std::size_t>(rank) + off;

            const std::uint64_t nr =
                derived().offset_extract(obits, static_cast<std::uint8_t>(tbl.width(len, k)));
            return static_cast<std::size_t>(rank) +
                   rrr_decode_prefix_rank<BlockSize>(nr, len, k, off);
        }

        [[nodiscard]] std::size_t rank0(std::size_t i) const noexcept {
            return i - rank1(i);
        }

        // Position of the r-th (0-indexed) one. Precondition: r < total ones.
        [[nodiscard]] std::size_t select1(std::size_t r) const noexcept {
            const auto& tbl = rrr_binomial_table<BlockSize>();

            // Rightmost sample group whose cumulative rank1 is <= r.
            // Branchless (the ternary lowers to a cmov): branch here
            // mispredicts almost every step and dominated select's cost.
            std::size_t lo = 0;
            std::size_t len = derived().nsamples();
            while (len > 1) {
                const std::size_t half = len / 2;
                lo += (derived().sample_rank(lo + half) <= r) ? half : 0;
                len -= half;
            }

            std::uint64_t rank = derived().sample_rank(lo);
            std::uint64_t obits = derived().sample_offset(lo);
            for (std::size_t b = lo * SampleRate;; ++b) {
                const std::size_t k = derived().bt(b);
                if (rank + k > r) // found block (the only one that may be partial)
                {
                    const std::size_t blen = block_len(b);
                    const std::size_t need = static_cast<std::size_t>(r - rank);
                    if (k == blen)
                        return b * BlockSize + need; // uniform all-ones
                    const std::uint64_t nr = derived().offset_extract(
                        obits, static_cast<std::uint8_t>(tbl.width(blen, k)));
                    return b * BlockSize + rrr_decode_select<BlockSize>(nr, blen, k, need);
                }
                rank += k;
                obits += tbl.full_width(k); // skipped block is full
            }
        }

        // Position of the r-th (0-indexed) zero. Precondition: r < total zeros.
        // rank0 at sample group s is derived from the stored rank1 sample: a
        // group start is at bit s*SampleRate*BlockSize (block starts are exact
        // BlockSize multiples), so rank0 = that - sample_rank(s).
        [[nodiscard]] std::size_t select0(std::size_t r) const noexcept {
            const auto& tbl = rrr_binomial_table<BlockSize>();
            auto rank0_at = [&](std::size_t s) -> std::uint64_t {
                return static_cast<std::uint64_t>(s) * SampleRate * BlockSize -
                       derived().sample_rank(s);
            };

            std::size_t lo = 0;
            std::size_t len = derived().nsamples();
            while (len > 1) {
                const std::size_t half = len / 2;
                lo += (rank0_at(lo + half) <= r) ? half : 0;
                len -= half;
            }

            std::uint64_t zeros = rank0_at(lo);
            std::uint64_t obits = derived().sample_offset(lo);
            for (std::size_t b = lo * SampleRate;; ++b) {
                const std::size_t k = derived().bt(b);
                // blen is BlockSize except for the vector's final (partial) block.
                const std::size_t blen = (b + 1 < derived().nblocks()) ? BlockSize : block_len(b);
                const std::size_t z = blen - k;
                if (zeros + z > r) {
                    const std::size_t need = static_cast<std::size_t>(r - zeros);
                    if (k == 0)
                        return b * BlockSize + need; // uniform all-zeros
                    const std::uint64_t nr = derived().offset_extract(
                        obits, static_cast<std::uint8_t>(tbl.width(blen, k)));
                    const std::uint64_t pattern = rrr_unrank_to_block<BlockSize>(nr, blen, k);
                    std::size_t seen = 0;
                    for (std::size_t p = 0; p < blen; ++p)
                        if (!((pattern >> p) & std::uint64_t{1})) {
                            if (seen == need)
                                return b * BlockSize + p;
                            ++seen;
                        }
                }
                zeros += z;
                obits += tbl.full_width(k); // advancing past a full block
            }
        }

    protected:
        [[nodiscard]] std::size_t block_len(std::size_t b) const noexcept {
            const std::size_t start = b * BlockSize;
            return std::min(BlockSize, derived().size() - start);
        }

        // Offset-bit position of block b's offset, walking from its sample group.
        [[nodiscard]] std::uint64_t offset_bits_of_block(std::size_t b) const noexcept {
            const auto& tbl = rrr_binomial_table<BlockSize>();
            const std::size_t s = b / SampleRate;
            std::uint64_t obits = derived().sample_offset(s);
            for (std::size_t j = s * SampleRate; j < b; ++j) // walk visits full blocks only
                obits += tbl.full_width(derived().bt(j));
            return obits;
        }

        [[nodiscard]] const Derived& derived() const noexcept {
            return static_cast<const Derived&>(*this);
        }
    };

} // namespace cds::detail

namespace cds {

    // rrr: owning, self-contained RRR-compressed bitmap with rank1/rank0/
    // select1/select0 and operator[]. Built from any bit_source
    // It never touches the source after construction so it has no lifetime coupling to it.
    // BlockSize <= 63 so every C(len,k) fits in uint64_t.
    template <std::size_t BlockSize, std::size_t SampleRate> class rrr_view;

    template <std::size_t BlockSize = 63, std::size_t SampleRate = 32>
    class rrr : public detail::rrr_ops<rrr<BlockSize, SampleRate>, BlockSize, SampleRate> {
        static_assert(BlockSize >= 1 && BlockSize <= 63,
                      "rrr: BlockSize must be in [1, 63] so combinatorial ranks fit in uint64_t");
        static_assert(SampleRate >= 1, "rrr: SampleRate must be >= 1");

        static constexpr std::uint8_t bt_width = detail::rrr_bt_width<BlockSize>();
        static constexpr pack_endian E = detail::rrr_internal_endian;

        using bt_vector_type =
            packed_vector<std::uint64_t, std::uint64_t, bt_width, E, pack_mode::dense>;
        using sample_vector_type =
            packed_vector<std::uint64_t, std::uint64_t, 0, E, pack_mode::dense>;
        using bops = bit_ops<std::uint64_t, E>;

    public:
        rrr() = default;

        template <bit_source Source> explicit rrr(const Source& source) {
            build<bit_source_traits<Source>::endian>(bit_source_traits<Source>::data(source),
                                                     bit_source_traits<Source>::size(source));
        }

        [[nodiscard]] rrr_view<BlockSize, SampleRate> as_view() const noexcept;

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }
        [[nodiscard]] std::size_t nblocks() const noexcept {
            return m_nblocks;
        }
        [[nodiscard]] std::size_t nsamples() const noexcept {
            return m_sample_rank.size();
        }
        [[nodiscard]] std::size_t bt(std::size_t j) const noexcept {
            return static_cast<std::size_t>(m_bt[j]);
        }
        [[nodiscard]] std::uint64_t sample_rank(std::size_t s) const noexcept {
            return static_cast<std::uint64_t>(m_sample_rank[s]);
        }
        [[nodiscard]] std::uint64_t sample_offset(std::size_t s) const noexcept {
            return static_cast<std::uint64_t>(m_sample_offset[s]);
        }
        [[nodiscard]] std::uint64_t offset_extract(std::uint64_t pos,
                                                   std::uint8_t w) const noexcept {
            return w == 0 ? 0 : bops::extract(m_offsets.data(), pos, w);
        }

        [[nodiscard]] std::size_t memory_size() const noexcept {
            return sizeof(*this) + m_bt.nb_words() * sizeof(std::uint64_t) +
                   m_sample_rank.nb_words() * sizeof(std::uint64_t) +
                   m_sample_offset.nb_words() * sizeof(std::uint64_t) +
                   m_offsets.size() * sizeof(std::uint64_t);
        }
        [[nodiscard]] std::size_t overhead_bits() const noexcept {
            return (memory_size() - sizeof(*this)) * 8;
        }

        template <typename Sink>
            requires io::byte_sink<Sink>
        [[nodiscard]] bool save(Sink& sink) const noexcept {
            if (!io::write_cds_version(sink))
                return false;
            const detail::rrr_header h{detail::rrr_magic,
                                       0,
                                       static_cast<std::uint64_t>(m_size),
                                       BlockSize,
                                       SampleRate,
                                       static_cast<std::uint64_t>(m_nblocks),
                                       static_cast<std::uint64_t>(m_sample_rank.size()),
                                       static_cast<std::uint64_t>(m_offsets.size()),
                                       static_cast<std::uint64_t>(m_rank_width),
                                       static_cast<std::uint64_t>(m_offset_pos_width)};
            if (!sink.write(&h, sizeof(h)))
                return false;
            if (!(m_bt.save(sink) && m_sample_rank.save(sink) && m_sample_offset.save(sink)))
                return false;
            if (!m_offsets.empty() &&
                !sink.write(m_offsets.data(), m_offsets.size() * sizeof(std::uint64_t)))
                return false;
            return true;
        }

        template <typename Source>
            requires io::byte_source<Source>
        [[nodiscard]] static std::expected<rrr, io::load_error> load(Source& source) {
            const auto version = io::read_cds_version_compatible(source);
            if (!version)
                return std::unexpected(version.error());

            detail::rrr_header h{};
            if (!source.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::rrr_magic)
                return std::unexpected(io::load_error::bad_magic);
            if (h.block_size != BlockSize || h.sample_rate != SampleRate)
                return std::unexpected(io::load_error::type_mismatch);

            rrr r;
            r.m_size = static_cast<std::size_t>(h.size);
            r.m_nblocks = static_cast<std::size_t>(h.nblocks);
            r.m_rank_width = static_cast<std::uint8_t>(h.rank_width);
            r.m_offset_pos_width = static_cast<std::uint8_t>(h.offset_pos_width);

            auto bt = bt_vector_type::load(source);
            if (!bt)
                return std::unexpected(bt.error());
            r.m_bt = std::move(*bt);
            auto sr = sample_vector_type::load(source);
            if (!sr)
                return std::unexpected(sr.error());
            r.m_sample_rank = std::move(*sr);
            auto so = sample_vector_type::load(source);
            if (!so)
                return std::unexpected(so.error());
            r.m_sample_offset = std::move(*so);

            r.m_offsets.resize(static_cast<std::size_t>(h.offsets_words));
            if (h.offsets_words > 0 &&
                !source.read(r.m_offsets.data(), r.m_offsets.size() * sizeof(std::uint64_t)))
                return std::unexpected(io::load_error::io_failure);

            return r;
        }

    private:
        template <pack_endian SourceEndian>
        void build(const std::uint64_t* data, std::size_t size) {
            m_size = size;
            m_nblocks = (m_size + BlockSize - 1) / BlockSize;
            const auto& table = detail::rrr_binomial_table<BlockSize>();

            // Pass 1: popcounts + totals, to size the runtime-width sample
            // vectors and the offsets bit-buffer before any writes.
            std::vector<std::size_t> popcounts(m_nblocks);
            std::uint64_t total_offset_bits = 0;
            std::uint64_t total_rank = 0;
            for (std::size_t b = 0; b < m_nblocks; ++b) {
                const std::size_t start = b * BlockSize;
                const std::size_t len = std::min(BlockSize, m_size - start);
                const std::uint64_t pattern =
                    detail::rrr_read_block<SourceEndian>(data, start, len);
                const auto k = static_cast<std::size_t>(std::popcount(pattern));
                popcounts[b] = k;
                total_offset_bits += table.width(len, k);
                total_rank += k;
            }

            m_rank_width = static_cast<std::uint8_t>(
                std::max<std::size_t>(1, static_cast<std::size_t>(std::bit_width(total_rank))));
            m_offset_pos_width = static_cast<std::uint8_t>(std::max<std::size_t>(
                1, static_cast<std::size_t>(std::bit_width(total_offset_bits))));

            m_bt = bt_vector_type(bt_width, E, pack_mode::dense);
            m_sample_rank = sample_vector_type(m_rank_width, E, pack_mode::dense);
            m_sample_offset = sample_vector_type(m_offset_pos_width, E, pack_mode::dense);
            m_offsets.assign(total_offset_bits / 64 + 2, 0);

            // Reserve EXACT counts before the push loop: push_back grows
            // geometrically, so without this the arrays keep up to ~2x slack,
            // inflating the real footprint by ~0.3 bits/bit.
            const std::size_t nsamp = m_nblocks / SampleRate + 2;
            m_bt.reserve(m_nblocks);
            m_sample_rank.reserve(nsamp);
            m_sample_offset.reserve(nsamp);

            // Pass 2: encode block types + combinadic offsets, sampling every
            // SampleRate-th block.
            std::uint64_t running_rank = 0;
            std::uint64_t offset_cursor = 0;
            for (std::size_t b = 0; b < m_nblocks; ++b) {
                if (b % SampleRate == 0) {
                    m_sample_rank.push_back(running_rank);
                    m_sample_offset.push_back(offset_cursor);
                }

                const std::size_t start = b * BlockSize;
                const std::size_t len = std::min(BlockSize, m_size - start);
                const std::size_t k = popcounts[b];
                m_bt.push_back(k);

                const std::size_t w = table.width(len, k);
                if (w > 0) {
                    const std::uint64_t pattern =
                        detail::rrr_read_block<SourceEndian>(data, start, len);
                    const std::uint64_t rank_val =
                        detail::rrr_rank_of_block<BlockSize>(pattern, len);
                    bops::insert(m_offsets.data(), offset_cursor, static_cast<std::uint8_t>(w),
                                 rank_val);
                }
                offset_cursor += w;
                running_rank += k;
            }
            m_sample_rank.push_back(running_rank);    // trailing sentinel
            m_sample_offset.push_back(offset_cursor); // trailing sentinel
        }

        std::size_t m_size = 0;
        std::size_t m_nblocks = 0;
        std::uint8_t m_rank_width = 1;
        std::uint8_t m_offset_pos_width = 1;
        bt_vector_type m_bt;
        sample_vector_type m_sample_rank;
        sample_vector_type m_sample_offset;
        std::vector<std::uint64_t> m_offsets;
    };

    // rrr_view: non-owning, zero-copy counterpart of rrr.
    template <std::size_t BlockSize = 63, std::size_t SampleRate = 32>
    class rrr_view
        : public detail::rrr_ops<rrr_view<BlockSize, SampleRate>, BlockSize, SampleRate> {
        static_assert(BlockSize >= 1 && BlockSize <= 63);
        static_assert(SampleRate >= 1);

        static constexpr std::uint8_t bt_width = detail::rrr_bt_width<BlockSize>();
        static constexpr pack_endian E = detail::rrr_internal_endian;

        using bt_view_type =
            const_packed_view<std::uint64_t, std::uint64_t, bt_width, E, pack_mode::dense>;
        using sample_view_type =
            const_packed_view<std::uint64_t, std::uint64_t, 0, E, pack_mode::dense>;
        using bops = bit_ops<std::uint64_t, E>;

    public:
        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }
        [[nodiscard]] std::size_t nblocks() const noexcept {
            return m_nblocks;
        }
        [[nodiscard]] std::size_t nsamples() const noexcept {
            return m_nsamples;
        }
        [[nodiscard]] std::size_t bt(std::size_t j) const noexcept {
            return static_cast<std::size_t>(m_bt[j]);
        }
        [[nodiscard]] std::uint64_t sample_rank(std::size_t s) const noexcept {
            return static_cast<std::uint64_t>(m_sample_rank[s]);
        }
        [[nodiscard]] std::uint64_t sample_offset(std::size_t s) const noexcept {
            return static_cast<std::uint64_t>(m_sample_offset[s]);
        }
        [[nodiscard]] std::uint64_t offset_extract(std::uint64_t pos,
                                                   std::uint8_t w) const noexcept {
            return w == 0 ? 0 : bops::extract(m_offsets.data(), pos, w);
        }

        template <typename ByteSource>
            requires io::span_source<ByteSource>
        [[nodiscard]] static std::expected<rrr_view, io::load_error> load(ByteSource& reader) {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::rrr_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::rrr_magic)
                return std::unexpected(io::load_error::bad_magic);
            if (h.block_size != BlockSize || h.sample_rate != SampleRate)
                return std::unexpected(io::load_error::type_mismatch);

            auto bt = bt_view_type::load(reader);
            if (!bt)
                return std::unexpected(bt.error());
            auto sr = sample_view_type::load(reader);
            if (!sr)
                return std::unexpected(sr.error());
            auto so = sample_view_type::load(reader);
            if (!so)
                return std::unexpected(so.error());

            const std::size_t offsets_words = static_cast<std::size_t>(h.offsets_words);
            const std::size_t nbytes = offsets_words * sizeof(std::uint64_t);
            const std::span<const std::byte> raw = reader.view(nbytes);
            if (raw.size() != nbytes)
                return std::unexpected(io::load_error::io_failure);
            const std::span<const std::uint64_t> offsets(
                reinterpret_cast<const std::uint64_t*>(raw.data()), offsets_words);

            return rrr_view(std::move(*bt), std::move(*sr), std::move(*so), offsets,
                            static_cast<std::size_t>(h.size), static_cast<std::size_t>(h.nblocks),
                            static_cast<std::size_t>(h.nsamples));
        }

    private:
        template <std::size_t, std::size_t> friend class rrr;

        rrr_view(bt_view_type bt, sample_view_type sr, sample_view_type so,
                 std::span<const std::uint64_t> offsets, std::size_t size, std::size_t nblocks,
                 std::size_t nsamples) noexcept
            : m_bt(bt), m_sample_rank(sr), m_sample_offset(so), m_offsets(offsets), m_size(size),
              m_nblocks(nblocks), m_nsamples(nsamples) {}

        bt_view_type m_bt;
        sample_view_type m_sample_rank;
        sample_view_type m_sample_offset;
        std::span<const std::uint64_t> m_offsets;
        std::size_t m_size = 0;
        std::size_t m_nblocks = 0;
        std::size_t m_nsamples = 0;
    };

    template <std::size_t BlockSize, std::size_t SampleRate>
    rrr_view<BlockSize, SampleRate> rrr<BlockSize, SampleRate>::as_view() const noexcept {
        return rrr_view<BlockSize, SampleRate>(m_bt.as_const_view(), m_sample_rank.as_const_view(),
                                               m_sample_offset.as_const_view(), m_offsets, m_size,
                                               m_nblocks, m_sample_rank.size());
    }

} // namespace cds
