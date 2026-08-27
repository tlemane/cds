#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include <doctest.h>

#include <cds/bit/view.hpp>

#include "bit_check.hpp"

namespace {

    template <typename Word, cds::pack_endian Endian>
    void run_bit_view_case(std::size_t n, double density, std::uint64_t seed) {
        using view_t = cds::bit_view<Word, Endian>;
        using cview_t = cds::const_bit_view<Word, Endian>;
        static_assert(cds::bit_source<view_t>, "bit_view must satisfy bit_source");
        static_assert(cds::bit_source<cview_t>, "const_bit_view must satisfy bit_source");

        constexpr std::size_t digits = std::numeric_limits<Word>::digits;
        const auto bits = make_bits(n, density, seed);

        std::vector<Word> buf(n / digits + 2, Word{0});

        view_t v(buf.data(), n);
        CHECK(v.size() == n);
        CHECK(v.offset() == 0);
        for (std::size_t i = 0; i < n; ++i)
            if (bits[i])
                v.set(i);

        check_bits_equal(v, bits);
        CHECK(v.popcount() == popcount_of(bits));
        if (n > 0) {
            CHECK(static_cast<std::uint8_t>(v.front()) == bits.front());
            CHECK(static_cast<std::uint8_t>(v.back()) == bits.back());
        }

        cview_t cv(buf.data(), n);
        CHECK(cv.size() == n);
        CHECK(cv.offset() == 0);
        check_bits_equal(cv, bits);

        if (n > 0) {
            view_t v2(buf.data(), n);
            const bool orig = (bits[0] != 0);
            v2.set(0, !orig);
            CHECK(v.get(0) == !orig);
            v2.set(0, orig);
            CHECK(v.get(0) == orig);
        }

        if (n >= 4) {
            auto sub = v.subview(2);
            REQUIRE(sub.size() == n - 2);
            for (std::size_t i = 0; i < sub.size(); ++i) {
                CAPTURE(i);
                CHECK(static_cast<std::uint8_t>(sub[i]) == bits[i + 2]);
            }
        }
    }

    template <typename Word>
    void run_bit_view_all_layouts(std::size_t n, double density, std::uint64_t seed) {
        using cds::pack_endian;
        using cds::pack_mode;

        run_bit_view_case<Word, pack_endian::lsb>(n, density, seed + 1);
        run_bit_view_case<Word, pack_endian::msb>(n, density, seed + 3);
    }

    template <typename Word> void run_bit_view_word_suite(std::uint64_t seed_base) {
        constexpr std::size_t digits = std::numeric_limits<Word>::digits;

        run_bit_view_all_layouts<Word>(0, 0.5, seed_base + 1);
        run_bit_view_all_layouts<Word>(1, 0.5, seed_base + 2);
        run_bit_view_all_layouts<Word>(digits - 1, 0.5, seed_base + 3);
        run_bit_view_all_layouts<Word>(digits, 0.5, seed_base + 4);
        run_bit_view_all_layouts<Word>(digits + 1, 0.5, seed_base + 5);
        run_bit_view_all_layouts<Word>(digits * 3 + 7, 0.5, seed_base + 6);
        run_bit_view_all_layouts<Word>(200, 0.02, seed_base + 7);
        run_bit_view_all_layouts<Word>(200, 0.5, seed_base + 8);
        run_bit_view_all_layouts<Word>(200, 0.98, seed_base + 9);
    }

}

TEST_CASE("bit/bit_view") {
    SUBCASE("Word = uint64_t") {
        run_bit_view_word_suite<std::uint64_t>(38000);
    }
    SUBCASE("Word = uint32_t") {
        run_bit_view_word_suite<std::uint32_t>(39000);
    }
    SUBCASE("Word = uint16_t") {
        run_bit_view_word_suite<std::uint16_t>(40000);
    }
    SUBCASE("Word = uint8_t") {
        run_bit_view_word_suite<std::uint8_t>(41000);
    }
}
