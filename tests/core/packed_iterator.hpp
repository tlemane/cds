#include <doctest.h>

#include <iterator>

#include <cds/core/packed/packer.hpp>
#include <cds/core/packed/iterator.hpp>

using namespace cds;

using word_t = std::uint64_t;
using value_t = std::uint64_t;

TEST_CASE("core/packed/iterator") {
    SUBCASE("fixed width / sparse / lsb — write and read") {
        using it_t = packed_iterator_t<word_t, value_t, 4, pack_endian::lsb, pack_mode::sparse>;
        using cit_t =
            const_packed_iterator_t<word_t, value_t, 4, pack_endian::lsb, pack_mode::sparse>;

        word_t data[2]{};

        it_t it{data, 0};
        *it = value_t{0x5};
        ++it;
        *it = value_t{0xa};
        ++it;
        *it = value_t{0xf};

        cit_t cit{data, 0};
        CHECK(*cit == 0x5);
        ++cit;
        CHECK(*cit == 0xa);
        ++cit;
        CHECK(*cit == 0xf);
    }

    SUBCASE("fixed width / dense / msb — write and read") {
        using it_t = packed_iterator_t<word_t, value_t, 13, pack_endian::msb, pack_mode::dense>;
        using cit_t =
            const_packed_iterator_t<word_t, value_t, 13, pack_endian::msb, pack_mode::dense>;

        word_t data[4]{};

        constexpr value_t values[] = {0x001, 0x123, 0x456, 0x789, 0xabc, 0xdef};

        it_t it{data, 0};
        for (auto v : values) {
            *it = v;
            ++it;
        }

        cit_t cit{data, 0};
        for (auto v : values) {
            CHECK(*cit == v);
            ++cit;
        }
    }

    SUBCASE("operator[] matches *(it + n)") {
        using it_t = packed_iterator_t<word_t, value_t, 8, pack_endian::lsb, pack_mode::dense>;

        word_t data[2]{};

        it_t base{data, 0};
        for (std::size_t i = 0; i < 8; ++i)
            base[static_cast<std::ptrdiff_t>(i)] = static_cast<value_t>(i);

        for (std::size_t i = 0; i < 8; ++i) {
            CHECK(base[static_cast<std::ptrdiff_t>(i)] == static_cast<value_t>(i));
            CHECK(*(base + static_cast<std::ptrdiff_t>(i)) == static_cast<value_t>(i));
        }
    }

    SUBCASE("random access arithmetic") {
        using it_t = packed_iterator_t<word_t, value_t, 4, pack_endian::lsb, pack_mode::sparse>;

        word_t data[2]{};

        it_t begin{data, 0};
        it_t end{data, 4};

        CHECK(end - begin == 4);
        CHECK((begin + 4) == end);
        CHECK((4 + begin) == end);
        CHECK((end - 4) == begin);

        it_t mid = begin;
        mid += 2;
        CHECK(mid - begin == 2);
        mid -= 1;
        CHECK(mid - begin == 1);
    }

    SUBCASE("comparisons") {
        using it_t = packed_iterator_t<word_t, value_t, 4, pack_endian::lsb, pack_mode::sparse>;

        word_t data[2]{};

        it_t a{data, 0};
        it_t b{data, 1};
        it_t a2{data, 0};

        CHECK(a == a2);
        CHECK_FALSE(a == b);
        CHECK(a < b);
        CHECK(b > a);
        CHECK(a <= a2);
        CHECK(a >= a2);
    }

    SUBCASE("pre/post increment and decrement") {
        using it_t = packed_iterator_t<word_t, value_t, 4, pack_endian::lsb, pack_mode::sparse>;

        word_t data[2]{};

        it_t it{data, 0};

        it_t pre = ++it;
        CHECK(pre == it);
        CHECK((it - it_t{data, 0}) == 1);

        it_t before_post = it;
        it_t post = it++;
        CHECK(post == before_post);
        CHECK((it - post) == 1);

        it_t dpre = --it;
        CHECK(dpre == it);

        it_t before_dpost = it;
        it_t dpost = it--;
        CHECK(dpost == before_dpost);
    }

    SUBCASE("width == 1") {
        using it_t = packed_iterator_t<word_t, value_t, 1, pack_endian::lsb, pack_mode::dense>;
        using cit_t =
            const_packed_iterator_t<word_t, value_t, 1, pack_endian::lsb, pack_mode::dense>;

        word_t data[2]{};

        it_t it{data, 0};
        for (std::size_t i = 0; i < 128; ++i, ++it)
            *it = static_cast<value_t>(i & 1);

        cit_t cit{data, 0};
        for (std::size_t i = 0; i < 128; ++i, ++cit)
            CHECK(*cit == static_cast<value_t>(i & 1));
    }

    SUBCASE("width == word size") {
        using it_t = packed_iterator_t<word_t, word_t, 64, pack_endian::lsb, pack_mode::dense>;
        using cit_t =
            const_packed_iterator_t<word_t, word_t, 64, pack_endian::lsb, pack_mode::dense>;

        word_t data[2]{};

        it_t it{data, 0};
        *it = 0x123456789abcdef0ULL;
        ++it;
        *it = 0xfedcba9876543210ULL;

        cit_t cit{data, 0};
        CHECK(*cit == 0x123456789abcdef0ULL);
        ++cit;
        CHECK(*cit == 0xfedcba9876543210ULL);
    }

    SUBCASE("runtime width / sparse") {
        using it_t = packed_iterator_t<word_t, value_t, 0, pack_endian::lsb, pack_mode::sparse>;
        using cit_t =
            const_packed_iterator_t<word_t, value_t, 0, pack_endian::lsb, pack_mode::sparse>;

        word_t data[2]{};
        constexpr std::uint8_t width = 5;

        it_t it{data, 0, width};
        *it = value_t{0x1a};
        ++it;
        *it = value_t{0x03};
        ++it;
        *it = value_t{0x11};

        cit_t cit{data, 0, width};
        CHECK(*cit == 0x1a);
        ++cit;
        CHECK(*cit == 0x03);
        ++cit;
        CHECK(*cit == 0x11);
    }

    SUBCASE("runtime width / dense") {
        using it_t = packed_iterator_t<word_t, value_t, 0, pack_endian::lsb, pack_mode::dense>;
        using cit_t =
            const_packed_iterator_t<word_t, value_t, 0, pack_endian::lsb, pack_mode::dense>;

        word_t data[2]{};
        constexpr std::uint8_t width = 5;

        it_t it{data, 0, width};
        *it = value_t{0x1a};
        ++it;
        *it = value_t{0x03};
        ++it;
        *it = value_t{0x11};

        cit_t cit{data, 0, width};
        CHECK(*cit == 0x1a);
        ++cit;
        CHECK(*cit == 0x03);
        ++cit;
        CHECK(*cit == 0x11);
    }

    SUBCASE("runtime endian / sparse") {
        using it_t = packed_iterator_t<word_t, value_t, 5, pack_endian::rt, pack_mode::sparse>;
        using cit_t =
            const_packed_iterator_t<word_t, value_t, 5, pack_endian::rt, pack_mode::sparse>;

        word_t data[2]{};

        it_t it{data, 0, 5, pack_endian::lsb};
        *it = value_t{0x12};
        ++it;
        *it = value_t{0x03};

        cit_t cit{data, 0, 5, pack_endian::lsb};
        CHECK(*cit == 0x12);
        ++cit;
        CHECK(*cit == 0x03);
    }

    SUBCASE("runtime endian / dense") {
        using it_t = packed_iterator_t<word_t, value_t, 13, pack_endian::rt, pack_mode::dense>;
        using cit_t =
            const_packed_iterator_t<word_t, value_t, 13, pack_endian::rt, pack_mode::dense>;

        word_t data[4]{};

        it_t it{data, 0, 13, pack_endian::msb};
        *it = value_t{0x12};
        ++it;
        *it = value_t{0x03};
        ++it;
        *it = value_t{0x55};

        cit_t cit{data, 0, 13, pack_endian::msb};
        CHECK(*cit == 0x12);
        ++cit;
        CHECK(*cit == 0x03);
        ++cit;
        CHECK(*cit == 0x55);
    }

    SUBCASE("runtime mode / sparse") {
        using it_t = packed_iterator_t<word_t, value_t, 4, pack_endian::lsb, pack_mode::rt>;
        using cit_t = const_packed_iterator_t<word_t, value_t, 4, pack_endian::lsb, pack_mode::rt>;

        word_t data[2]{};

        it_t it{data, 0, 4, pack_endian::lsb, pack_mode::sparse};
        *it = value_t{0x5};
        ++it;
        *it = value_t{0xa};

        cit_t cit{data, 0, 4, pack_endian::lsb, pack_mode::sparse};
        CHECK(*cit == 0x5);
        ++cit;
        CHECK(*cit == 0xa);
    }

    SUBCASE("runtime mode / dense") {
        using it_t = packed_iterator_t<word_t, value_t, 13, pack_endian::lsb, pack_mode::rt>;
        using cit_t = const_packed_iterator_t<word_t, value_t, 13, pack_endian::lsb, pack_mode::rt>;

        word_t data[4]{};

        it_t it{data, 0, 13, pack_endian::lsb, pack_mode::dense};
        *it = value_t{0x123};
        ++it;
        *it = value_t{0x456};
        ++it;
        *it = value_t{0x789};

        cit_t cit{data, 0, 13, pack_endian::lsb, pack_mode::dense};
        CHECK(*cit == 0x123);
        ++cit;
        CHECK(*cit == 0x456);
        ++cit;
        CHECK(*cit == 0x789);
    }

    SUBCASE("runtime endian + runtime mode") {
        using it_t = packed_iterator_t<word_t, value_t, 13, pack_endian::rt, pack_mode::rt>;
        using cit_t = const_packed_iterator_t<word_t, value_t, 13, pack_endian::rt, pack_mode::rt>;

        word_t data[4]{};

        it_t it{data, 0, 13, pack_endian::lsb, pack_mode::dense};
        *it = value_t{0x123};
        ++it;
        *it = value_t{0x456};

        cit_t cit{data, 0, 13, pack_endian::lsb, pack_mode::dense};
        CHECK(*cit == 0x123);
        ++cit;
        CHECK(*cit == 0x456);
    }

    SUBCASE("runtime width + runtime endian + runtime mode") {
        using it_t = packed_iterator_t<word_t, value_t, 0, pack_endian::rt, pack_mode::rt>;
        using cit_t = const_packed_iterator_t<word_t, value_t, 0, pack_endian::rt, pack_mode::rt>;

        word_t data[4]{};
        constexpr std::uint8_t width = 13;

        it_t it{data, 0, width, pack_endian::lsb, pack_mode::dense};
        *it = value_t{0x123};
        ++it;
        *it = value_t{0x456};

        cit_t cit{data, 0, width, pack_endian::lsb, pack_mode::dense};
        CHECK(*cit == 0x123);
        ++cit;
        CHECK(*cit == 0x456);
    }

    SUBCASE("std::distance / std::advance") {
        using it_t = packed_iterator_t<word_t, value_t, 4, pack_endian::lsb, pack_mode::sparse>;

        word_t data[2]{};

        it_t begin{data, 0};
        it_t it = begin;
        std::advance(it, 3);

        CHECK(std::distance(begin, it) == 3);
    }
}
