#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <doctest.h>

#include <cds/core/broadword.hpp>

namespace {

    template <typename Word>
    [[nodiscard]] std::size_t oracle_popcount_below_lsb(Word w, std::size_t i) noexcept {
        std::size_t c = 0;
        for (std::size_t b = 0; b < i; ++b)
            c += ((w >> b) & Word{1}) ? std::size_t{1} : std::size_t{0};
        return c;
    }

    template <typename Word>
    [[nodiscard]] std::size_t oracle_popcount_below_msb(Word w, std::size_t i,
                                                        std::size_t d) noexcept {
        std::size_t c = 0;
        for (std::size_t b = d - i; b < d; ++b)
            c += ((w >> b) & Word{1}) ? std::size_t{1} : std::size_t{0};
        return c;
    }

    template <typename Word> [[nodiscard]] Word oracle_reverse(Word w, std::size_t d) noexcept {
        Word r = 0;
        for (std::size_t b = 0; b < d; ++b)
            if ((w >> b) & Word{1})
                r = static_cast<Word>(r | (Word{1} << (d - 1 - b)));
        return r;
    }

    template <typename Word>
    [[nodiscard]] std::size_t oracle_select_lsb(Word w, std::size_t r, std::size_t d) noexcept {
        for (std::size_t b = 0; b < d; ++b)
            if ((w >> b) & Word{1}) {
                if (r == 0)
                    return b;
                --r;
            }
        return d;
    }

    template <typename Word>
    [[nodiscard]] std::size_t oracle_select_msb(Word w, std::size_t r, std::size_t d) noexcept {
        for (std::size_t k = 0; k < d; ++k) {
            const std::size_t b = d - 1 - k;
            if ((w >> b) & Word{1}) {
                if (r == 0)
                    return k;
                --r;
            }
        }
        return d;
    }

    template <typename Word> [[nodiscard]] std::vector<Word> broadword_words() {
        constexpr std::size_t d = std::numeric_limits<Word>::digits;
        std::vector<Word> ws = {
            Word{0},
            static_cast<Word>(~Word{0}),
            Word{1},
            static_cast<Word>(Word{1} << (d - 1)),
            static_cast<Word>(0x5555555555555555ULL),
            static_cast<Word>(0xAAAAAAAAAAAAAAAAULL),
            static_cast<Word>(0x0F0F0F0F0F0F0F0FULL),
            static_cast<Word>(0xF0F0F0F0F0F0F0F0ULL),
        };
        std::uint64_t s = 0x123456789abcdefULL;
        for (int i = 0; i < 8; ++i) {
            s = s * 6364136223846793005ULL + 1442695040888963407ULL;
            ws.push_back(static_cast<Word>(s >> 11));
        }
        return ws;
    }

    template <typename Word> void run_broadword_word_suite() {
        using cds::pack_endian;
        namespace bw = cds::broadword;
        constexpr std::size_t d = std::numeric_limits<Word>::digits;

        CHECK(bw::word_digits<Word> == d);

        for (Word w : broadword_words<Word>()) {
            CAPTURE(static_cast<std::uint64_t>(w));

            CHECK(bw::popcount(w) == static_cast<std::size_t>(std::popcount(w)));

            for (std::size_t i = 0; i <= d; ++i) {
                CAPTURE(i);
                CHECK(bw::template popcount_below<pack_endian::lsb>(w, i) ==
                      oracle_popcount_below_lsb(w, i));
                CHECK(bw::template popcount_below<pack_endian::msb>(w, i) ==
                      oracle_popcount_below_msb(w, i, d));
            }

            const Word rev = bw::reverse_bits(w);
            CHECK(rev == oracle_reverse(w, d));
            CHECK(bw::reverse_bits(rev) == w);

            const std::size_t pc = bw::popcount(w);
            for (std::size_t r = 0; r < pc; ++r) {
                CAPTURE(r);
                CHECK(bw::template select_in_word<pack_endian::lsb>(w, r) ==
                      oracle_select_lsb(w, r, d));
                CHECK(bw::template select_in_word<pack_endian::msb>(w, r) ==
                      oracle_select_msb(w, r, d));
            }
        }
    }

    namespace bw = cds::broadword;
    using cds::pack_endian;

    static_assert(bw::word_digits<std::uint64_t> == 64);
    static_assert(bw::word_digits<std::uint8_t> == 8);

    static_assert(bw::popcount_below<pack_endian::lsb>(std::uint64_t{0b1010}, 3) == 1);
    static_assert(bw::popcount_below<pack_endian::lsb>(std::uint64_t{0b1010}, 4) == 2);
    static_assert(bw::popcount_below<pack_endian::msb>(std::uint64_t{0b1010}, 64) == 2);

    static_assert(bw::reverse_bits(std::uint64_t{1}) == (std::uint64_t{1} << 63));
    static_assert(bw::reverse_bits(bw::reverse_bits(std::uint64_t{0x123456789abcdefULL})) ==
                  0x123456789abcdefULL);

    static_assert(bw::select_in_word<pack_endian::lsb>(std::uint64_t{0b1010}, 0) == 1);
    static_assert(bw::select_in_word<pack_endian::lsb>(std::uint64_t{0b1010}, 1) == 3);
    static_assert(bw::select_in_word<pack_endian::lsb>(std::uint32_t{0b1010}, 1) == 3);
    static_assert(bw::select_in_word<pack_endian::msb>(std::uint64_t{1} << 63, 0) == 0);

}

TEST_CASE("core/broadword") {
    SUBCASE("Word = uint64_t") {
        run_broadword_word_suite<std::uint64_t>();
    }
    SUBCASE("Word = uint32_t") {
        run_broadword_word_suite<std::uint32_t>();
    }
    SUBCASE("Word = uint16_t") {
        run_broadword_word_suite<std::uint16_t>();
    }
    SUBCASE("Word = uint8_t") {
        run_broadword_word_suite<std::uint8_t>();
    }
}
