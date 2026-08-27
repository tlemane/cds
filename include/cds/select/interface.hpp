#pragma once

#include <cstddef>
#include <utility>

#include <cds/select/concepts.hpp>

namespace cds {

    class select1_interface {
    public:
        virtual ~select1_interface() = default;

        [[nodiscard]] virtual std::size_t select1(std::size_t r) const noexcept = 0;
    };

    template <select1_structure T> class select1_adapter final : public select1_interface {
    public:
        template <typename... Args>
        explicit select1_adapter(Args&&... args) : m_impl(std::forward<Args>(args)...) {}

        select1_adapter(const select1_adapter&) = delete;
        select1_adapter(select1_adapter&&) = delete;
        select1_adapter& operator=(const select1_adapter&) = delete;
        select1_adapter& operator=(select1_adapter&&) = delete;

        ~select1_adapter() override = default;

        [[nodiscard]] std::size_t select1(std::size_t r) const noexcept override {
            return m_impl.select1(r);
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

    template <select1_structure T> select1_adapter(T) -> select1_adapter<T>;

    class select0_interface {
    public:
        virtual ~select0_interface() = default;

        [[nodiscard]] virtual std::size_t select0(std::size_t r) const noexcept = 0;
    };

    template <select0_structure T> class select0_adapter final : public select0_interface {
    public:
        template <typename... Args>
        explicit select0_adapter(Args&&... args) : m_impl(std::forward<Args>(args)...) {}

        select0_adapter(const select0_adapter&) = delete;
        select0_adapter(select0_adapter&&) = delete;
        select0_adapter& operator=(const select0_adapter&) = delete;
        select0_adapter& operator=(select0_adapter&&) = delete;

        ~select0_adapter() override = default;

        [[nodiscard]] std::size_t select0(std::size_t r) const noexcept override {
            return m_impl.select0(r);
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

    template <select0_structure T> select0_adapter(T) -> select0_adapter<T>;

    class select_interface {
    public:
        virtual ~select_interface() = default;

        [[nodiscard]] virtual std::size_t select1(std::size_t r) const noexcept = 0;
        [[nodiscard]] virtual std::size_t select0(std::size_t r) const noexcept = 0;
    };

    template <select_structure T> class select_adapter final : public select_interface {
    public:
        template <typename... Args>
        explicit select_adapter(Args&&... args) : m_impl(std::forward<Args>(args)...) {}

        select_adapter(const select_adapter&) = delete;
        select_adapter(select_adapter&&) = delete;
        select_adapter& operator=(const select_adapter&) = delete;
        select_adapter& operator=(select_adapter&&) = delete;

        ~select_adapter() override = default;

        [[nodiscard]] std::size_t select1(std::size_t r) const noexcept override {
            return m_impl.select1(r);
        }

        [[nodiscard]] std::size_t select0(std::size_t r) const noexcept override {
            return m_impl.select0(r);
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

    template <select_structure T> select_adapter(T) -> select_adapter<T>;

}
