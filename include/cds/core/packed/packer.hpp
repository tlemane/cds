#pragma once

#include <cassert>
#include <cstdint>
#include <limits>
#include <utility>

#include <cds/core/common.hpp>
#include <cds/core/packed/type.hpp>
#include <cds/core/packed/reference.hpp>

namespace cds {

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian, pack_mode Mode>
    struct bit_packer;

    namespace detail {

        constexpr inline std::size_t sparse_word(std::size_t index, std::size_t width,
                                                 std::size_t digits) noexcept {
            return index / (digits / width);
        }

        constexpr inline std::size_t sparse_bit_from_word(std::size_t index, std::size_t width,
                                                          std::size_t digits,
                                                          std::size_t w) noexcept {
            const std::size_t lost = digits % width;
            const std::size_t bit = (lost == 0) ? index * width : index * width + lost * w;
            return bit % digits;
        }

        constexpr inline std::size_t sparse_bit(std::size_t index, std::size_t width,
                                                std::size_t digits) noexcept {
            const std::size_t lost = digits % width;
            const std::size_t w = sparse_word(index, width, digits);
            const std::size_t bit = (lost == 0) ? index * width : index * width + lost * w;
            return bit % digits;
        }

        template <pack_endian Endian, typename Word>
        constexpr inline Word extract_bits(const Word* data, std::size_t s, std::size_t e,
                                           std::size_t sb, std::size_t width, std::size_t digits,
                                           Word mask) noexcept {
            if (s == e) {
                if constexpr (Endian == pack_endian::msb) {
                    const std::size_t eb = sb + width - 1;
                    return sc<Word>(data[s] >> (digits - (eb + 1))) & mask;
                } else {
                    return sc<Word>(data[s] >> sb) & mask;
                }
            } else {
                const std::size_t rem = width - (digits - sb);
                if constexpr (Endian == pack_endian::msb) {
                    const Word w1 = data[s] & lsb_mask<Word>(digits - sb);
                    const Word w2 = (data[e] >> (digits - rem)) & lsb_mask<Word>(rem);
                    return ((w1 << rem) | w2) & mask;
                } else {
                    const Word w1 = (data[s] >> sb) & lsb_mask<Word>(digits - sb);
                    const Word w2 = data[e] & lsb_mask<Word>(rem);
                    return ((w2 << (digits - sb)) | w1) & mask;
                }
            }
        }

        template <pack_endian Endian, bool Safe, typename Word>
        constexpr inline void insert_bits(Word* data, std::size_t s, std::size_t e, std::size_t sb,
                                          std::size_t width, std::size_t digits, Word mask,
                                          Word v) noexcept {
            if (s == e) {
                if constexpr (Endian == pack_endian::msb) {
                    const std::size_t eb = sb + width - 1;
                    const std::size_t shift = digits - (eb + 1);
                    if constexpr (Safe) {
                        data[s] &= sc<Word>(~(mask << shift));
                    }
                    data[s] |= sc<Word>(v << shift);
                } else {
                    if constexpr (Safe) {
                        data[s] &= sc<Word>(~(mask << sb));
                    }
                    data[s] |= sc<Word>(v << sb);
                }
            } else {
                const std::size_t rem = width - (digits - sb);
                if constexpr (Endian == pack_endian::msb) {
                    if constexpr (Safe) {
                        data[s] &= sc<Word>(~lsb_mask<Word>(digits - sb));
                        data[e] &= sc<Word>(~msb_mask<Word>(rem));
                    }
                    data[s] |= sc<Word>((v >> rem) & lsb_mask<Word>(digits - sb));
                    data[e] |= sc<Word>((v & lsb_mask<Word>(rem)) << (digits - rem));
                } else {
                    if constexpr (Safe) {
                        data[s] &= sc<Word>(~msb_mask<Word>(digits - sb));
                        data[e] &= sc<Word>(~lsb_mask<Word>(rem));
                    }
                    data[s] |= sc<Word>(v << sb);
                    data[e] |= sc<Word>((v >> (digits - sb)) & lsb_mask<Word>(rem));
                }
            }
        }

    } // namespace detail

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian>
    struct bit_packer<Word, Value, Width, Endian, pack_mode::none> {
        using word_type = Word;
        using traits = packed_traits<Value>;
        using packed_type = typename traits::packed_type;
        using value_type = typename traits::value_type;
        using reference = packed_reference<word_type, value_type, Width, Endian, pack_mode::none>;
        using const_reference =
            const_packed_reference<word_type, value_type, Width, Endian, pack_mode::none>;

        static inline constexpr value_type unpack(const word_type* data,
                                                  std::size_t index) noexcept {
            return traits::from(sc<typename traits::packed_type>(data[index]));
        }

        template <bool safe = true>
        static inline constexpr void pack(word_type* data, std::size_t index,
                                          value_type value) noexcept {
            data[index] = static_cast<word_type>(traits::to(value));
        }

        static inline constexpr reference ref(word_type* data, std::size_t index,
                                              std::uint8_t width = 0) noexcept {
            if constexpr (Width == 0)
                return reference(data, index, width, Endian, pack_mode::none);
            else
                return reference(data, index, Width, Endian, pack_mode::none);
        }

        static inline constexpr const_reference cref(const word_type* data, std::size_t index,
                                                     std::uint8_t width = 0) noexcept {
            if constexpr (Width == 0)
                return const_reference(data, index, width, Endian, pack_mode::none);
            else
                return const_reference(data, index, Width, Endian, pack_mode::none);
        }
    };

    template <typename Word, typename Value, pack_endian Endian, pack_mode Mode>
        requires(Mode != pack_mode::none)
    struct bit_packer<Word, Value, 1, Endian, Mode> {
        using word_type = Word;
        using traits = packed_traits<Value>;
        using packed_type = typename traits::packed_type;
        using value_type = typename traits::value_type;
        using reference = packed_reference<word_type, value_type, 1, Endian, Mode>;
        using const_reference = const_packed_reference<word_type, value_type, 1, Endian, Mode>;

        static constexpr std::size_t digits{std::numeric_limits<word_type>::digits};

        static inline constexpr value_type unpack(const word_type* data,
                                                  std::size_t index) noexcept {
            if constexpr (Endian == pack_endian::msb) {
                return (data[index / digits] >> sc<word_type>((digits - 1) - (index % digits))) & 1;
            } else {
                return (data[index / digits] >> sc<word_type>(index % digits)) & 1;
            }
        }

        static inline constexpr void flip(word_type* data, std::size_t index) noexcept {
            const std::size_t shift =
                (Endian == pack_endian::msb) ? (digits - 1) - (index % digits) : (index % digits);
            const word_type bit = sc<word_type>(word_type{1} << shift);
            data[index / digits] ^= bit;
        }

        template <bool safe = true>
        static inline constexpr void pack(word_type* data, std::size_t index,
                                          value_type value) noexcept {
            const std::size_t shift =
                (Endian == pack_endian::msb) ? (digits - 1) - (index % digits) : (index % digits);

            const word_type bit = sc<word_type>(word_type{1} << shift);
            const word_type v = sc<word_type>(sc<word_type>(value != 0) << shift);

            if constexpr (safe) {
                data[index / digits] = (data[index / digits] & ~bit) | v;
            } else {
                data[index / digits] |= v;
            }
        }

        static inline constexpr reference ref(word_type* data, std::size_t index,
                                              std::uint8_t = 0) noexcept {
            return reference(data, index, sc<std::uint8_t>(1), Endian, Mode);
        }

        static inline constexpr const_reference cref(const word_type* data, std::size_t index,
                                                     std::uint8_t = 0) noexcept {
            return const_reference(data, index, sc<std::uint8_t>(1), Endian, Mode);
        }
    };

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian>
        requires(Width != 1)
    struct bit_packer<Word, Value, Width, Endian, pack_mode::sparse> {
        using word_type = Word;
        using traits = packed_traits<Value>;
        using packed_type = typename traits::packed_type;
        using value_type = typename traits::value_type;
        using reference = packed_reference<word_type, value_type, Width, Endian, pack_mode::sparse>;
        using const_reference =
            const_packed_reference<word_type, value_type, Width, Endian, pack_mode::sparse>;

        static constexpr std::size_t width{Width};
        static constexpr std::size_t digits{std::numeric_limits<word_type>::digits};
        static constexpr word_type mask{width == digits ? word_type{0}
                                                        : (word_type{1} << width) - 1};

        static inline constexpr value_type unpack(const word_type* data,
                                                  std::size_t index) noexcept {
            if constexpr (width == digits) {
                return bit_packer<word_type, value_type, 1, Endian, pack_mode::none>::unpack(data,
                                                                                             index);
            } else {
                const std::size_t w = detail::sparse_word(index, width, digits);
                const std::size_t b = detail::sparse_bit_from_word(index, width, digits, w);

                if constexpr (Endian == pack_endian::msb)
                    return traits::from(sc<typename traits::packed_type>(
                        sc<word_type>(data[w] >> ((digits - width) - b)) & mask));
                else
                    return traits::from(
                        sc<typename traits::packed_type>(sc<word_type>(data[w] >> b) & mask));
            }
        }

        template <bool safe = true>
        static inline constexpr void pack(word_type* data, std::size_t index,
                                          value_type value) noexcept {
            if constexpr (width == digits) {
                bit_packer<word_type, value_type, 1, Endian, pack_mode::none>::template pack<safe>(
                    data, index, value);
            } else {
                const std::size_t w = detail::sparse_word(index, width, digits);
                const std::size_t b = detail::sparse_bit_from_word(index, width, digits, w);
                const word_type v = sc<word_type>(traits::to(value)) & mask;

                if constexpr (Endian == pack_endian::msb) {
                    const std::size_t shift = (digits - width) - b;
                    if constexpr (safe) {
                        data[w] = sc<word_type>((data[w] & ~(mask << shift)) | (v << shift));
                    } else {
                        data[w] |= sc<word_type>(v << shift);
                    }
                } else {
                    if constexpr (safe) {
                        data[w] = sc<word_type>((data[w] & ~(mask << b)) | (v << b));
                    } else {
                        data[w] |= sc<word_type>(v << b);
                    }
                }
            }
        }

        static inline constexpr reference ref(word_type* data, std::size_t index) noexcept {
            return reference(data, index, Width, Endian, pack_mode::sparse);
        }

        static inline constexpr const_reference cref(const word_type* data,
                                                     std::size_t index) noexcept {
            return const_reference(data, index, Width, Endian, pack_mode::sparse);
        }
    };

    template <typename Word, typename Value, pack_endian Endian>
    struct bit_packer<Word, Value, 0, Endian, pack_mode::sparse> {
        using word_type = Word;
        using traits = packed_traits<Value>;
        using packed_type = typename traits::packed_type;
        using value_type = typename traits::value_type;
        using reference = packed_reference<word_type, value_type, 0, Endian, pack_mode::sparse>;
        using const_reference =
            const_packed_reference<word_type, value_type, 0, Endian, pack_mode::sparse>;

        static constexpr std::size_t digits{std::numeric_limits<word_type>::digits};

        static inline constexpr value_type unpack(const word_type* data, std::size_t index,
                                                  std::uint8_t width) noexcept {
            assert(width > 0 && width <= digits);

            if (width == digits)
                return bit_packer<word_type, value_type, 1, Endian, pack_mode::none>::unpack(data,
                                                                                             index);

            const std::size_t w = detail::sparse_word(index, width, digits);
            const std::size_t b = detail::sparse_bit_from_word(index, width, digits, w);
            const word_type mask = lsb_mask<word_type>(width);

            if constexpr (Endian == pack_endian::msb)
                return traits::from(sc<typename traits::packed_type>(
                    sc<word_type>(data[w] >> ((digits - width) - b)) & mask));
            else
                return traits::from(
                    sc<typename traits::packed_type>(sc<word_type>(data[w] >> b) & mask));
        }

        template <bool safe = true>
        static inline constexpr void pack(word_type* data, std::size_t index, value_type value,
                                          std::uint8_t width) noexcept {
            assert(width > 0 && width <= digits);

            if (width == digits) {
                bit_packer<word_type, value_type, 1, Endian, pack_mode::none>::template pack<safe>(
                    data, index, value);
                return;
            }

            const std::size_t w = detail::sparse_word(index, width, digits);
            const std::size_t b = detail::sparse_bit_from_word(index, width, digits, w);
            const word_type mask = lsb_mask<word_type>(width);
            const word_type v = sc<word_type>(traits::to(value)) & mask;

            if constexpr (Endian == pack_endian::msb) {
                const std::size_t shift = (digits - width) - b;
                if constexpr (safe) {
                    data[w] = sc<word_type>((data[w] & ~(mask << shift)) | (v << shift));
                } else {
                    data[w] |= sc<word_type>(v << shift);
                }
            } else {
                if constexpr (safe) {
                    data[w] = sc<word_type>((data[w] & ~(mask << b)) | (v << b));
                } else {
                    data[w] |= sc<word_type>(v << b);
                }
            }
        }

        static inline constexpr reference ref(word_type* data, std::size_t index,
                                              std::uint8_t width) noexcept {
            return reference(data, index, width, Endian, pack_mode::sparse);
        }

        static inline constexpr const_reference cref(const word_type* data, std::size_t index,
                                                     std::uint8_t width) noexcept {
            return const_reference(data, index, width, Endian, pack_mode::sparse);
        }

        constexpr bit_packer(std::uint8_t w) noexcept : width(w) {}

        constexpr inline value_type unpack(const word_type* data,
                                           std::size_t index) const noexcept {
            return unpack(data, index, width);
        }

        template <bool safe = true>
        constexpr inline void pack(word_type* data, std::size_t index,
                                   value_type value) const noexcept {
            pack<safe>(data, index, value, width);
        }

        constexpr inline reference ref(word_type* data, std::size_t index) const noexcept {
            return ref(data, index, width);
        }

        constexpr inline const_reference cref(const word_type* data,
                                              std::size_t index) const noexcept {
            return cref(data, index, width);
        }

        std::uint8_t width;
    };

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian>
        requires(Width != 1)
    struct bit_packer<Word, Value, Width, Endian, pack_mode::dense> {
        using word_type = Word;
        using traits = packed_traits<Value>;
        using packed_type = typename traits::packed_type;
        using value_type = typename traits::value_type;
        using reference = packed_reference<word_type, value_type, Width, Endian, pack_mode::dense>;
        using const_reference =
            const_packed_reference<word_type, value_type, Width, Endian, pack_mode::dense>;

        static constexpr std::size_t digits{std::numeric_limits<word_type>::digits};
        static constexpr std::size_t width{Width};
        static constexpr word_type mask{width == digits ? word_type{0}
                                                        : (word_type{1} << width) - 1};

        static inline constexpr value_type unpack(const word_type* data,
                                                  std::size_t index) noexcept {
            if constexpr (width == digits) {
                return bit_packer<word_type, value_type, 1, Endian, pack_mode::none>::unpack(data,
                                                                                             index);
            } else if constexpr (digits % width == 0) {
                return bit_packer<word_type, value_type, Width, Endian, pack_mode::sparse>::unpack(
                    data, index);
            } else {
                const std::size_t s = (index * width) / digits;
                const std::size_t sb = (index * width) % digits;
                const std::size_t e = s + (sb + width > digits ? 1 : 0);
                return traits::from(sc<typename traits::packed_type>(
                    detail::extract_bits<Endian>(data, s, e, sb, width, digits, mask)));
            }
        }

        template <bool safe = true>
        static inline constexpr void pack(word_type* data, std::size_t index,
                                          value_type value) noexcept {
            if constexpr (width == digits) {
                bit_packer<word_type, value_type, 1, Endian, pack_mode::none>::template pack<safe>(
                    data, index, value);
            } else if constexpr (digits % width == 0) {
                bit_packer<word_type, value_type, Width, Endian,
                           pack_mode::sparse>::template pack<safe>(data, index, value);
            } else {
                const std::size_t s = (index * width) / digits;
                const std::size_t sb = (index * width) % digits;
                const std::size_t e = s + (sb + width > digits ? 1 : 0);
                const word_type v = sc<word_type>(traits::to(value)) & mask;
                detail::insert_bits<Endian, safe>(data, s, e, sb, width, digits, mask, v);
            }
        }

        static inline constexpr reference ref(word_type* data, std::size_t index) noexcept {
            return reference(data, index, Width, Endian, pack_mode::dense);
        }

        static inline constexpr const_reference cref(const word_type* data,
                                                     std::size_t index) noexcept {
            return const_reference(data, index, Width, Endian, pack_mode::dense);
        }
    };

    template <typename Word, typename Value, pack_endian Endian>
    struct bit_packer<Word, Value, 0, Endian, pack_mode::dense> {
        using word_type = Word;
        using traits = packed_traits<Value>;
        using packed_type = typename traits::packed_type;
        using value_type = typename traits::value_type;
        using reference = packed_reference<word_type, value_type, 0, Endian, pack_mode::dense>;
        using const_reference =
            const_packed_reference<word_type, value_type, 0, Endian, pack_mode::dense>;

        static constexpr std::size_t digits{std::numeric_limits<word_type>::digits};

        static inline constexpr value_type unpack(const word_type* data, std::size_t index,
                                                  std::uint8_t width) noexcept {
            assert(width > 0 && width <= digits);

            if (width == digits)
                return bit_packer<word_type, value_type, 1, Endian, pack_mode::none>::unpack(data,
                                                                                             index);
            if (digits % width == 0)
                return bit_packer<word_type, value_type, 0, Endian, pack_mode::sparse>::unpack(
                    data, index, width);

            const std::size_t s = (index * width) / digits;
            const std::size_t e = ((index * width) + width - 1) / digits;
            const std::size_t sb = (index * width) % digits;
            const word_type mask = lsb_mask<word_type>(width);

            return traits::from(sc<typename traits::packed_type>(
                detail::extract_bits<Endian>(data, s, e, sb, width, digits, mask)));
        }

        template <bool safe = true>
        static inline constexpr void pack(word_type* data, std::size_t index, value_type value,
                                          std::uint8_t width) noexcept {
            assert(width > 0 && width <= digits);

            if (width == digits) {
                bit_packer<word_type, value_type, 1, Endian, pack_mode::none>::template pack<safe>(
                    data, index, value);
                return;
            }
            if (digits % width == 0) {
                bit_packer<word_type, value_type, 0, Endian,
                           pack_mode::sparse>::template pack<safe>(data, index, value, width);
                return;
            }

            const std::size_t s = (index * width) / digits;
            const std::size_t e = ((index * width) + width - 1) / digits;
            const std::size_t sb = (index * width) % digits;
            const word_type mask = lsb_mask<word_type>(width);
            const word_type v = sc<word_type>(traits::to(value)) & mask;

            detail::insert_bits<Endian, safe>(data, s, e, sb, width, digits, mask, v);
        }

        static inline constexpr reference ref(word_type* data, std::size_t index,
                                              std::uint8_t width) noexcept {
            return reference(data, index, width, Endian, pack_mode::dense);
        }

        static inline constexpr const_reference cref(const word_type* data, std::size_t index,
                                                     std::uint8_t width) noexcept {
            return const_reference(data, index, width, Endian, pack_mode::dense);
        }

        constexpr bit_packer(std::uint8_t w) noexcept : width(w) {}

        constexpr inline value_type unpack(const word_type* data,
                                           std::size_t index) const noexcept {
            return unpack(data, index, width);
        }

        template <bool safe = true>
        constexpr inline void pack(word_type* data, std::size_t index,
                                   value_type value) const noexcept {
            pack<safe>(data, index, value, width);
        }

        constexpr inline reference ref(word_type* data, std::size_t index) const noexcept {
            return ref(data, index, width);
        }

        constexpr inline const_reference cref(const word_type* data,
                                              std::size_t index) const noexcept {
            return cref(data, index, width);
        }

        std::uint8_t width{0};
    };

    template <typename Word, typename Value, std::uint8_t Width>
    struct bit_packer<Word, Value, Width, pack_endian::rt, pack_mode::sparse> {
        using word_type = Word;
        using traits = packed_traits<Value>;
        using packed_type = typename traits::packed_type;
        using value_type = typename traits::value_type;
        using reference =
            packed_reference<word_type, value_type, Width, pack_endian::rt, pack_mode::sparse>;
        using const_reference = const_packed_reference<word_type, value_type, Width,
                                                       pack_endian::rt, pack_mode::sparse>;

        template <pack_endian E>
        using bp = bit_packer<word_type, value_type, Width, E, pack_mode::sparse>;

        static inline constexpr value_type unpack(const word_type* data, std::size_t index,
                                                  pack_endian type) noexcept {
            switch (type) {
                case pack_endian::msb: return bp<pack_endian::msb>::unpack(data, index);
                case pack_endian::lsb: return bp<pack_endian::lsb>::unpack(data, index);
                default: std::unreachable();
            }
        }

        template <bool safe = true>
        static inline constexpr void pack(word_type* data, std::size_t index, value_type value,
                                          pack_endian type) noexcept {
            switch (type) {
                case pack_endian::msb:
                    bp<pack_endian::msb>::template pack<safe>(data, index, value);
                    break;
                case pack_endian::lsb:
                    bp<pack_endian::lsb>::template pack<safe>(data, index, value);
                    break;
                default: std::unreachable();
            }
        }

        static inline constexpr reference ref(word_type* data, std::size_t index,
                                              pack_endian type) noexcept {
            return reference(data, index, Width, type, pack_mode::sparse);
        }

        static inline constexpr const_reference cref(const word_type* data, std::size_t index,
                                                     pack_endian type) noexcept {
            return const_reference(data, index, Width, type, pack_mode::sparse);
        }
    };

    template <typename Word, typename Value>
    struct bit_packer<Word, Value, 0, pack_endian::rt, pack_mode::sparse> {
        using word_type = Word;
        using traits = packed_traits<Value>;
        using packed_type = typename traits::packed_type;
        using value_type = typename traits::value_type;
        using reference =
            packed_reference<word_type, value_type, 0, pack_endian::rt, pack_mode::sparse>;
        using const_reference =
            const_packed_reference<word_type, value_type, 0, pack_endian::rt, pack_mode::sparse>;

        template <pack_endian E>
        using bp = bit_packer<word_type, value_type, 0, E, pack_mode::sparse>;

        static inline constexpr value_type unpack(const word_type* data, std::size_t index,
                                                  std::uint8_t width, pack_endian type) noexcept {
            switch (type) {
                case pack_endian::msb: return bp<pack_endian::msb>::unpack(data, index, width);
                case pack_endian::lsb: return bp<pack_endian::lsb>::unpack(data, index, width);
                default: std::unreachable();
            }
        }

        template <bool safe = true>
        static inline constexpr void pack(word_type* data, std::size_t index, value_type value,
                                          std::uint8_t width, pack_endian type) noexcept {
            switch (type) {
                case pack_endian::msb:
                    bp<pack_endian::msb>::template pack<safe>(data, index, value, width);
                    break;
                case pack_endian::lsb:
                    bp<pack_endian::lsb>::template pack<safe>(data, index, value, width);
                    break;
                default: std::unreachable();
            }
        }

        static inline constexpr reference ref(word_type* data, std::size_t index,
                                              std::uint8_t width, pack_endian type) noexcept {
            return reference(data, index, width, type, pack_mode::sparse);
        }

        static inline constexpr const_reference cref(const word_type* data, std::size_t index,
                                                     std::uint8_t width,
                                                     pack_endian type) noexcept {
            return const_reference(data, index, width, type, pack_mode::sparse);
        }
    };

    template <typename Word, typename Value, std::uint8_t Width>
    struct bit_packer<Word, Value, Width, pack_endian::rt, pack_mode::dense> {
        using word_type = Word;
        using traits = packed_traits<Value>;
        using packed_type = typename traits::packed_type;
        using value_type = typename traits::value_type;
        using reference =
            packed_reference<word_type, value_type, Width, pack_endian::rt, pack_mode::dense>;
        using const_reference =
            const_packed_reference<word_type, value_type, Width, pack_endian::rt, pack_mode::dense>;

        template <pack_endian E>
        using bp = bit_packer<word_type, value_type, Width, E, pack_mode::dense>;

        static inline constexpr value_type unpack(const word_type* data, std::size_t index,
                                                  pack_endian type) noexcept {
            switch (type) {
                case pack_endian::msb: return bp<pack_endian::msb>::unpack(data, index);
                case pack_endian::lsb: return bp<pack_endian::lsb>::unpack(data, index);
                default: std::unreachable();
            }
        }

        template <bool safe = true>
        static inline constexpr void pack(word_type* data, std::size_t index, value_type value,
                                          pack_endian type) noexcept {
            switch (type) {
                case pack_endian::msb:
                    bp<pack_endian::msb>::template pack<safe>(data, index, value);
                    break;
                case pack_endian::lsb:
                    bp<pack_endian::lsb>::template pack<safe>(data, index, value);
                    break;
                default: std::unreachable();
            }
        }

        static inline constexpr reference ref(word_type* data, std::size_t index,
                                              pack_endian type) noexcept {
            return reference(data, index, Width, type, pack_mode::dense);
        }

        static inline constexpr const_reference cref(const word_type* data, std::size_t index,
                                                     pack_endian type) noexcept {
            return const_reference(data, index, Width, type, pack_mode::dense);
        }
    };

    template <typename Word, typename Value>
    struct bit_packer<Word, Value, 0, pack_endian::rt, pack_mode::dense> {
        using word_type = Word;
        using traits = packed_traits<Value>;
        using packed_type = typename traits::packed_type;
        using value_type = typename traits::value_type;
        using reference =
            packed_reference<word_type, value_type, 0, pack_endian::rt, pack_mode::dense>;
        using const_reference =
            const_packed_reference<word_type, value_type, 0, pack_endian::rt, pack_mode::dense>;

        template <pack_endian E>
        using bp = bit_packer<word_type, value_type, 0, E, pack_mode::dense>;

        static inline constexpr value_type unpack(const word_type* data, std::size_t index,
                                                  std::uint8_t width, pack_endian type) noexcept {
            switch (type) {
                case pack_endian::msb: return bp<pack_endian::msb>::unpack(data, index, width);
                case pack_endian::lsb: return bp<pack_endian::lsb>::unpack(data, index, width);
                default: std::unreachable();
            }
        }

        template <bool safe = true>
        static inline constexpr void pack(word_type* data, std::size_t index, value_type value,
                                          std::uint8_t width, pack_endian type) noexcept {
            switch (type) {
                case pack_endian::msb:
                    bp<pack_endian::msb>::template pack<safe>(data, index, value, width);
                    break;
                case pack_endian::lsb:
                    bp<pack_endian::lsb>::template pack<safe>(data, index, value, width);
                    break;
                default: std::unreachable();
            }
        }

        static inline constexpr reference ref(word_type* data, std::size_t index,
                                              std::uint8_t width, pack_endian type) noexcept {
            return reference(data, index, width, type, pack_mode::dense);
        }

        static inline constexpr const_reference cref(const word_type* data, std::size_t index,
                                                     std::uint8_t width,
                                                     pack_endian type) noexcept {
            return const_reference(data, index, width, type, pack_mode::dense);
        }
    };

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian>
    struct bit_packer<Word, Value, Width, Endian, pack_mode::rt> {
        using word_type = Word;
        using traits = packed_traits<Value>;
        using packed_type = typename traits::packed_type;
        using value_type = typename traits::value_type;
        using reference = packed_reference<word_type, value_type, Width, Endian, pack_mode::rt>;
        using const_reference =
            const_packed_reference<word_type, value_type, Width, Endian, pack_mode::rt>;

        template <pack_mode M> using bp = bit_packer<word_type, value_type, Width, Endian, M>;

        static inline constexpr value_type unpack(const word_type* data, std::size_t index,
                                                  pack_mode mode) noexcept {
            switch (mode) {
                case pack_mode::none: return bp<pack_mode::none>::unpack(data, index);
                case pack_mode::sparse: return bp<pack_mode::sparse>::unpack(data, index);
                case pack_mode::dense: return bp<pack_mode::dense>::unpack(data, index);
                default: std::unreachable();
            }
        }

        template <bool safe = true>
        static inline constexpr void pack(word_type* data, std::size_t index, value_type value,
                                          pack_mode mode) noexcept {
            switch (mode) {
                case pack_mode::none:
                    bp<pack_mode::none>::template pack<safe>(data, index, value);
                    break;
                case pack_mode::sparse:
                    bp<pack_mode::sparse>::template pack<safe>(data, index, value);
                    break;
                case pack_mode::dense:
                    bp<pack_mode::dense>::template pack<safe>(data, index, value);
                    break;
                default: std::unreachable();
            }
        }

        static inline constexpr reference ref(word_type* data, std::size_t index,
                                              pack_mode mode) noexcept {
            return reference(data, index, Width, Endian, mode);
        }

        static inline constexpr const_reference cref(const word_type* data, std::size_t index,
                                                     pack_mode mode) noexcept {
            return const_reference(data, index, Width, Endian, mode);
        }
    };

    template <typename Word, typename Value, pack_endian Endian>
    struct bit_packer<Word, Value, 0, Endian, pack_mode::rt> {
        using word_type = Word;
        using traits = packed_traits<Value>;
        using packed_type = typename traits::packed_type;
        using value_type = typename traits::value_type;
        using reference = packed_reference<word_type, value_type, 0, Endian, pack_mode::rt>;
        using const_reference =
            const_packed_reference<word_type, value_type, 0, Endian, pack_mode::rt>;

        static inline constexpr value_type unpack(const word_type* data, std::size_t index,
                                                  std::uint8_t width, pack_mode mode) noexcept {
            switch (mode) {
                case pack_mode::none:
                    return bit_packer<word_type, value_type, 0, Endian, pack_mode::none>::unpack(
                        data, index);
                case pack_mode::sparse:
                    return bit_packer<word_type, value_type, 0, Endian, pack_mode::sparse>::unpack(
                        data, index, width);
                case pack_mode::dense:
                    return bit_packer<word_type, value_type, 0, Endian, pack_mode::dense>::unpack(
                        data, index, width);
                default: std::unreachable();
            }
        }

        template <bool safe = true>
        static inline constexpr void pack(word_type* data, std::size_t index, value_type value,
                                          std::uint8_t width, pack_mode mode) noexcept {
            switch (mode) {
                case pack_mode::none:
                    bit_packer<word_type, value_type, 0, Endian,
                               pack_mode::none>::template pack<safe>(data, index, value);
                    break;
                case pack_mode::sparse:
                    bit_packer<word_type, value_type, 0, Endian,
                               pack_mode::sparse>::template pack<safe>(data, index, value, width);
                    break;
                case pack_mode::dense:
                    bit_packer<word_type, value_type, 0, Endian,
                               pack_mode::dense>::template pack<safe>(data, index, value, width);
                    break;
                default: std::unreachable();
            }
        }

        static inline constexpr reference ref(word_type* data, std::size_t index,
                                              std::uint8_t width, pack_mode mode) noexcept {
            return reference(data, index, width, Endian, mode);
        }

        static inline constexpr const_reference cref(const word_type* data, std::size_t index,
                                                     std::uint8_t width, pack_mode mode) noexcept {
            return const_reference(data, index, width, Endian, mode);
        }
    };

    template <typename Word, typename Value, std::uint8_t Width>
    struct bit_packer<Word, Value, Width, pack_endian::rt, pack_mode::rt> {
        using word_type = Word;
        using traits = packed_traits<Value>;
        using packed_type = typename traits::packed_type;
        using value_type = typename traits::value_type;
        using reference =
            packed_reference<word_type, value_type, Width, pack_endian::rt, pack_mode::rt>;
        using const_reference =
            const_packed_reference<word_type, value_type, Width, pack_endian::rt, pack_mode::rt>;

        template <pack_endian E>
        using bp = bit_packer<word_type, value_type, Width, E, pack_mode::rt>;

        static inline constexpr value_type unpack(const word_type* data, std::size_t index,
                                                  pack_endian type, pack_mode mode) noexcept {
            switch (type) {
                case pack_endian::msb: return bp<pack_endian::msb>::unpack(data, index, mode);
                case pack_endian::lsb: return bp<pack_endian::lsb>::unpack(data, index, mode);
                default: std::unreachable();
            }
        }

        template <bool safe = true>
        static inline constexpr void pack(word_type* data, std::size_t index, value_type value,
                                          pack_endian type, pack_mode mode) noexcept {
            switch (type) {
                case pack_endian::msb:
                    bp<pack_endian::msb>::template pack<safe>(data, index, value, mode);
                    break;
                case pack_endian::lsb:
                    bp<pack_endian::lsb>::template pack<safe>(data, index, value, mode);
                    break;
                default: std::unreachable();
            }
        }

        static inline constexpr reference ref(word_type* data, std::size_t index, pack_endian type,
                                              pack_mode mode) noexcept {
            return reference(data, index, Width, type, mode);
        }

        static inline constexpr const_reference cref(const word_type* data, std::size_t index,
                                                     pack_endian type, pack_mode mode) noexcept {
            return const_reference(data, index, Width, type, mode);
        }
    };

    template <typename Word, typename Value>
    struct bit_packer<Word, Value, 0, pack_endian::rt, pack_mode::rt> {
        using word_type = Word;
        using traits = packed_traits<Value>;
        using packed_type = typename traits::packed_type;
        using value_type = typename traits::value_type;
        using reference =
            packed_reference<word_type, value_type, 0, pack_endian::rt, pack_mode::rt>;
        using const_reference =
            const_packed_reference<word_type, value_type, 0, pack_endian::rt, pack_mode::rt>;

        template <pack_endian E> using bp = bit_packer<word_type, value_type, 0, E, pack_mode::rt>;

        static inline constexpr value_type unpack(const word_type* data, std::size_t index,
                                                  std::uint8_t width, pack_endian type,
                                                  pack_mode mode) noexcept {
            switch (type) {
                case pack_endian::msb:
                    return bp<pack_endian::msb>::unpack(data, index, width, mode);
                case pack_endian::lsb:
                    return bp<pack_endian::lsb>::unpack(data, index, width, mode);
                default: std::unreachable();
            }
        }

        template <bool safe = true>
        static inline constexpr void pack(word_type* data, std::size_t index, value_type value,
                                          std::uint8_t width, pack_endian type,
                                          pack_mode mode) noexcept {
            switch (type) {
                case pack_endian::msb:
                    bp<pack_endian::msb>::template pack<safe>(data, index, value, width, mode);
                    break;
                case pack_endian::lsb:
                    bp<pack_endian::lsb>::template pack<safe>(data, index, value, width, mode);
                    break;
                default: std::unreachable();
            }
        }

        static inline constexpr reference ref(word_type* data, std::size_t index,
                                              std::uint8_t width, pack_endian type,
                                              pack_mode mode) noexcept {
            return reference(data, index, width, type, mode);
        }

        static inline constexpr const_reference cref(const word_type* data, std::size_t index,
                                                     std::uint8_t width, pack_endian type,
                                                     pack_mode mode) noexcept {
            return const_reference(data, index, width, type, mode);
        }
    };

    template <typename Word, typename Value, std::uint8_t Width>
    using bp_lsb_dense = bit_packer<Word, Value, Width, pack_endian::lsb, pack_mode::dense>;

    template <typename Word, typename Value, std::uint8_t Width>
    using bp_lsb_sparse = bit_packer<Word, Value, Width, pack_endian::lsb, pack_mode::sparse>;

    template <typename Word, typename Value, std::uint8_t Width>
    using bp_msb_dense = bit_packer<Word, Value, Width, pack_endian::msb, pack_mode::dense>;

    template <typename Word, typename Value, std::uint8_t Width>
    using bp_msb_sparse = bit_packer<Word, Value, Width, pack_endian::msb, pack_mode::sparse>;

    template <typename Word, typename Value> using bp_wrt_lsb_dense = bp_lsb_dense<Word, Value, 0>;

    template <typename Word, typename Value>
    using bp_wrt_lsb_sparse = bp_lsb_sparse<Word, Value, 0>;

    template <typename Word, typename Value> using bp_wrt_msb_dense = bp_msb_dense<Word, Value, 0>;

    template <typename Word, typename Value>
    using bp_wrt_msb_sparse = bp_msb_sparse<Word, Value, 0>;

    template <typename Word, typename Value, std::uint8_t Width>
    using bp_rt_dense = bit_packer<Word, Value, Width, pack_endian::rt, pack_mode::dense>;

    template <typename Word, typename Value, std::uint8_t Width>
    using bp_rt_sparse = bit_packer<Word, Value, Width, pack_endian::rt, pack_mode::sparse>;

    template <typename Word, typename Value, std::uint8_t Width>
    using bp_lsb_rt = bit_packer<Word, Value, Width, pack_endian::lsb, pack_mode::rt>;

    template <typename Word, typename Value, std::uint8_t Width>
    using bp_msb_rt = bit_packer<Word, Value, Width, pack_endian::msb, pack_mode::rt>;

    template <typename Word, typename Value, std::uint8_t Width>
    using bp_rt_rt = bit_packer<Word, Value, Width, pack_endian::rt, pack_mode::rt>;

    template <typename Word, typename Value> using bp_wrt_rt_dense = bp_rt_dense<Word, Value, 0>;

    template <typename Word, typename Value> using bp_wrt_rt_sparse = bp_rt_sparse<Word, Value, 0>;

    template <typename Word, typename Value> using bp_wrt_lsb_rt = bp_lsb_rt<Word, Value, 0>;

    template <typename Word, typename Value> using bp_wrt_msb_rt = bp_msb_rt<Word, Value, 0>;

    template <typename Word, typename Value> using bp_wrt_rt_rt = bp_rt_rt<Word, Value, 0>;

} // namespace cds
