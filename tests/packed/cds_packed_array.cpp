#include <doctest.h>

#include <algorithm>
#include <iterator>
#include <utility>

#include <cds/packed/array.hpp>

#include "packed_check.hpp"

using namespace cds;

using arr_t = packed_array<word_t, value_t, 4, 4, pack_endian::lsb, pack_mode::sparse>;

TEST_CASE("core/packed/array") {

    SUBCASE("fixed width / sparse / lsb — push_back, size, front/back") {
        using arr8 = packed_array<word_t, value_t, 8, 4, pack_endian::lsb, pack_mode::sparse>;

        arr8 a;

        CHECK(a.capacity() == 8);
        CHECK(a.empty());
        CHECK(a.size() == 0);

        for (value_t v : {value_t{0x1}, value_t{0x2}, value_t{0x3}, value_t{0xf}})
            a.push_back(v);

        CHECK(a.size() == 4);
        CHECK_FALSE(a.empty());
        CHECK(a.front() == 0x1);
        CHECK(a.back() == 0xf);

        CHECK(a[0] == 0x1);
        CHECK(a[1] == 0x2);
        CHECK(a[2] == 0x3);
        CHECK(a[3] == 0xf);

        a.clear();
        CHECK(a.empty());
        CHECK(a.size() == 0);
    }

    SUBCASE("fixed width / dense / msb — write through operator[]") {
        using arr_md = packed_array<word_t, value_t, 6, 13, pack_endian::msb, pack_mode::dense>;

        arr_md a;
        for (std::size_t i = 0; i < 6; ++i)
            a.push_back(static_cast<value_t>(i * 0x111));

        for (std::size_t i = 0; i < 6; ++i)
            CHECK(a[i] == static_cast<value_t>(i * 0x111));

        a[2] = value_t{0x1ead};
        CHECK(a.size() == 6);
        CHECK(a[2] == 0x1ead);
    }

    SUBCASE("emplace_back behaves like push_back") {
        arr_t a;
        a.emplace_back(value_t{0x1});
        a.emplace_back(value_t{0x2});

        CHECK(a.size() == 2);
        CHECK(a[0] == 0x1);
        CHECK(a[1] == 0x2);
    }

    SUBCASE("iteration via begin/end") {
        using arr5 = packed_array<word_t, value_t, 5, 4, pack_endian::lsb, pack_mode::sparse>;

        arr5 a;
        for (value_t v : {value_t{0x1}, value_t{0x2}, value_t{0x3}, value_t{0x4}, value_t{0x5}})
            a.push_back(v);

        value_t expected = 1;
        for (auto it = a.begin(); it != a.end(); ++it, ++expected)
            CHECK(*it == expected);

        CHECK(std::distance(a.begin(), a.end()) == 5);

        expected = 1;
        for (auto v : a) {
            CHECK(v == expected);
            ++expected;
        }
    }

    SUBCASE("const iteration via cbegin/cend") {
        using arr3 = packed_array<word_t, value_t, 3, 4, pack_endian::lsb, pack_mode::sparse>;

        arr3 a;
        a.push_back(value_t{0x1});
        a.push_back(value_t{0x2});
        a.push_back(value_t{0x3});

        const arr3& ca = a;

        value_t expected = 1;
        for (auto it = ca.cbegin(); it != ca.cend(); ++it, ++expected)
            CHECK(*it == expected);

        expected = 1;
        for (auto it = ca.begin(); it != ca.end(); ++it, ++expected)
            CHECK(*it == expected);
    }

    SUBCASE("range constructor") {
        value_t values[] = {0x1, 0x2, 0x3, 0x4};
        arr_t a(std::begin(values), std::end(values));

        CHECK(a.size() == 4);
        CHECK(a[0] == 0x1);
        CHECK(a[1] == 0x2);
        CHECK(a[2] == 0x3);
        CHECK(a[3] == 0x4);
    }

    SUBCASE("initializer_list constructor") {
        arr_t a{value_t{0x1}, value_t{0x2}, value_t{0x3}, value_t{0x4}};

        CHECK(a.size() == 4);
        CHECK(a[0] == 0x1);
        CHECK(a[1] == 0x2);
        CHECK(a[2] == 0x3);
        CHECK(a[3] == 0x4);
    }

    SUBCASE("copy construction is a deep copy") {
        arr_t a{value_t{0x1}, value_t{0x2}};
        arr_t b = a;

        b[0] = value_t{0xf};
        b.push_back(value_t{0x3});

        CHECK(a.size() == 2);
        CHECK(a[0] == 0x1);
        CHECK(b.size() == 3);
        CHECK(b[0] == 0xf);
    }

    SUBCASE("copy assignment is a deep copy") {
        arr_t a{value_t{0x1}, value_t{0x2}};
        arr_t b{value_t{0x9}};

        b = a;
        b[0] = value_t{0xf};

        CHECK(a[0] == 0x1);
        CHECK(b[0] == 0xf);
        CHECK(b.size() == 2);
    }

    SUBCASE("move construction transfers state") {
        arr_t a{value_t{0x1}, value_t{0x2}, value_t{0x3}};
        arr_t b = std::move(a);

        CHECK(b.size() == 3);
        CHECK(b[0] == 0x1);
        CHECK(b[1] == 0x2);
        CHECK(b[2] == 0x3);
    }

    SUBCASE("move assignment transfers state") {
        arr_t a{value_t{0x1}, value_t{0x2}};
        arr_t b{value_t{0x9}};

        b = std::move(a);

        CHECK(b.size() == 2);
        CHECK(b[0] == 0x1);
        CHECK(b[1] == 0x2);
    }

    SUBCASE("runtime width / sparse") {
        using arr_rtw = packed_array<word_t, value_t, 2, 0, pack_endian::lsb, pack_mode::sparse>;

        constexpr std::uint8_t width = 5;
        arr_rtw a(width, pack_endian::lsb, pack_mode::sparse);

        REQUIRE(a.capacity() > 0);

        for (std::size_t i = 0; i < a.capacity(); ++i)
            a.push_back(static_cast<value_t>(i % 32));

        CHECK(a.size() == a.capacity());

        for (std::size_t i = 0; i < a.size(); ++i)
            CHECK(a[i] == static_cast<value_t>(i % 32));
    }

    SUBCASE("runtime endian + runtime mode") {
        using arr_rtem = packed_array<word_t, value_t, 4, 13, pack_endian::rt, pack_mode::rt>;

        arr_rtem a(13, pack_endian::msb, pack_mode::dense);

        for (value_t v : {value_t{0x123}, value_t{0x456}, value_t{0x789}, value_t{0xabc}})
            a.push_back(v);

        CHECK(a[0] == 0x123);
        CHECK(a[1] == 0x456);
        CHECK(a[2] == 0x789);
        CHECK(a[3] == 0xabc);
    }

    SUBCASE("runtime width + runtime endian + runtime mode") {
        using arr_rt = packed_array<word_t, value_t, 2, 0, pack_endian::rt, pack_mode::rt>;

        arr_rt a(13, pack_endian::lsb, pack_mode::dense);

        REQUIRE(a.capacity() > 0);

        const std::size_t n = std::min<std::size_t>(a.capacity(), 4);
        for (std::size_t i = 0; i < n; ++i)
            a.push_back(static_cast<value_t>(i * 0x111));

        for (std::size_t i = 0; i < n; ++i)
            CHECK(a[i] == static_cast<value_t>(i * 0x111));
    }
}

TEST_CASE("core/packed/array — cross-layout round-trip") {
    SUBCASE("Word = uint64_t") {
        packed_array_sweep<std::uint64_t>();
    }
    SUBCASE("Word = uint32_t") {
        packed_array_sweep<std::uint32_t>();
    }
    SUBCASE("Word = uint16_t") {
        packed_array_sweep<std::uint16_t>();
    }
    SUBCASE("Word = uint8_t") {
        packed_array_sweep<std::uint8_t>();
    }
}
