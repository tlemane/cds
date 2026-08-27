#pragma once

// Claude & Navarro
// "The Wavelet Matrix", SPIRE 2012.

#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>
#include <vector>

#include <cds/bit_dict.hpp>
#include <cds/bit/vector.hpp>
#include <cds/core/debug.hpp>
#include <cds/io/byte.hpp>

namespace cds::detail {

    struct wavelet_header {
        std::uint32_t magic;
        std::uint32_t reserved{0};
        std::uint64_t size;
        std::uint64_t bits;
    };
    static_assert(io::mmap_aligned_header<wavelet_header>);

    inline constexpr std::uint32_t wavelet_magic = io::cds_magic(io::format_id::wavelet);

    template <typename Derived, typename Symbol> class wavelet_ops {
    public:
        using symbol_type = Symbol;

        // Symbol at position i.
        // Precondition: i < size().
        [[nodiscard]] symbol_type access(std::size_t i) const noexcept {
            const Derived& d = self();
            CDS_ASSERT(i < d.size(), "wavelet_matrix::access: index {} exceeds size {}", i,
                       d.size());
            symbol_type sym = 0;
            const std::size_t nb = d.bits();
            for (std::size_t l = 0; l < nb; ++l) {
                // One rank1 per level, reused for both branches (rank0 = i - r1).
                // When the level fuses rank1 and the bit into a single word load
                // (rank9-backed), take it, else read the bit separately.
                const auto& lv = d.level(l);
                std::size_t r1;
                bool b;
                if constexpr (requires { lv.rank1_bit(i); }) {
                    const rank_bit rb = lv.rank1_bit(i);
                    r1 = rb.rank;
                    b = rb.bit;
                } else {
                    r1 = lv.rank1(i);
                    b = lv[i];
                }
                sym = static_cast<symbol_type>((sym << 1) | (b ? symbol_type{1} : symbol_type{0}));
                i = b ? d.z(l) + r1 : i - r1;
            }
            return sym;
        }

        // Occurrences of symbol c in [0, i).
        // Precondition: i <= size().
        [[nodiscard]] std::size_t rank(symbol_type c, std::size_t i) const noexcept {
            const Derived& d = self();
            CDS_ASSERT(i <= d.size(), "wavelet_matrix::rank: index {} exceeds size {}", i,
                       d.size());
            const std::size_t nb = d.bits();
            std::size_t p = 0;
            for (std::size_t l = 0; l < nb; ++l) {
                // One rank per position, reused for both branches (rank0 = x - r).
                const auto& lv = d.level(l);
                const std::size_t rp = lv.rank1(p);
                const std::size_t ri = lv.rank1(i);
                if (bit_of(c, nb, l)) {
                    const std::size_t z = d.z(l);
                    p = z + rp;
                    i = z + ri;
                } else {
                    p = p - rp;
                    i = i - ri;
                }
            }
            return i - p;
        }

        // Precondition: r < rank(c, size()).
        [[nodiscard]] std::size_t select(symbol_type c, std::size_t r) const noexcept {
            const Derived& d = self();
            const std::size_t nb = d.bits();
            std::size_t p = 0;
            for (std::size_t l = 0; l < nb; ++l) {
                const auto& lv = d.level(l);
                p = bit_of(c, nb, l) ? d.z(l) + lv.rank1(p) : lv.rank0(p);
            }
            std::size_t i = p + r;
            for (std::size_t l = nb; l-- > 0;) {
                const auto& lv = d.level(l);
                i = bit_of(c, nb, l) ? lv.select1(i - d.z(l)) : lv.select0(i);
            }
            return i;
        }

    protected:
        wavelet_ops() = default;

    private:
        [[nodiscard]] const Derived& self() const noexcept {
            return static_cast<const Derived&>(*this);
        }
        [[nodiscard]] static bool bit_of(symbol_type c, std::size_t nb, std::size_t l) noexcept {
            return ((c >> (nb - 1 - l)) & symbol_type{1}) != 0; // MSB first
        }
    };

}

namespace cds {

    template <typename Symbol, std::size_t Bits, typename LevelViewT> class wavelet_matrix_view;

    // wavelet_matrix: static sequence over an integer alphabet with access /
    // rank / select, built as log(alphabet) bit_dictionary levels
    // LevelT models bit_dictionary (rrr for compressed levels).
    // Bits is bits-per-symbol. Bits == 0 infers it at build time.
    template <typename Symbol = std::uint64_t, std::size_t Bits = 0,
              typename LevelT = bit_dict_default<>>
    class wavelet_matrix
        : public detail::wavelet_ops<wavelet_matrix<Symbol, Bits, LevelT>, Symbol> {
    public:
        using symbol_type = Symbol;
        using level_type = LevelT;

        wavelet_matrix() = default;

        explicit wavelet_matrix(std::span<const Symbol> values, std::size_t alphabet_bits = 0) {
            m_size = values.size();
            m_bits = resolve_bits(values, alphabet_bits);

            std::vector<Symbol> cur(values.begin(), values.end());
            std::vector<Symbol> nxt(cur.size());
            m_z.resize(m_bits);
            m_levels.reserve(m_bits);

            for (std::size_t l = 0; l < m_bits; ++l) {
                const std::size_t shift = m_bits - 1 - l; // MSB first
                level_bitvec b;
                b.reserve(m_size);
                std::size_t zeros = 0;
                for (std::size_t i = 0; i < m_size; ++i) {
                    const bool bit = ((cur[i] >> shift) & Symbol{1}) != 0;
                    b.push_back(bit);
                    zeros += bit ? 0u : 1u;
                }
                m_z[l] = zeros;
                m_levels.emplace_back(LevelT(std::move(b)));

                std::size_t z0 = 0, z1 = zeros; // stable partition: zeros then ones
                for (std::size_t i = 0; i < m_size; ++i) {
                    if (((cur[i] >> shift) & Symbol{1}) != 0)
                        nxt[z1++] = cur[i];
                    else
                        nxt[z0++] = cur[i];
                }
                std::swap(cur, nxt);
            }
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }
        [[nodiscard]] std::size_t bits() const noexcept {
            return m_bits;
        }
        [[nodiscard]] std::size_t alphabet_size() const noexcept {
            return (m_bits >= 64) ? 0 : (std::size_t{1} << m_bits);
        }
        [[nodiscard]] const LevelT& level(std::size_t l) const noexcept {
            return m_levels[l];
        }
        [[nodiscard]] std::size_t z(std::size_t l) const noexcept {
            return m_z[l];
        }

        [[nodiscard]] std::size_t memory_size() const noexcept {
            std::size_t bytes = sizeof(*this) + m_z.size() * sizeof(std::size_t);
            for (const auto& lv : m_levels)
                bytes += lv.memory_size() - sizeof(LevelT);
            return bytes;
        }

        template <typename Sink>
            requires io::byte_sink<Sink>
        [[nodiscard]] bool save(Sink& sink) const noexcept {
            if (!io::write_cds_version(sink))
                return false;
            const detail::wavelet_header h{detail::wavelet_magic, 0,
                                           static_cast<std::uint64_t>(m_size),
                                           static_cast<std::uint64_t>(m_bits)};
            if (!sink.write(&h, sizeof(h)))
                return false;
            if (!io::write_vector(sink, m_z))
                return false;
            for (const auto& lv : m_levels)
                if (!lv.save(sink))
                    return false;
            return true;
        }

        template <typename ByteSource>
            requires io::byte_source<ByteSource>
        [[nodiscard]] static std::expected<wavelet_matrix, io::load_error>
        load(ByteSource& reader) {
            const auto ver = io::read_cds_version_compatible(reader);
            if (!ver)
                return std::unexpected(ver.error());
            detail::wavelet_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::wavelet_magic)
                return std::unexpected(io::load_error::bad_magic);
            if constexpr (Bits != 0)
                if (h.bits != Bits)
                    return std::unexpected(io::load_error::type_mismatch);

            auto z = io::read_vector<std::size_t>(reader);
            if (!z)
                return std::unexpected(z.error());

            wavelet_matrix wm;
            wm.m_size = static_cast<std::size_t>(h.size);
            wm.m_bits = static_cast<std::size_t>(h.bits);
            wm.m_z = std::move(*z);
            wm.m_levels.reserve(wm.m_bits);
            for (std::size_t l = 0; l < wm.m_bits; ++l) {
                auto lv = LevelT::load(reader);
                if (!lv)
                    return std::unexpected(lv.error());
                wm.m_levels.emplace_back(std::move(*lv));
            }
            return wm;
        }

        [[nodiscard]] auto as_view() const noexcept {
            using LV = decltype(std::declval<const LevelT&>().as_view());
            std::vector<LV> lv;
            lv.reserve(m_bits);
            for (const auto& l : m_levels)
                lv.push_back(l.as_view());
            return wavelet_matrix_view<Symbol, Bits, LV>(
                std::move(lv), std::span<const std::size_t>(m_z), m_size, m_bits);
        }

    private:
        using level_bitvec = bit_vector<std::uint64_t, pack_endian::lsb>;

        [[nodiscard]] static std::size_t resolve_bits(std::span<const Symbol> values,
                                                      std::size_t alphabet_bits) noexcept {
            if constexpr (Bits != 0) {
                return Bits;
            } else {
                if (alphabet_bits != 0)
                    return alphabet_bits;
                Symbol mx = 0;
                for (Symbol s : values)
                    mx = (s > mx) ? s : mx;
                const std::size_t w =
                    static_cast<std::size_t>(std::bit_width(static_cast<std::uint64_t>(mx)));
                return (w == 0) ? 1 : w;
            }
        }

        std::vector<LevelT> m_levels;
        std::vector<std::size_t> m_z;
        std::size_t m_size = 0;
        std::size_t m_bits = (Bits != 0) ? Bits : 1;
    };

    // wavelet_matrix_view: non-owning
    template <typename Symbol = std::uint64_t, std::size_t Bits = 0,
              typename LevelViewT = bit_dict_view<
                  const_bit_view<std::uint64_t, pack_endian::lsb>,
                  rank9_view<bit_vector<std::uint64_t, pack_endian::lsb>>,
                  darray_view<bit_vector<std::uint64_t, pack_endian::lsb>, select_target::both>>>
    class wavelet_matrix_view
        : public detail::wavelet_ops<wavelet_matrix_view<Symbol, Bits, LevelViewT>, Symbol> {
    public:
        using symbol_type = Symbol;

        wavelet_matrix_view(std::vector<LevelViewT> levels, std::span<const std::size_t> z,
                            std::size_t size, std::size_t bits) noexcept
            : m_levels(std::move(levels)), m_z(z), m_size(size), m_bits(bits) {}

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }
        [[nodiscard]] std::size_t bits() const noexcept {
            return m_bits;
        }
        [[nodiscard]] const LevelViewT& level(std::size_t l) const noexcept {
            return m_levels[l];
        }
        [[nodiscard]] std::size_t z(std::size_t l) const noexcept {
            return m_z[l];
        }

    private:
        std::vector<LevelViewT> m_levels;
        std::span<const std::size_t> m_z;
        std::size_t m_size = 0;
        std::size_t m_bits = 0;
    };

}
