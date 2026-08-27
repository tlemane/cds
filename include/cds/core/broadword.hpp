#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <cds/core/common.hpp>
#include <cds/core/packed/type.hpp>

#if defined(__BMI2__)
#define CDS_HAS_BMI2 1
#include <immintrin.h>
#else
#define CDS_HAS_BMI2 0
#endif

namespace cds::broadword {

    static constexpr std::size_t digits = 64;

    // number of set bits among the first`i` bits of `w`, counted from the Endian side
    template <pack_endian Endian>
        requires(Endian == pack_endian::lsb || Endian == pack_endian::msb)
    [[nodiscard]] constexpr std::size_t popcount_below(std::uint64_t w, std::size_t i) noexcept {
        if (i >= digits)
            return static_cast<std::size_t>(std::popcount(w));

        if (i == 0)
            return 0;

        if constexpr (Endian == pack_endian::lsb)
            return static_cast<std::size_t>(std::popcount(w & lsb_mask<std::uint64_t>(i)));
        else
            return static_cast<std::size_t>(std::popcount(w & msb_mask<std::uint64_t>(i)));
    }

    // Precondition: 0 <= i < 64.
    template <pack_endian Endian>
        requires(Endian == pack_endian::lsb || Endian == pack_endian::msb)
    [[nodiscard]] constexpr std::size_t popcount_prefix(std::uint64_t w, std::size_t i) noexcept {
        if constexpr (Endian == pack_endian::lsb) {
            // i in [0, 63]: (1 << i) - 1 is the low-i-bits mask (0 when i == 0).
            const std::uint64_t mask = (std::uint64_t{1} << i) - 1;
            return static_cast<std::size_t>(std::popcount(w & mask));
        } else {
            const std::uint64_t nz = -static_cast<std::uint64_t>(i != 0);
            const std::uint64_t mask = (~std::uint64_t{0} << ((64 - i) & 63)) & nz;
            return static_cast<std::size_t>(std::popcount(w & mask));
        }
    }

    [[nodiscard]] constexpr std::size_t popcount(std::uint64_t w) noexcept {
        return static_cast<std::size_t>(std::popcount(w));
    }

    [[nodiscard]] constexpr std::uint64_t reverse_bits(std::uint64_t w) noexcept {
        w = ((w >> 1) & 0x5555555555555555ULL) | ((w & 0x5555555555555555ULL) << 1);
        w = ((w >> 2) & 0x3333333333333333ULL) | ((w & 0x3333333333333333ULL) << 2);
        w = ((w >> 4) & 0x0F0F0F0F0F0F0F0FULL) | ((w & 0x0F0F0F0F0F0F0F0FULL) << 4);
        return std::byteswap(w);
    }

    namespace detail {

        // From https://graphics.stanford.edu/~seander/bithacks.html#SelectPosFromMSBRank
        [[nodiscard]] constexpr std::size_t select_in_word_lsb_portable(std::uint64_t w,
                                                                        std::size_t r) noexcept {
            constexpr std::uint64_t ones = ~std::uint64_t{0};

            const std::uint64_t count2 = w - ((w >> 1) & (ones / 3));
            const std::uint64_t count4 = (count2 & (ones / 5)) + ((count2 >> 2) & (ones / 5));
            const std::uint64_t count8 = (count4 + (count4 >> 4)) & (ones / 0x11);
            const std::uint64_t count16 = (count8 + (count8 >> 8)) & (ones / 0x101);

            std::uint64_t need =
                static_cast<std::uint64_t>(std::popcount(w)) - static_cast<std::uint64_t>(r);
            std::uint64_t pos = 64;
            std::uint64_t chunk;

            chunk = (count16 >> 32) + (count16 >> 48);
            pos -= ((chunk - need) & 256) >> 3;
            need -= chunk & ((chunk - need) >> 8);

            chunk = (count16 >> (pos - 16)) & 0xff;
            pos -= ((chunk - need) & 256) >> 4;
            need -= chunk & ((chunk - need) >> 8);

            chunk = (count8 >> (pos - 8)) & 0xf;
            pos -= ((chunk - need) & 256) >> 5;
            need -= chunk & ((chunk - need) >> 8);

            chunk = (count4 >> (pos - 4)) & 0x7;
            pos -= ((chunk - need) & 256) >> 6;
            need -= chunk & ((chunk - need) >> 8);

            chunk = (count2 >> (pos - 2)) & 0x3;
            pos -= ((chunk - need) & 256) >> 7;
            need -= chunk & ((chunk - need) >> 8);

            chunk = (w >> (pos - 1)) & 0x1;
            pos -= ((chunk - need) & 256) >> 8;

            return static_cast<std::size_t>(pos - 1);
        }

    } // namespace detail

    [[nodiscard]] constexpr std::size_t select_in_word_lsb(std::uint64_t w,
                                                           std::size_t r) noexcept {
        if consteval {
            return detail::select_in_word_lsb_portable(w, r);
        } else {
#if CDS_HAS_BMI2
            return static_cast<std::size_t>(std::countr_zero(_pdep_u64(std::uint64_t{1} << r, w)));
#else
            return detail::select_in_word_lsb_portable(w, r);
#endif
        }
    }

    // position of the r-th set bit (r 0-indexed), counted from the Endian side.
    template <pack_endian Endian>
        requires(Endian == pack_endian::lsb || Endian == pack_endian::msb)
    [[nodiscard]] constexpr std::size_t select_in_word(std::uint64_t w, std::size_t r) noexcept {
        if constexpr (Endian == pack_endian::lsb)
            return select_in_word_lsb(w, r);
        else
            return select_in_word_lsb(reverse_bits(w), r);
    }

    template <typename Word>
    inline constexpr std::size_t word_digits = std::numeric_limits<Word>::digits;

    template <pack_endian Endian, typename Word>
        requires(Endian == pack_endian::lsb || Endian == pack_endian::msb) &&
                (!std::same_as<Word, std::uint64_t>)
    [[nodiscard]] constexpr std::size_t popcount_below(Word w, std::size_t i) noexcept {
        constexpr std::size_t d = word_digits<Word>;
        if (i >= d)
            return static_cast<std::size_t>(std::popcount(w));

        if (i == 0)
            return 0;

        if constexpr (Endian == pack_endian::lsb) {
            const Word mask = static_cast<Word>((Word{1} << i) - 1);
            return static_cast<std::size_t>(std::popcount(static_cast<Word>(w & mask)));
        } else {
            const Word mask = static_cast<Word>(~((Word{1} << (d - i)) - 1));
            return static_cast<std::size_t>(std::popcount(static_cast<Word>(w & mask)));
        }
    }

    template <typename Word>
        requires(!std::same_as<Word, std::uint64_t>)
    [[nodiscard]] constexpr std::size_t popcount(Word w) noexcept {
        return static_cast<std::size_t>(std::popcount(w));
    }

    template <typename Word>
        requires(!std::same_as<Word, std::uint64_t>)
    [[nodiscard]] constexpr Word reverse_bits(Word w) noexcept {
        constexpr std::size_t d = word_digits<Word>;
        Word r = 0;
        for (std::size_t i = 0; i < d; ++i) {
            r = static_cast<Word>((r << 1) | (w & Word{1}));
            w = static_cast<Word>(w >> 1);
        }
        return r;
    }

    template <typename Word>
        requires(!std::same_as<Word, std::uint64_t>)
    [[nodiscard]] constexpr std::size_t select_in_word_lsb_scan(Word w, std::size_t r) noexcept {
        constexpr std::size_t d = word_digits<Word>;
        for (std::size_t i = 0; i < d; ++i) {
            if ((w >> i) & Word{1}) {
                if (r == 0)
                    return i;
                --r;
            }
        }
        return d;
    }

#if CDS_HAS_BMI2
    [[nodiscard]] constexpr std::size_t select_in_word_lsb_u32_pdep(std::uint32_t w,
                                                                    std::size_t r) noexcept {
        return static_cast<std::size_t>(std::countr_zero(_pdep_u32(std::uint32_t{1} << r, w)));
    }
#endif

    // Precondition: w has at least r + 1 bits set (r is 0-indexed).
    template <pack_endian Endian, typename Word>
        requires(Endian == pack_endian::lsb || Endian == pack_endian::msb) &&
                (!std::same_as<Word, std::uint64_t>)
    [[nodiscard]] constexpr std::size_t select_in_word(Word w, std::size_t r) noexcept {
        if constexpr (std::same_as<Word, std::uint32_t>) {
            if consteval {
                if constexpr (Endian == pack_endian::lsb)
                    return select_in_word_lsb_scan(w, r);
                else
                    return select_in_word_lsb_scan(reverse_bits(w), r);
            } else {
#if CDS_HAS_BMI2
                if constexpr (Endian == pack_endian::lsb)
                    return select_in_word_lsb_u32_pdep(w, r);
                else
                    return select_in_word_lsb_u32_pdep(reverse_bits(w), r);
#else
                if constexpr (Endian == pack_endian::lsb)
                    return select_in_word_lsb_scan(w, r);
                else
                    return select_in_word_lsb_scan(reverse_bits(w), r);
#endif
            }
        } else {
            if constexpr (Endian == pack_endian::lsb)
                return select_in_word_lsb_scan(w, r);
            else
                return select_in_word_lsb_scan(reverse_bits(w), r);
        }
    }

} // namespace cds::broadword
