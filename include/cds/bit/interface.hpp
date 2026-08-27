#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#include <cds/packed/array.hpp>
#include <cds/packed/view.hpp>
#include <cds/packed/vector.hpp>
#include <cds/packed/interface.hpp>

namespace cds {

    template <typename Bit>
    concept BitReadable = requires(const Bit& b, std::size_t i) {
        { b[i] } -> std::convertible_to<std::uint8_t>;
        { b.size() } -> std::convertible_to<std::size_t>;
        { b.empty() } -> std::convertible_to<bool>;
    };

    template <typename Bit>
    concept BitWritable = BitReadable<Bit> && requires(Bit& b, std::size_t i, std::uint8_t v) {
        b[i] = v;
        b.at(i) = v;
    };

    template <typename Bit>
    concept BitDynamic = BitWritable<Bit> && requires(Bit& b, std::uint8_t v) {
        b.push_back(v);
        b.pop_back();
    };

    template <typename Word, std::size_t Capacity, pack_endian Endian>
    using bit_array_impl = packed_array<Word, std::uint8_t, Capacity, 1, Endian, pack_mode::dense>;

    template <typename Word, pack_endian Endian>
    using bit_view_impl = packed_view<Word, std::uint8_t, 1, Endian, pack_mode::dense>;

    template <typename Word, pack_endian Endian>
    using const_bit_view_impl = const_packed_view<Word, std::uint8_t, 1, Endian, pack_mode::dense>;

    template <typename Word, pack_endian Endian, typename Allocator = std::allocator<Word>>
    using bit_vector_impl =
        packed_vector<Word, std::uint8_t, 1, Endian, pack_mode::dense, Allocator>;

    template <typename Derived> class bit_mutation_ops {
    public:
        bool get(std::size_t index) const noexcept {
            return sc<bool>(derived()[index]);
        }
        void set(std::size_t index) noexcept {
            derived()[index] = std::uint8_t{1};
        }

        void set_one(std::size_t index) noexcept {
            set(index);
        }

        void set_zero(std::size_t index) noexcept {
            clear(index);
        }

        void set(std::size_t index, bool value) noexcept {
            derived()[index] = sc<std::uint8_t>(value);
        }

        void clear(std::size_t index) noexcept {
            derived()[index] = std::uint8_t{0};
        }

        void flip(std::size_t index) noexcept {
            using bp = std::remove_cvref_t<decltype(derived()[index])>::bp;
            bp::flip(derived().data(), index);
        }

        void clear_safe(std::size_t index) noexcept {
            derived().at(index) = std::uint8_t{0};
        }

        [[nodiscard]] std::size_t popcount() const noexcept {
            const auto* data = derived().data();
            const std::size_t n = derived().size();
            constexpr std::size_t digits =
                std::numeric_limits<std::remove_cvref_t<decltype(*data)>>::digits;

            const std::size_t full_words = n / digits;
            std::size_t count = 0;
            for (std::size_t w = 0; w < full_words; ++w) {
                count += static_cast<std::size_t>(std::popcount(data[w]));
            }

            for (std::size_t i = full_words * digits; i < n; ++i) {
                count += derived()[i] ? 1u : 0u;
            }

            return count;
        }

    private:
        [[nodiscard]] Derived& derived() noexcept {
            return static_cast<Derived&>(*this);
        }

        [[nodiscard]] const Derived& derived() const noexcept {
            return static_cast<const Derived&>(*this);
        }
    };

    using bit_const_interface = packed_const_interface<std::uint8_t>;

    class bit_interface : public packed_interface<std::uint8_t> {
    public:
        virtual ~bit_interface() = default;

        using packed_interface<std::uint8_t>::set;
        virtual void set(std::size_t index) noexcept = 0;

        virtual void flip(std::size_t index) noexcept = 0;
    };

    class bit_dynamic_interface : public bit_interface {
    public:
        virtual void push_back(std::uint8_t value) = 0;
        virtual void pop_back() = 0;
    };

    template <BitWritable Bit> class bit_adapter final : public bit_interface {
    public:
        template <typename... Args>
        explicit bit_adapter(Args&&... args) : m_bit(std::forward<Args>(args)...) {}

        bit_adapter(const bit_adapter&) = delete;
        bit_adapter(bit_adapter&&) = delete;
        bit_adapter& operator=(const bit_adapter&) = delete;
        bit_adapter& operator=(bit_adapter&&) = delete;

        ~bit_adapter() override = default;

        [[nodiscard]] std::uint8_t get(std::size_t index) const noexcept override {
            return sc<std::uint8_t>(m_bit[index]);
        }

        [[nodiscard]] std::size_t size() const noexcept override {
            return m_bit.size();
        }

        [[nodiscard]] bool empty() const noexcept override {
            return m_bit.empty();
        }

        void set(std::size_t index) noexcept override {
            m_bit[index] = std::uint8_t{1};
        }

        void set(std::size_t index, std::uint8_t value) noexcept override {
            m_bit[index] = value;
        }

        void clear(std::size_t index) noexcept override {
            m_bit[index] = std::uint8_t{0};
        }

        void flip(std::size_t index) noexcept override {
            const std::uint8_t x = m_bit[index];
            m_bit[index] = x ^ std::uint8_t{1};
        }

        [[nodiscard]] Bit& underlying() noexcept {
            return m_bit;
        }
        [[nodiscard]] const Bit& underlying() const noexcept {
            return m_bit;
        }

    private:
        Bit m_bit;
    };

    template <BitReadable Bit> class const_bit_adapter final : public bit_const_interface {
    public:
        template <typename... Args>
        explicit const_bit_adapter(Args&&... args) : m_bit(std::forward<Args>(args)...) {}

        const_bit_adapter(const const_bit_adapter&) = delete;
        const_bit_adapter(const_bit_adapter&&) = delete;
        const_bit_adapter& operator=(const const_bit_adapter&) = delete;
        const_bit_adapter& operator=(const_bit_adapter&&) = delete;

        ~const_bit_adapter() override = default;

        [[nodiscard]] std::uint8_t get(std::size_t index) const noexcept override {
            return sc<std::uint8_t>(m_bit[index]);
        }

        [[nodiscard]] std::size_t size() const noexcept override {
            return m_bit.size();
        }

        [[nodiscard]] bool empty() const noexcept override {
            return m_bit.empty();
        }

        [[nodiscard]] const Bit& underlying() const noexcept {
            return m_bit;
        }

    private:
        Bit m_bit;
    };

    template <BitDynamic Bit> class bit_dynamic_adapter final : public bit_dynamic_interface {
    public:
        template <typename... Args>
        explicit bit_dynamic_adapter(Args&&... args) : m_bit(std::forward<Args>(args)...) {}

        bit_dynamic_adapter(const bit_dynamic_adapter&) = delete;
        bit_dynamic_adapter(bit_dynamic_adapter&&) = delete;
        bit_dynamic_adapter& operator=(const bit_dynamic_adapter&) = delete;
        bit_dynamic_adapter& operator=(bit_dynamic_adapter&&) = delete;

        ~bit_dynamic_adapter() override = default;

        [[nodiscard]] std::uint8_t get(std::size_t index) const noexcept override {
            return sc<std::uint8_t>(m_bit[index]);
        }

        [[nodiscard]] std::size_t size() const noexcept override {
            return m_bit.size();
        }

        [[nodiscard]] bool empty() const noexcept override {
            return m_bit.empty();
        }

        void set(std::size_t index) noexcept override {
            m_bit[index] = std::uint8_t{1};
        }

        void set(std::size_t index, std::uint8_t value) noexcept override {
            m_bit[index] = value;
        }

        void clear(std::size_t index) noexcept override {
            m_bit[index] = std::uint8_t{0};
        }

        void flip(std::size_t index) noexcept override {
            const std::uint8_t x = m_bit[index];
            m_bit[index] = x ^ std::uint8_t{1};
        }

        void push_back(std::uint8_t value) override {
            m_bit.push_back(value);
        }

        void pop_back() override {
            m_bit.pop_back();
        }

        [[nodiscard]] Bit& underlying() noexcept {
            return m_bit;
        }
        [[nodiscard]] const Bit& underlying() const noexcept {
            return m_bit;
        }

    private:
        Bit m_bit;
    };

    template <typename T> struct bit_source_traits;

    template <typename T>
    concept bit_source = requires(const T& t) {
        typename bit_source_traits<T>::word_type;
        {
            bit_source_traits<T>::data(t)
        } -> std::same_as<const typename bit_source_traits<T>::word_type*>;
        { bit_source_traits<T>::size(t) } -> std::convertible_to<std::size_t>;
        { bit_source_traits<T>::offset(t) } -> std::convertible_to<std::size_t>;
    } && requires {
        { bit_source_traits<T>::endian } -> std::convertible_to<pack_endian>;
    };

    template <typename Word, std::size_t Capacity, pack_endian Endian>
    struct bit_source_traits<bit_array_impl<Word, Capacity, Endian>> {
        using source_type = bit_array_impl<Word, Capacity, Endian>;
        using word_type = Word;
        static constexpr pack_endian endian = Endian;
        static constexpr pack_mode mode = pack_mode::dense;

        static const Word* data(const source_type& s) noexcept {
            return s.data();
        }
        static std::size_t size(const source_type& s) noexcept {
            return s.size();
        }
        static std::size_t offset(const source_type&) noexcept {
            return 0;
        }
    };

    template <typename Word, pack_endian Endian, typename Allocator>
    struct bit_source_traits<bit_vector_impl<Word, Endian, Allocator>> {
        using source_type = bit_vector_impl<Word, Endian, Allocator>;
        using word_type = Word;
        static constexpr pack_endian endian = Endian;
        static constexpr pack_mode mode = pack_mode::dense;

        static const Word* data(const source_type& s) noexcept {
            return s.data();
        }
        static std::size_t size(const source_type& s) noexcept {
            return s.size();
        }
        static std::size_t offset(const source_type&) noexcept {
            return 0;
        }
    };

    template <typename Word, pack_endian Endian>
    struct bit_source_traits<bit_view_impl<Word, Endian>> {
        using source_type = bit_view_impl<Word, Endian>;
        using word_type = Word;
        static constexpr pack_endian endian = Endian;
        static constexpr pack_mode mode = pack_mode::dense;

        static const Word* data(const source_type& s) noexcept {
            return s.data();
        }
        static std::size_t size(const source_type& s) noexcept {
            return s.size();
        }
        static std::size_t offset(const source_type& s) noexcept {
            return s.offset();
        }
    };

    template <typename Word, pack_endian Endian>
    struct bit_source_traits<const_bit_view_impl<Word, Endian>> {
        using source_type = const_bit_view_impl<Word, Endian>;
        using word_type = Word;
        static constexpr pack_endian endian = Endian;
        static constexpr pack_mode mode = pack_mode::dense;

        static const Word* data(const source_type& s) noexcept {
            return s.data();
        }
        static std::size_t size(const source_type& s) noexcept {
            return s.size();
        }
        static std::size_t offset(const source_type& s) noexcept {
            return s.offset();
        }
    };

}
