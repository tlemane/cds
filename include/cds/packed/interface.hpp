#pragma once

#include <concepts>
#include <cstddef>
#include <utility>

#include <cds/packed/array.hpp>
#include <cds/packed/view.hpp>
#include <cds/packed/vector.hpp>

namespace cds {

    template <typename T>
    concept PackedReadable = requires(const T& t, std::size_t i) {
        typename T::value_type;
        { t[i] } -> std::convertible_to<typename T::value_type>;
        { t.size() } -> std::convertible_to<std::size_t>;
        { t.empty() } -> std::convertible_to<bool>;
    };

    template <typename T>
    concept PackedWritable =
        PackedReadable<T> && requires(T& t, std::size_t i, typename T::value_type v) {
            t[i] = v;
            t.at(i) = v;
        };

    template <typename T>
    concept PackedDynamic = PackedWritable<T> && requires(T& t, typename T::value_type v) {
        t.push_back(v);
        t.pop_back();
    };

    template <typename Value> class packed_const_interface {
    public:
        virtual ~packed_const_interface() = default;
        [[nodiscard]] virtual Value get(std::size_t index) const noexcept = 0;
        [[nodiscard]] virtual std::size_t size() const noexcept = 0;
        [[nodiscard]] virtual bool empty() const noexcept = 0;
    };

    template <typename Value> class packed_interface : public packed_const_interface<Value> {
    public:
        virtual ~packed_interface() = default;

        virtual void set(std::size_t index, Value value) noexcept = 0;
        virtual void clear(std::size_t index) noexcept = 0;
    };

    template <typename Value> class packed_dynamic_interface : public packed_interface<Value> {
    public:
        virtual void push_back(Value value) = 0;
        virtual void pop_back() = 0;
    };

    template <PackedWritable T>
    class packed_adapter final : public packed_interface<typename T::value_type> {
    public:
        using value_type = typename T::value_type;

        template <typename... Args>
        explicit packed_adapter(Args&&... args) : m_packed(std::forward<Args>(args)...) {}

        packed_adapter(const packed_adapter&) = delete;
        packed_adapter(packed_adapter&&) = delete;
        packed_adapter& operator=(const packed_adapter&) = delete;
        packed_adapter& operator=(packed_adapter&&) = delete;

        ~packed_adapter() override = default;

        [[nodiscard]] value_type get(std::size_t index) const noexcept override {
            return sc<value_type>(m_packed[index]);
        }

        [[nodiscard]] std::size_t size() const noexcept override {
            return m_packed.size();
        }

        [[nodiscard]] bool empty() const noexcept override {
            return m_packed.empty();
        }

        void set(std::size_t index, value_type value) noexcept override {
            m_packed[index] = value;
        }

        void clear(std::size_t index) noexcept override {
            m_packed[index] = value_type{0};
        }

        [[nodiscard]] T& underlying() noexcept {
            return m_packed;
        }
        [[nodiscard]] const T& underlying() const noexcept {
            return m_packed;
        }

    private:
        T m_packed;
    };

    template <PackedReadable T>
    class const_packed_adapter final : public packed_const_interface<typename T::value_type> {
    public:
        using value_type = typename T::value_type;

        template <typename... Args>
        explicit const_packed_adapter(Args&&... args) : m_packed(std::forward<Args>(args)...) {}

        const_packed_adapter(const const_packed_adapter&) = delete;
        const_packed_adapter(const_packed_adapter&&) = delete;
        const_packed_adapter& operator=(const const_packed_adapter&) = delete;
        const_packed_adapter& operator=(const_packed_adapter&&) = delete;

        ~const_packed_adapter() override = default;

        [[nodiscard]] value_type get(std::size_t index) const noexcept override {
            return sc<value_type>(m_packed[index]);
        }

        [[nodiscard]] std::size_t size() const noexcept override {
            return m_packed.size();
        }

        [[nodiscard]] bool empty() const noexcept override {
            return m_packed.empty();
        }

        [[nodiscard]] const T& underlying() const noexcept {
            return m_packed;
        }

    private:
        T m_packed;
    };

    template <PackedDynamic T>
    class packed_dynamic_adapter final : public packed_dynamic_interface<typename T::value_type> {
    public:
        using value_type = typename T::value_type;

        template <typename... Args>
        explicit packed_dynamic_adapter(Args&&... args) : m_packed(std::forward<Args>(args)...) {}

        packed_dynamic_adapter(const packed_dynamic_adapter&) = delete;
        packed_dynamic_adapter(packed_dynamic_adapter&&) = delete;
        packed_dynamic_adapter& operator=(const packed_dynamic_adapter&) = delete;
        packed_dynamic_adapter& operator=(packed_dynamic_adapter&&) = delete;

        ~packed_dynamic_adapter() override = default;

        [[nodiscard]] value_type get(std::size_t index) const noexcept override {
            return sc<value_type>(m_packed[index]);
        }

        [[nodiscard]] std::size_t size() const noexcept override {
            return m_packed.size();
        }

        [[nodiscard]] bool empty() const noexcept override {
            return m_packed.empty();
        }

        void set(std::size_t index, value_type value) noexcept override {
            m_packed[index] = value;
        }

        void clear(std::size_t index) noexcept override {
            m_packed[index] = value_type{0};
        }

        void push_back(value_type value) override {
            m_packed.push_back(value);
        }

        void pop_back() override {
            m_packed.pop_back();
        }

        [[nodiscard]] T& underlying() noexcept {
            return m_packed;
        }
        [[nodiscard]] const T& underlying() const noexcept {
            return m_packed;
        }

    private:
        T m_packed;
    };

} // namespace cds
