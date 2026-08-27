#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

#include <cds/core/debug.hpp>
#include <cds/core/packed/type.hpp>
#include <cds/core/packed/packer.hpp>

namespace cds {

    template <typename Word, pack_endian Endian>
        requires(Endian != pack_endian::rt)
    struct bit_ops {
        using word_type = Word;

        static constexpr std::size_t digits{std::numeric_limits<word_type>::digits};

        static constexpr word_type extract(const word_type* data, std::size_t bit_index,
                                           std::uint8_t width) noexcept {
            assert(width > 0 && width <= digits);

            const std::size_t s = bit_index / digits;
            const std::size_t e = (bit_index + width - 1) / digits;
            const std::size_t sb = bit_index % digits;
            const word_type mask = static_cast<word_type>(
                (width == digits) ? ~word_type{0} : ((word_type{1} << width) - 1));

            return detail::extract_bits<Endian>(data, s, e, sb, width, digits, mask);
        }

        // reads a single unaligned 64-bit word at byte granularity
        // Precondition: 0 < width <= 57
        [[nodiscard]] static std::uint64_t
        extract_fast(const word_type* data, std::size_t bit_index, std::uint8_t width) noexcept
            requires(Endian == pack_endian::lsb)
        {
            CDS_ASSERT(width > 0 && width <= 57, "extract_fast: width ({}) must be in [1, 57]",
                       static_cast<unsigned>(width));
            std::uint64_t word;
            std::memcpy(&word, reinterpret_cast<const unsigned char*>(data) + (bit_index >> 3),
                        sizeof(word));
            return (word >> (bit_index & 7)) & ((std::uint64_t{1} << width) - 1);
        }

        template <bool safe = true>
        static constexpr void insert(word_type* data, std::size_t bit_index, std::uint8_t width,
                                     word_type value) noexcept {
            assert(width > 0 && width <= digits);

            const std::size_t s = bit_index / digits;
            const std::size_t e = (bit_index + width - 1) / digits;
            const std::size_t sb = bit_index % digits;
            const word_type mask = static_cast<word_type>(
                (width == digits) ? ~word_type{0} : ((word_type{1} << width) - 1));
            const word_type v = value & mask;

            detail::insert_bits<Endian, safe>(data, s, e, sb, width, digits, mask, v);
        }
    };

    template <typename Word> struct bit_ops<Word, pack_endian::rt> {
        using word_type = Word;

        static constexpr std::size_t digits{std::numeric_limits<word_type>::digits};

        template <pack_endian E> using bo = bit_ops<word_type, E>;

        static constexpr word_type extract(const word_type* data, std::size_t bit_index,
                                           std::uint8_t width, pack_endian type) noexcept {
            switch (type) {
                case pack_endian::msb: return bo<pack_endian::msb>::extract(data, bit_index, width);
                case pack_endian::lsb: return bo<pack_endian::lsb>::extract(data, bit_index, width);
                default: std::unreachable();
            }
        }

        template <bool safe = true>
        static constexpr void insert(word_type* data, std::size_t bit_index, std::uint8_t width,
                                     word_type value, pack_endian type) noexcept {
            switch (type) {
                case pack_endian::msb:
                    bo<pack_endian::msb>::template insert<safe>(data, bit_index, width, value);
                    break;
                case pack_endian::lsb:
                    bo<pack_endian::lsb>::template insert<safe>(data, bit_index, width, value);
                    break;
                default: std::unreachable();
            }
        }
    };

    template <typename Word> using bit_ops_lsb = bit_ops<Word, pack_endian::lsb>;

    template <typename Word> using bit_ops_msb = bit_ops<Word, pack_endian::msb>;

    template <typename Word> using bit_ops_rt = bit_ops<Word, pack_endian::rt>;

} // namespace cds
