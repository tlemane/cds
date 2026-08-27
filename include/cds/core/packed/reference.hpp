#pragma once

#include <utility>
#include <cds/core/packed/storage.hpp>

namespace cds {

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian, pack_mode Mode>
    struct bit_packer;

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian, pack_mode Mode>
    class const_packed_reference : public packing_storage<Width, Endian, Mode> {
    public:
        using word_type = Word;
        using value_type = Value;
        using storage_type = packing_storage<Width, Endian, Mode>;
        using bp = bit_packer<word_type, value_type, Width, Endian, Mode>;

        template <typename... Args>
        constexpr const_packed_reference(const word_type* data, std::size_t index,
                                         Args&&... args) noexcept
            : storage_type(std::forward<Args>(args)...), m_data(data), m_index(index) {}

        operator value_type() const noexcept {
            return this->operator*();
        }

        value_type operator*() const noexcept {
            return this->visit([&](auto... args) { return bp::unpack(m_data, m_index, args...); });
        }

    private:
        const word_type* m_data{nullptr};
        std::size_t m_index{0};
    };

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian, pack_mode Mode>
    class packed_reference : public packing_storage<Width, Endian, Mode> {
    public:
        using word_type = Word;
        using value_type = Value;
        using storage_type = packing_storage<Width, Endian, Mode>;
        using bp = bit_packer<word_type, value_type, Width, Endian, Mode>;
        static constexpr bool safe = true;

        template <typename... Args>
        constexpr packed_reference(word_type* data, std::size_t index, Args&&... args) noexcept
            : storage_type(std::forward<Args>(args)...), m_data(data), m_index(index) {}

        operator value_type() const noexcept {
            return this->operator*();
        }

        value_type operator*() const noexcept {
            if constexpr (Width == 0 || Endian == pack_endian::rt || Mode == pack_mode::rt) {
                return this->visit(
                    [&](auto... args) { return bp::unpack(m_data, m_index, args...); });
            } else {
                return bp::unpack(m_data, m_index);
            }
        }

        packed_reference& operator=(value_type value) noexcept {
            if constexpr (Width == 0 || Endian == pack_endian::rt || Mode == pack_mode::rt) {
                this->visit([&](auto... args) {
                    bp::template pack<safe>(m_data, m_index, value, args...);
                });
            } else {
                bp::template pack<safe>(m_data, m_index, value);
            }

            return *this;
        }

        packed_reference& operator=(unsafe<value_type> value) noexcept {
            if constexpr (Width == 0 || Endian == pack_endian::rt || Mode == pack_mode::rt) {
                this->visit([&](auto... args) {
                    bp::template pack<false>(m_data, m_index, value.value, args...);
                });
            } else {
                bp::template pack<false>(m_data, m_index, value.value);
            }
            return *this;
        }

    private:
        word_type* m_data{nullptr};
        std::size_t m_index{0};
    };

} // namespace cds
