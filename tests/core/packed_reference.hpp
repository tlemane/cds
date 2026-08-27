#include <doctest.h>

#include <cds/core/packed/packer.hpp>
#include <cds/core/packed/reference.hpp>

using namespace cds;

using word_t = std::uint64_t;
using value_t = std::uint64_t;

TEST_CASE("core/packed/reference") {

    SUBCASE("fixed width / sparse / lsb") {
        using bp = bit_packer<word_t, value_t, 4, pack_endian::lsb, pack_mode::sparse>;

        word_t data[2]{};

        bp::ref(data, 0) = value_t{0x5};
        bp::ref(data, 1) = value_t{0xa};
        bp::ref(data, 2) = value_t{0xf};

        CHECK(*bp::cref(data, 0) == 0x5);
        CHECK(*bp::cref(data, 1) == 0xa);
        CHECK(*bp::cref(data, 2) == 0xf);

        value_t v0 = bp::cref(data, 0);
        CHECK(v0 == 0x5);

        CHECK(*bp::ref(data, 0) == 0x5);
    }

    SUBCASE("fixed width / sparse / msb") {
        using bp = bit_packer<word_t, value_t, 4, pack_endian::msb, pack_mode::sparse>;

        word_t data[2]{};

        bp::ref(data, 0) = value_t{0x5};
        bp::ref(data, 1) = value_t{0xa};
        bp::ref(data, 2) = value_t{0xf};

        CHECK(*bp::cref(data, 0) == 0x5);
        CHECK(*bp::cref(data, 1) == 0xa);
        CHECK(*bp::cref(data, 2) == 0xf);
    }

    SUBCASE("fixed width / dense / lsb") {
        using bp = bit_packer<word_t, value_t, 4, pack_endian::lsb, pack_mode::dense>;

        word_t data[2]{};

        bp::ref(data, 0) = value_t{0x5};
        bp::ref(data, 1) = value_t{0xa};
        bp::ref(data, 2) = value_t{0xf};

        CHECK(*bp::cref(data, 0) == 0x5);
        CHECK(*bp::cref(data, 1) == 0xa);
        CHECK(*bp::cref(data, 2) == 0xf);
    }

    SUBCASE("fixed width / dense / msb") {
        using bp = bit_packer<word_t, value_t, 4, pack_endian::msb, pack_mode::dense>;

        word_t data[2]{};

        bp::ref(data, 0) = value_t{0x5};
        bp::ref(data, 1) = value_t{0xa};
        bp::ref(data, 2) = value_t{0xf};

        CHECK(*bp::cref(data, 0) == 0x5);
        CHECK(*bp::cref(data, 1) == 0xa);
        CHECK(*bp::cref(data, 2) == 0xf);
    }

    SUBCASE("dense width divides word size") {
        using bp = bit_packer<word_t, value_t, 8, pack_endian::lsb, pack_mode::dense>;

        word_t data[2]{};

        for (std::size_t i = 0; i < 16; ++i)
            bp::ref(data, i) = static_cast<value_t>(i);

        for (std::size_t i = 0; i < 16; ++i)
            CHECK(*bp::cref(data, i) == static_cast<value_t>(i));
    }

    SUBCASE("dense width crosses word boundary / lsb") {
        using bp = bit_packer<word_t, value_t, 13, pack_endian::lsb, pack_mode::dense>;

        word_t data[4]{};

        constexpr value_t values[] = {0x001, 0x123, 0x456, 0x789, 0xabc, 0xdef};

        for (std::size_t i = 0; i < std::size(values); ++i)
            bp::ref(data, i) = values[i];

        for (std::size_t i = 0; i < std::size(values); ++i)
            CHECK(*bp::cref(data, i) == values[i]);
    }

    SUBCASE("dense width crosses word boundary / msb") {
        using bp = bit_packer<word_t, value_t, 13, pack_endian::msb, pack_mode::dense>;

        word_t data[4]{};

        constexpr value_t values[] = {0x001, 0x123, 0x456, 0x789, 0xabc, 0xdef};

        for (std::size_t i = 0; i < std::size(values); ++i)
            bp::ref(data, i) = values[i];

        for (std::size_t i = 0; i < std::size(values); ++i)
            CHECK(*bp::cref(data, i) == values[i]);
    }

    SUBCASE("width == 1") {
        using bp = bit_packer<word_t, value_t, 1, pack_endian::lsb, pack_mode::dense>;

        word_t data[2]{};

        for (std::size_t i = 0; i < 128; ++i)
            bp::ref(data, i) = static_cast<value_t>(i & 1);

        for (std::size_t i = 0; i < 128; ++i)
            CHECK(*bp::cref(data, i) == static_cast<value_t>(i & 1));
    }

    SUBCASE("assignment through reference replaces existing value") {
        using bp = bit_packer<word_t, value_t, 5, pack_endian::lsb, pack_mode::dense>;

        word_t data[2]{};

        bp::ref(data, 0) = value_t{0x1f};
        CHECK(*bp::cref(data, 0) == 0x1f);

        bp::ref(data, 0) = value_t{0x03};
        CHECK(*bp::cref(data, 0) == 0x03);
    }

    SUBCASE("runtime width / sparse") {
        using bp = bit_packer<word_t, value_t, 0, pack_endian::lsb, pack_mode::sparse>;

        word_t data[2]{};
        constexpr std::uint8_t width = 5;

        bp::ref(data, 0, width) = value_t{0x1a};
        bp::ref(data, 1, width) = value_t{0x03};
        bp::ref(data, 2, width) = value_t{0x11};

        CHECK(*bp::cref(data, 0, width) == 0x1a);
        CHECK(*bp::cref(data, 1, width) == 0x03);
        CHECK(*bp::cref(data, 2, width) == 0x11);
    }

    SUBCASE("runtime width / dense") {
        using bp = bit_packer<word_t, value_t, 0, pack_endian::lsb, pack_mode::dense>;

        word_t data[2]{};
        constexpr std::uint8_t width = 5;

        bp::ref(data, 0, width) = value_t{0x1a};
        bp::ref(data, 1, width) = value_t{0x03};
        bp::ref(data, 2, width) = value_t{0x11};

        CHECK(*bp::cref(data, 0, width) == 0x1a);
        CHECK(*bp::cref(data, 1, width) == 0x03);
        CHECK(*bp::cref(data, 2, width) == 0x11);
    }

    SUBCASE("runtime endian / sparse") {
        using bp = bit_packer<word_t, value_t, 5, pack_endian::rt, pack_mode::sparse>;

        word_t data[2]{};

        bp::ref(data, 0, pack_endian::lsb) = value_t{0x12};
        bp::ref(data, 1, pack_endian::lsb) = value_t{0x03};

        CHECK(*bp::cref(data, 0, pack_endian::lsb) == 0x12);
        CHECK(*bp::cref(data, 1, pack_endian::lsb) == 0x03);
    }

    SUBCASE("runtime endian / dense") {
        using bp = bit_packer<word_t, value_t, 13, pack_endian::rt, pack_mode::dense>;

        word_t data[4]{};

        bp::ref(data, 0, pack_endian::msb) = value_t{0x12};
        bp::ref(data, 1, pack_endian::msb) = value_t{0x03};
        bp::ref(data, 2, pack_endian::msb) = value_t{0x55};

        CHECK(*bp::cref(data, 0, pack_endian::msb) == 0x12);
        CHECK(*bp::cref(data, 1, pack_endian::msb) == 0x03);
        CHECK(*bp::cref(data, 2, pack_endian::msb) == 0x55);
    }

    SUBCASE("runtime mode / none") {
        using bp = bit_packer<word_t, value_t, 64, pack_endian::lsb, pack_mode::rt>;

        word_t data[2]{};

        bp::ref(data, 0, pack_mode::none) = value_t{0x42};

        CHECK(*bp::cref(data, 0, pack_mode::none) == 0x42);
    }

    SUBCASE("runtime mode / sparse") {
        using bp = bit_packer<word_t, value_t, 4, pack_endian::lsb, pack_mode::rt>;

        word_t data[2]{};

        bp::ref(data, 0, pack_mode::sparse) = value_t{0x5};
        bp::ref(data, 1, pack_mode::sparse) = value_t{0xa};

        CHECK(*bp::cref(data, 0, pack_mode::sparse) == 0x5);
        CHECK(*bp::cref(data, 1, pack_mode::sparse) == 0xa);
    }

    SUBCASE("runtime mode / dense") {
        using bp = bit_packer<word_t, value_t, 13, pack_endian::lsb, pack_mode::rt>;

        word_t data[4]{};

        bp::ref(data, 0, pack_mode::dense) = value_t{0x123};
        bp::ref(data, 1, pack_mode::dense) = value_t{0x456};
        bp::ref(data, 2, pack_mode::dense) = value_t{0x789};

        CHECK(*bp::cref(data, 0, pack_mode::dense) == 0x123);
        CHECK(*bp::cref(data, 1, pack_mode::dense) == 0x456);
        CHECK(*bp::cref(data, 2, pack_mode::dense) == 0x789);
    }

    SUBCASE("runtime endian + runtime mode") {
        using bp = bit_packer<word_t, value_t, 13, pack_endian::rt, pack_mode::rt>;

        word_t data[4]{};

        bp::ref(data, 0, pack_endian::lsb, pack_mode::dense) = value_t{0x123};
        bp::ref(data, 1, pack_endian::lsb, pack_mode::dense) = value_t{0x456};

        CHECK(*bp::cref(data, 0, pack_endian::lsb, pack_mode::dense) == 0x123);
        CHECK(*bp::cref(data, 1, pack_endian::lsb, pack_mode::dense) == 0x456);
    }

    SUBCASE("runtime width + runtime endian + runtime mode") {
        using bp = bit_packer<word_t, value_t, 0, pack_endian::rt, pack_mode::rt>;

        word_t data[4]{};
        constexpr std::uint8_t width = 13;

        bp::ref(data, 0, width, pack_endian::lsb, pack_mode::dense) = value_t{0x123};
        bp::ref(data, 1, width, pack_endian::lsb, pack_mode::dense) = value_t{0x456};

        CHECK(*bp::cref(data, 0, width, pack_endian::lsb, pack_mode::dense) == 0x123);
        CHECK(*bp::cref(data, 1, width, pack_endian::lsb, pack_mode::dense) == 0x456);
    }

    SUBCASE("reference and static API agree") {
        using bp = bit_packer<word_t, value_t, 4, pack_endian::lsb, pack_mode::sparse>;

        word_t data[2]{};

        bp::ref(data, 0) = value_t{0x7};
        CHECK(bp::unpack(data, 0) == 0x7);

        bp::pack(data, 1, value_t{0x9});
        CHECK(*bp::cref(data, 1) == 0x9);
    }
}
