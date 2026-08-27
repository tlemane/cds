#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <utility>

#include <cds/bit/vector.hpp>
#include <cds/rank/rank9.hpp>
#include <cds/select/darray.hpp>
#include <cds/io/byte.hpp>

namespace cds {

    // A static bit sequence answering access, rank, and select on both sides.
    template <typename D>
    concept bit_dictionary = requires(const D& d, std::size_t i, std::size_t r) {
        { d.size() } -> std::convertible_to<std::size_t>;
        { d[i] } -> std::convertible_to<bool>;
        { d.rank1(i) } -> std::convertible_to<std::size_t>;
        { d.rank0(i) } -> std::convertible_to<std::size_t>;
        { d.select1(r) } -> std::convertible_to<std::size_t>;
        { d.select0(r) } -> std::convertible_to<std::size_t>;
    };

    template <typename BitVecView, typename RankView, typename SelectView> class bit_dict_view;

    // bit_dict: an owning bit vector bundled with rank + select over its own
    // bits (models bit_dictionary). Same ownership rule as rank_backed_select:
    // copy rebuilds the rank/select over the copied bits. Move is fine.
    //
    // The select shares the rank index when it layers on it (select9 over
    // rank9, select_poppy over rank_poppy): one index answers both rank and
    // select, so the dict stores just the rank plus the select small samples.
    // A standalone select (darray) is built from the bits instead.
    template <typename BitVec, typename RankT, typename SelectT> class bit_dict {
    public:
        using bitvec_type = BitVec;

        bit_dict() : m_bits(), m_rank(m_bits), m_select(make_select(m_bits, m_rank)) {}

        explicit bit_dict(BitVec bits)
            : m_bits(std::move(bits)), m_rank(m_bits), m_select(make_select(m_bits, m_rank)) {}

        bit_dict(const bit_dict& o)
            : m_bits(o.m_bits), m_rank(m_bits), m_select(make_select(m_bits, m_rank)) {}
        bit_dict(bit_dict&& o) noexcept
            : m_bits(std::move(o.m_bits)), m_rank(std::move(o.m_rank)),
              m_select(std::move(o.m_select)) {}
        bit_dict& operator=(const bit_dict& o) {
            if (this != &o) {
                m_bits = o.m_bits;
                m_rank = RankT(m_bits);
                m_select = make_select(m_bits, m_rank);
            }
            return *this;
        }
        bit_dict& operator=(bit_dict&& o) noexcept {
            m_bits = std::move(o.m_bits);
            m_rank = std::move(o.m_rank);
            m_select = std::move(o.m_select);
            return *this;
        }
        ~bit_dict() = default;

        [[nodiscard]] std::size_t size() const noexcept {
            return m_bits.size();
        }
        [[nodiscard]] bool operator[](std::size_t i) const noexcept {
            return static_cast<bool>(m_bits[i]);
        }
        [[nodiscard]] std::size_t rank1(std::size_t i) const noexcept {
            return m_rank.rank1(i);
        }
        [[nodiscard]] std::size_t rank0(std::size_t i) const noexcept {
            return i - m_rank.rank1(i);
        }
        [[nodiscard]] std::size_t select1(std::size_t r) const noexcept {
            return m_select.select1(r);
        }
        [[nodiscard]] std::size_t select0(std::size_t r) const noexcept {
            return m_select.select0(r);
        }

        // rank1(i) and the bit at i
        // Precondition: i < size.
        [[nodiscard]] rank_bit rank1_bit(std::size_t i) const noexcept {
            if constexpr (requires { m_rank.rank1_bit(i); })
                return m_rank.rank1_bit(i);
            else
                return {m_rank.rank1(i), static_cast<bool>(m_bits[i])};
        }

        [[nodiscard]] const BitVec& bits() const noexcept {
            return m_bits;
        }

        [[nodiscard]] std::size_t memory_size() const noexcept {
            return sizeof(*this) + (m_rank.memory_size() - sizeof(RankT)) +
                   (m_select.memory_size() - sizeof(SelectT)) +
                   m_bits.nb_words() * sizeof(typename BitVec::word_type);
        }

        template <typename Sink>
            requires io::byte_sink<Sink>
        [[nodiscard]] bool save(Sink& sink) const noexcept {
            return m_bits.save(sink);
        }

        template <typename ByteSource>
            requires io::byte_source<ByteSource>
        [[nodiscard]] static std::expected<bit_dict, io::load_error>
        load(ByteSource& reader) noexcept {
            auto b = BitVec::load(reader);
            if (!b)
                return std::unexpected(b.error());
            return bit_dict(std::move(*b));
        }

        [[nodiscard]] auto as_view() const noexcept {
            return bit_dict_view(m_bits.as_const_view(), m_rank.as_view(), m_select.as_view());
        }

    private:
        [[nodiscard]] static SelectT make_select(const BitVec& bits, const RankT& rank) {
            if constexpr (std::constructible_from<SelectT, const RankT&>)
                return SelectT(rank);
            else
                return SelectT(bits);
        }

        BitVec m_bits;
        RankT m_rank;
        SelectT m_select;
    };

    // bit_dict non-owning
    template <typename BitVecView, typename RankView, typename SelectView> class bit_dict_view {
    public:
        bit_dict_view(BitVecView bits, RankView rank, SelectView sel) noexcept
            : m_bits(bits), m_rank(rank), m_select(sel) {}

        [[nodiscard]] std::size_t size() const noexcept {
            return m_bits.size();
        }
        [[nodiscard]] bool operator[](std::size_t i) const noexcept {
            return static_cast<bool>(m_bits[i]);
        }
        [[nodiscard]] std::size_t rank1(std::size_t i) const noexcept {
            return m_rank.rank1(i);
        }
        [[nodiscard]] std::size_t rank0(std::size_t i) const noexcept {
            return i - m_rank.rank1(i);
        }
        [[nodiscard]] std::size_t select1(std::size_t r) const noexcept {
            return m_select.select1(r);
        }
        [[nodiscard]] std::size_t select0(std::size_t r) const noexcept {
            return m_select.select0(r);
        }

        [[nodiscard]] rank_bit rank1_bit(std::size_t i) const noexcept {
            if constexpr (requires { m_rank.rank1_bit(i); })
                return m_rank.rank1_bit(i);
            else
                return {m_rank.rank1(i), static_cast<bool>(m_bits[i])};
        }

    private:
        BitVecView m_bits;
        RankView m_rank;
        SelectView m_select;
    };

    template <typename Word = std::uint64_t, pack_endian Endian = pack_endian::lsb>
    using bit_dict_default = bit_dict<bit_vector<Word, Endian>, rank9<bit_vector<Word, Endian>>,
                                      darray<bit_vector<Word, Endian>, select_target::both>>;

} // namespace cds
