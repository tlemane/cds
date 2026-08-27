#pragma once

#include <cstddef>
#include <utility>
#include <cds/rank/concepts.hpp>

namespace cds {

    // Type-erasing interface for any rank_structure.
    // Ownership depends on the underlying rank strucutre, e.g.
    // rank9 is reference-holding, rrr is self-contained
    class rank_interface {
    public:
        virtual ~rank_interface() = default;

        [[nodiscard]] virtual std::size_t rank1(std::size_t i) const noexcept = 0;
        [[nodiscard]] virtual std::size_t rank0(std::size_t i) const noexcept = 0;
        [[nodiscard]] virtual std::size_t size() const noexcept = 0;
    };

    // Concrete rank_interface. Owns a rank structure T
    // (e.g. rank9<...>) and forwards the virtual calls to it, so a static rank
    // type can be used through a runtime-polymorphic rank_interface pointer.
    template <rank_structure T> class rank_adapter final : public rank_interface {
    public:
        template <typename... Args>
        explicit rank_adapter(Args&&... args) : m_impl(std::forward<Args>(args)...) {}

        rank_adapter(const rank_adapter&) = delete;
        rank_adapter(rank_adapter&&) = delete;
        rank_adapter& operator=(const rank_adapter&) = delete;
        rank_adapter& operator=(rank_adapter&&) = delete;

        ~rank_adapter() override = default;

        [[nodiscard]] std::size_t rank1(std::size_t i) const noexcept override {
            return m_impl.rank1(i);
        }

        [[nodiscard]] std::size_t rank0(std::size_t i) const noexcept override {
            return m_impl.rank0(i);
        }

        [[nodiscard]] std::size_t size() const noexcept override {
            return m_impl.size();
        }

        [[nodiscard]] T& underlying() noexcept {
            return m_impl;
        }
        [[nodiscard]] const T& underlying() const noexcept {
            return m_impl;
        }

    private:
        T m_impl;
    };

    // CTAD, e.g. rank_adapter(r.as_view())
    template <rank_structure T> rank_adapter(T) -> rank_adapter<T>;

} // namespace cds
