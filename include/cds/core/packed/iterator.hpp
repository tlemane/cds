#pragma once

#include <iterator>

#include <cds/core/common.hpp>
#include <cds/core/packed/storage.hpp>
#include <cds/core/packed/reference.hpp>

namespace cds {

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian, pack_mode Mode,
              bool Const>
    class packed_iterator : public packing_storage<Width, Endian, Mode> {
        using storage_type = packing_storage<Width, Endian, Mode>;

    public:
        using word_type = Word;
        using value_type = Value;

        using pointer = std::conditional_t<Const, const word_type*, word_type*>;

        using reference =
            std::conditional_t<Const,
                               const_packed_reference<word_type, value_type, Width, Endian, Mode>,
                               packed_reference<word_type, value_type, Width, Endian, Mode>>;

        using bp = typename reference::bp;

        using difference_type = std::ptrdiff_t;
        using iterator_category = std::random_access_iterator_tag;
        using iterator_concept = std::random_access_iterator_tag;

        constexpr packed_iterator(pointer data, difference_type index, std::uint8_t width = Width,
                                  pack_endian endian = Endian, pack_mode mode = Mode) noexcept
            : storage_type(width, endian, mode), m_data(data), m_index(index) {}

        [[nodiscard]]
        constexpr reference operator*() const noexcept {
            return this->visit([&](auto... args) -> reference {
                if constexpr (Const)
                    return bp::cref(m_data, sc<std::size_t>(m_index), args...);
                else
                    return bp::ref(m_data, sc<std::size_t>(m_index), args...);
            });
        }

        [[nodiscard]]
        constexpr reference operator[](difference_type n) const noexcept {
            return *(*this + n);
        }

        constexpr packed_iterator& operator++() noexcept {
            ++m_index;
            return *this;
        }

        constexpr packed_iterator operator++(int) noexcept {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        constexpr packed_iterator& operator--() noexcept {
            --m_index;
            return *this;
        }

        constexpr packed_iterator operator--(int) noexcept {
            auto tmp = *this;
            --*this;
            return tmp;
        }

        constexpr packed_iterator& operator+=(difference_type n) noexcept {
            m_index += n;
            return *this;
        }

        constexpr packed_iterator& operator-=(difference_type n) noexcept {
            m_index -= n;
            return *this;
        }

        [[nodiscard]]
        friend constexpr packed_iterator operator+(packed_iterator it, difference_type n) noexcept {
            return it += n;
        }

        [[nodiscard]]
        friend constexpr packed_iterator operator+(difference_type n, packed_iterator it) noexcept {
            return it += n;
        }

        [[nodiscard]]
        friend constexpr packed_iterator operator-(packed_iterator it, difference_type n) noexcept {
            return it -= n;
        }

        [[nodiscard]]
        friend constexpr difference_type operator-(const packed_iterator& lhs,
                                                   const packed_iterator& rhs) noexcept {
            return lhs.m_index - rhs.m_index;
        }

        friend constexpr bool operator==(const packed_iterator& lhs,
                                         const packed_iterator& rhs) noexcept {
            return lhs.m_data == rhs.m_data && lhs.m_index == rhs.m_index;
        }

        friend constexpr auto operator<=>(const packed_iterator& lhs,
                                          const packed_iterator& rhs) noexcept {
            return lhs.m_index <=> rhs.m_index;
        }

    private:
        pointer m_data{};
        difference_type m_index{};
    };

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian, pack_mode Mode>
    using packed_iterator_t = packed_iterator<Word, Value, Width, Endian, Mode, false>;

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian, pack_mode Mode>
    using const_packed_iterator_t = packed_iterator<Word, Value, Width, Endian, Mode, true>;

} // namespace cds
