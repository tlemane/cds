#pragma once

#include <expected>
#include <cds/core/debug.hpp>
#include <cds/core/packed/type.hpp>
#include <cds/core/packed/packer.hpp>
#include <cds/core/packed/iterator.hpp>
#include <cds/packed/view.hpp>
#include <cds/io/format.hpp>
#include <cds/io/byte.hpp>

namespace cds {

    template <typename Word, typename Value, std::size_t Capacity, std::uint8_t Width,
              pack_endian Endian, pack_mode Mode>
    class packed_array : public packing_storage<Width, Endian, Mode> {
    public:
        using word_type = Word;
        using value_type = Value;

        using storage_type = packing_storage<Width, Endian, Mode>;
        using bp = bit_packer<word_type, value_type, Width, Endian, Mode>;
        using reference = packed_reference<word_type, value_type, Width, Endian, Mode>;
        using const_reference = const_packed_reference<word_type, value_type, Width, Endian, Mode>;
        using iterator = packed_iterator_t<word_type, value_type, Width, Endian, Mode>;
        using const_iterator = const_packed_iterator_t<word_type, value_type, Width, Endian, Mode>;

        using view_type = packed_view<Word, Value, Width, Endian, Mode>;
        using const_view_type = const_packed_view<Word, Value, Width, Endian, Mode>;

    private:
        static inline constexpr bool is_word_capacity = (Width == 0) || (Mode == pack_mode::rt);

        static inline constexpr std::size_t s_capacity_words =
            is_word_capacity ? Capacity : to_nb_words<word_type>(Capacity, Width, Mode);

    private:
        constexpr reference get_ref(std::size_t index) noexcept {
            return this->visit([&](auto... args) { return bp::ref(m_data, index, args...); });
        }
        constexpr const_reference get_cref(std::size_t index) const noexcept {
            return this->visit([&](auto... args) { return bp::cref(m_data, index, args...); });
        }

        constexpr iterator get_it(std::size_t index) noexcept {
            return this->visit([&](auto... args) {
                return iterator(m_data, sc<typename iterator::difference_type>(index), args...);
            });
        }
        constexpr const_iterator get_cit(std::size_t index) const noexcept {
            return this->visit([&](auto... args) {
                return const_iterator(m_data, sc<typename iterator::difference_type>(index),
                                      args...);
            });
        }

    public:
        constexpr packed_array(const packed_array&) noexcept = default;
        constexpr packed_array(packed_array&&) noexcept = default;

        constexpr packed_array& operator=(const packed_array&) noexcept = default;
        constexpr packed_array& operator=(packed_array&&) noexcept = default;

        ~packed_array() noexcept = default;

        constexpr packed_array(std::uint8_t width = Width, pack_endian endian = Endian,
                               pack_mode mode = Mode) noexcept
            : storage_type(width, endian, mode) {}

        template <typename InputIt>
        constexpr packed_array(InputIt first, InputIt last, std::uint8_t width = Width,
                               pack_endian endian = Endian, pack_mode mode = Mode) noexcept
            : packed_array(width, endian, mode) {
            for (; first != last; ++first)
                push_back(*first);
        }

        constexpr packed_array(std::initializer_list<value_type> values, std::uint8_t width = Width,
                               pack_endian endian = Endian, pack_mode mode = Mode) noexcept
            : packed_array(values.begin(), values.end(), width, endian, mode) {}

        [[nodiscard]] view_type as_view() noexcept {
            return {data(), size(), this->width(), this->endian(), this->mode()};
        }

        [[nodiscard]] const_view_type as_const_view() const noexcept {
            return {data(), size(), this->width(), this->endian(), this->mode()};
        }

        [[nodiscard]] constexpr reference operator[](std::size_t index) noexcept {
            CDS_ASSERT(index < m_size, "out of range: index={}, size={}", index, m_size);
            return get_ref(index);
        }

        [[nodiscard]] constexpr const_reference operator[](std::size_t index) const noexcept {
            CDS_ASSERT(index < m_size, "out of range: index={}, size={}", index, m_size);
            return get_cref(index);
        }

        [[nodiscard]] constexpr reference at(std::size_t index) {
            CDS_PANIC(index < m_size, "out of range: index={}, size={}", index, m_size);
            return get_ref(index);
        }

        [[nodiscard]] constexpr const_reference at(std::size_t index) const {
            CDS_PANIC(index < m_size, "out of range: index={}, size={}", index, m_size);
            return get_cref(index);
        }

        [[nodiscard]] constexpr reference front() noexcept {
            return get_ref(0);
        }

        [[nodiscard]] constexpr const_reference front() const noexcept {
            return get_cref(0);
        }

        [[nodiscard]] constexpr reference back() noexcept {
            return get_ref(m_size - 1);
        }

        [[nodiscard]] constexpr const_reference back() const noexcept {
            return get_cref(m_size - 1);
        }

        constexpr auto& push_back(value_type value) {
            CDS_ASSERT(m_size < capacity(), "out of range: capacity={}, size={}", capacity(),
                       m_size);
            (*this)[m_size++] = value;
            return *this;
        }

        constexpr auto& emplace_back(value_type value) {
            return push_back(value);
        }

        void pop_back() noexcept {
            CDS_ASSERT(m_size > 0, "pop_back on empty packed_vector");
            --m_size;
        }

        [[nodiscard]] constexpr iterator begin() noexcept {
            return get_it(0);
        }

        [[nodiscard]] constexpr iterator end() noexcept {
            return get_it(m_size);
        }

        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            return get_cit(0);
        }

        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return get_cit(m_size);
        }

        [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
            return get_cit(0);
        }

        [[nodiscard]] constexpr const_iterator cend() const noexcept {
            return get_cit(m_size);
        }

        [[nodiscard]] constexpr std::size_t capacity() const noexcept {
            return is_word_capacity ? to_capacity<word_type>(Capacity, this->width(), this->mode())
                                    : Capacity;
        }

        [[nodiscard]] constexpr std::size_t memory_size() const noexcept {
            return s_capacity_words * sizeof(Word) + sizeof(*this);
        }

        [[nodiscard]] constexpr std::size_t size() const noexcept {
            return m_size;
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return m_size == 0;
        }

        [[nodiscard]] constexpr const word_type* data() const noexcept {
            return m_data;
        }

        [[nodiscard]] constexpr word_type* data() noexcept {
            return m_data;
        }

        [[nodiscard]] constexpr std::span<word_type> span() noexcept {
            return {data(), s_capacity_words};
        }

        [[nodiscard]] constexpr std::span<const word_type> span() const noexcept {
            return {data(), s_capacity_words};
        }

        constexpr void clear() noexcept {
            m_size = 0;
        }

        template <typename Sink> [[nodiscard]] bool save(Sink& sink) const noexcept {
            if (!io::write_cds_version(sink))
                return false;

            const detail::packed_save_header h{detail::packed_save_magic,
                                               this->width(),
                                               static_cast<std::uint8_t>(this->endian()),
                                               static_cast<std::uint8_t>(this->mode()),
                                               0,
                                               static_cast<std::uint64_t>(m_size)};

            if (!sink.write(&h, sizeof(h)))
                return false;

            const std::size_t nw = to_nb_words<word_type>(m_size, this->width(), this->mode());
            if (nw > 0 && !sink.write(m_data, nw * sizeof(word_type)))
                return false;

            return true;
        }

        template <typename Source>
        [[nodiscard]] static std::expected<packed_array, io::load_error>
        load(Source& source) noexcept {
            const auto version = io::read_cds_version_compatible(source);
            if (!version)
                return std::unexpected(version.error());

            detail::packed_save_header h{};
            if (!source.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::packed_save_magic)
                return std::unexpected(io::load_error::bad_magic);

            const auto params = detail::resolve_packing_params<Width, Endian, Mode>(h);
            if (!params)
                return std::unexpected(io::load_error::type_mismatch);
            const auto [width, endian, mode] = *params;

            packed_array a(width, endian, mode);

            if (static_cast<std::size_t>(h.size) > a.capacity())
                return std::unexpected(io::load_error::capacity_exceeded);

            a.m_size = static_cast<std::size_t>(h.size);

            const std::size_t nw = to_nb_words<word_type>(a.m_size, width, mode);
            if (nw > 0 && !source.read(a.m_data, nw * sizeof(word_type)))
                return std::unexpected(io::load_error::io_failure);

            return a;
        }

    private:
        word_type m_data[s_capacity_words]{0};
        std::size_t m_size{0};
    };

}
