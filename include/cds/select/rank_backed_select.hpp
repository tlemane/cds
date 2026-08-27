#pragma once

#include <cstddef>
#include <expected>
#include <utility>

#include <cds/io/byte.hpp>

namespace cds {

    // rank_backed_select: composes a rank structure with a select structure built
    // on top of it.
    //
    // select9/select_poppy are built on a rank, their constructor takes a
    // const rank_type&, but ef needs an index it can build straight from the
    // source. This adapter bridges that for any rank/select pair, letting ef carry
    // a far more succinct select0 index than darray<both>.
    //
    // Lifetime: the select holds pointers into the source words and spans into the
    // rank's heap. So the source must outlive this, and copies must REBUILD the
    // select (a copied rank has a fresh heap the original's select can't
    // reference)
    // moves are fine
    template <typename Source, typename RankT, typename SelectT> class rank_backed_select {
        static_assert(std::constructible_from<RankT, const Source&>,
                      "rank_backed_select: RankT must be constructible from const Source&");
        static_assert(
            std::constructible_from<SelectT, const RankT&>,
            "rank_backed_select: SelectT must be constructible from const RankT& "
            "(it layers on the rank, e.g. select9 over rank9, select_poppy over rank_poppy)");

    public:
        using source_type = Source;
        using rank_type = RankT;
        using select_type = SelectT;

        // Build from the bit source: rank first, then select over it.
        explicit rank_backed_select(const Source& source) : m_rank(source), m_select(m_rank) {}

        // Copy rebuilds select over the copied rank
        rank_backed_select(const rank_backed_select& other)
            : m_rank(other.m_rank), m_select(m_rank) {}
        rank_backed_select(rank_backed_select&& other) noexcept
            : m_rank(std::move(other.m_rank)), m_select(std::move(other.m_select)) {}
        rank_backed_select& operator=(const rank_backed_select& other) {
            if (this != &other) {
                m_rank = other.m_rank;
                m_select = SelectT(m_rank); // rebuild over our own rank
            }
            return *this;
        }
        rank_backed_select& operator=(rank_backed_select&& other) noexcept {
            m_rank = std::move(other.m_rank);
            m_select = std::move(other.m_select);
            return *this;
        }
        ~rank_backed_select() = default;

        [[nodiscard]] std::size_t select1(std::size_t r) const noexcept {
            return m_select.select1(r);
        }
        [[nodiscard]] std::size_t select0(std::size_t r) const noexcept {
            return m_select.select0(r);
        }

        [[nodiscard]] const rank_type& rank() const noexcept {
            return m_rank;
        }
        [[nodiscard]] const select_type& select() const noexcept {
            return m_select;
        }

        [[nodiscard]] auto as_view() const noexcept {
            return m_select.as_view();
        }

        [[nodiscard]] std::size_t memory_size() const noexcept {
            return sizeof(*this) + (m_rank.memory_size() - sizeof(RankT)) +
                   (m_select.memory_size() - sizeof(SelectT));
        }

        template <typename Sink>
            requires io::byte_sink<Sink>
        [[nodiscard]] bool save(Sink& sink) const noexcept {
            return m_rank.save(sink) && m_select.save(sink);
        }

        template <typename ByteSource>
            requires io::byte_source<ByteSource>
        [[nodiscard]] static std::expected<rank_backed_select, io::load_error>
        load(ByteSource& reader, const Source& source) {
            auto rank = RankT::load(reader, source);
            if (!rank)
                return std::unexpected(rank.error());
            RankT r = std::move(*rank);

            auto sel = SelectT::load(reader, r);
            if (!sel)
                return std::unexpected(sel.error());

            return rank_backed_select(from_tag{}, std::move(r), std::move(*sel));
        }

    private:
        struct from_tag {};
        rank_backed_select(from_tag, RankT&& rank, SelectT&& select) noexcept
            : m_rank(std::move(rank)), m_select(std::move(select)) {}

        RankT m_rank;
        SelectT m_select;
    };

} // namespace cds
