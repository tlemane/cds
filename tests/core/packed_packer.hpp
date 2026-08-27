#include <doctest.h>

#include <cds/core/packed/packer.hpp>

using namespace cds;

using word_t = std::uint64_t;
using value_t = std::uint64_t;

TEST_CASE("core/packed/bit_packer") {
    SUBCASE("fixed width / sparse / lsb") {
        using bp = bit_packer<word_t, value_t, 4, pack_endian::lsb, pack_mode::sparse>;

        word_t data[2]{};

        bp::pack(data, 0, value_t{0x5});
        bp::pack(data, 1, value_t{0xa});
        bp::pack(data, 2, value_t{0xf});

        CHECK(bp::unpack(data, 0) == 0x5);
        CHECK(bp::unpack(data, 1) == 0xa);
        CHECK(bp::unpack(data, 2) == 0xf);
    }

    SUBCASE("fixed width / sparse / msb") {
        using bp = bit_packer<word_t, value_t, 4, pack_endian::msb, pack_mode::sparse>;

        word_t data[2]{};

        bp::pack(data, 0, value_t{0x5});
        bp::pack(data, 1, value_t{0xa});
        bp::pack(data, 2, value_t{0xf});

        CHECK(bp::unpack(data, 0) == 0x5);
        CHECK(bp::unpack(data, 1) == 0xa);
        CHECK(bp::unpack(data, 2) == 0xf);
    }

    SUBCASE("fixed width / dense / lsb") {
        using bp = bit_packer<word_t, value_t, 4, pack_endian::lsb, pack_mode::dense>;

        word_t data[2]{};

        bp::pack(data, 0, value_t{0x5});
        bp::pack(data, 1, value_t{0xa});
        bp::pack(data, 2, value_t{0xf});

        CHECK(bp::unpack(data, 0) == 0x5);
        CHECK(bp::unpack(data, 1) == 0xa);
        CHECK(bp::unpack(data, 2) == 0xf);
    }

    SUBCASE("fixed width / dense / msb") {
        using bp = bit_packer<word_t, value_t, 4, pack_endian::msb, pack_mode::dense>;

        word_t data[2]{};

        bp::pack(data, 0, value_t{0x5});
        bp::pack(data, 1, value_t{0xa});
        bp::pack(data, 2, value_t{0xf});

        CHECK(bp::unpack(data, 0) == 0x5);
        CHECK(bp::unpack(data, 1) == 0xa);
        CHECK(bp::unpack(data, 2) == 0xf);
    }

    SUBCASE("dense width divides word size") {
        using bp = bit_packer<word_t, value_t, 8, pack_endian::lsb, pack_mode::dense>;

        word_t data[2]{};

        for (std::size_t i = 0; i < 16; ++i)
            bp::pack(data, i, static_cast<value_t>(i));

        for (std::size_t i = 0; i < 16; ++i)
            CHECK(bp::unpack(data, i) == static_cast<value_t>(i));
    }

    SUBCASE("dense width crosses word boundary / lsb") {
        using bp = bit_packer<word_t, value_t, 13, pack_endian::lsb, pack_mode::dense>;

        word_t data[4]{};

        constexpr value_t values[] = {0x001, 0x123, 0x456, 0x789, 0xabc, 0xdef};

        for (std::size_t i = 0; i < std::size(values); ++i)
            bp::pack(data, i, values[i]);

        for (std::size_t i = 0; i < std::size(values); ++i)
            CHECK(bp::unpack(data, i) == values[i]);
    }

    SUBCASE("dense width crosses word boundary / msb") {
        using bp = bit_packer<word_t, value_t, 13, pack_endian::msb, pack_mode::dense>;

        word_t data[4]{};

        constexpr value_t values[] = {0x001, 0x123, 0x456, 0x789, 0xabc, 0xdef};

        for (std::size_t i = 0; i < std::size(values); ++i)
            bp::pack(data, i, values[i]);

        for (std::size_t i = 0; i < std::size(values); ++i)
            CHECK(bp::unpack(data, i) == values[i]);
    }

    SUBCASE("width == word size") {
        using bp = bit_packer<word_t, word_t, 64, pack_endian::lsb, pack_mode::dense>;

        word_t data[2]{};

        bp::pack(data, 0, 0x123456789abcdef0ULL);
        bp::pack(data, 1, 0xfedcba9876543210ULL);

        CHECK(bp::unpack(data, 0) == 0x123456789abcdef0ULL);
        CHECK(bp::unpack(data, 1) == 0xfedcba9876543210ULL);
    }

    SUBCASE("width == 1") {
        using bp = bit_packer<word_t, value_t, 1, pack_endian::lsb, pack_mode::dense>;

        word_t data[2]{};

        for (std::size_t i = 0; i < 128; ++i) {
            bp::pack(data, i, static_cast<value_t>(i & 1));
        }

        for (std::size_t i = 0; i < 128; ++i)
            CHECK(bp::unpack(data, i) == static_cast<value_t>(i & 1));
    }

    SUBCASE("safe pack replaces existing value") {
        using bp = bit_packer<word_t, value_t, 5, pack_endian::lsb, pack_mode::dense>;

        word_t data[2]{};

        bp::pack(data, 0, value_t{0x1f});
        CHECK(bp::unpack(data, 0) == 0x1f);

        bp::pack(data, 0, value_t{0x03});
        CHECK(bp::unpack(data, 0) == 0x03);
    }

    SUBCASE("runtime width / sparse") {
        using bp = bit_packer<word_t, value_t, 0, pack_endian::lsb, pack_mode::sparse>;

        bp packer{5};

        word_t data[2]{};

        packer.pack(data, 0, value_t{0x1a});
        packer.pack(data, 1, value_t{0x03});
        packer.pack(data, 2, value_t{0x11});

        CHECK(packer.unpack(data, 0) == 0x1a);
        CHECK(packer.unpack(data, 1) == 0x03);
        CHECK(packer.unpack(data, 2) == 0x11);
    }

    SUBCASE("runtime width / dense") {
        using bp = bit_packer<word_t, value_t, 0, pack_endian::lsb, pack_mode::dense>;

        word_t data[2]{};

        bp::pack(data, 0, value_t{0x1a}, 5);
        bp::pack(data, 1, value_t{0x03}, 5);
        bp::pack(data, 2, value_t{0x11}, 5);

        CHECK(bp::unpack(data, 0, 5) == 0x1a);
        CHECK(bp::unpack(data, 1, 5) == 0x03);
        CHECK(bp::unpack(data, 2, 5) == 0x11);
    }

    SUBCASE("runtime endian / sparse") {
        using bp = bit_packer<word_t, value_t, 5, pack_endian::rt, pack_mode::sparse>;

        word_t data[2]{};

        bp::pack(data, 0, value_t{0x12}, pack_endian::lsb);
        bp::pack(data, 1, value_t{0x03}, pack_endian::lsb);

        CHECK(bp::unpack(data, 0, pack_endian::lsb) == 0x12);
        CHECK(bp::unpack(data, 1, pack_endian::lsb) == 0x03);
    }

    SUBCASE("runtime endian / dense") {
        using bp = bit_packer<word_t, value_t, 13, pack_endian::rt, pack_mode::dense>;

        word_t data[4]{};

        bp::pack(data, 0, value_t{0x12}, pack_endian::msb);
        bp::pack(data, 1, value_t{0x03}, pack_endian::msb);
        bp::pack(data, 2, value_t{0x55}, pack_endian::msb);

        CHECK(bp::unpack(data, 0, pack_endian::msb) == 0x12);
        CHECK(bp::unpack(data, 1, pack_endian::msb) == 0x03);
        CHECK(bp::unpack(data, 2, pack_endian::msb) == 0x55);
    }

    SUBCASE("runtime mode / none") {
        using bp = bit_packer<word_t, value_t, 64, pack_endian::lsb, pack_mode::rt>;

        word_t data[2]{};

        bp::pack(data, 0, value_t{0x42}, pack_mode::none);

        CHECK(bp::unpack(data, 0, pack_mode::none) == 0x42);
    }

    SUBCASE("runtime mode / sparse") {
        using bp = bit_packer<word_t, value_t, 4, pack_endian::lsb, pack_mode::rt>;

        word_t data[2]{};

        bp::pack(data, 0, value_t{0x5}, pack_mode::sparse);
        bp::pack(data, 1, value_t{0xa}, pack_mode::sparse);

        CHECK(bp::unpack(data, 0, pack_mode::sparse) == 0x5);
        CHECK(bp::unpack(data, 1, pack_mode::sparse) == 0xa);
    }

    SUBCASE("runtime mode / dense") {
        using bp = bit_packer<word_t, value_t, 13, pack_endian::lsb, pack_mode::rt>;

        word_t data[4]{};

        bp::pack(data, 0, value_t{0x123}, pack_mode::dense);
        bp::pack(data, 1, value_t{0x456}, pack_mode::dense);
        bp::pack(data, 2, value_t{0x789}, pack_mode::dense);

        CHECK(bp::unpack(data, 0, pack_mode::dense) == 0x123);
        CHECK(bp::unpack(data, 1, pack_mode::dense) == 0x456);
        CHECK(bp::unpack(data, 2, pack_mode::dense) == 0x789);
    }

    SUBCASE("runtime endian + runtime mode") {
        using bp = bit_packer<word_t, value_t, 13, pack_endian::rt, pack_mode::rt>;

        word_t data[4]{};

        bp::pack(data, 0, value_t{0x123}, pack_endian::lsb, pack_mode::dense);

        bp::pack(data, 1, value_t{0x456}, pack_endian::lsb, pack_mode::dense);

        CHECK(bp::unpack(data, 0, pack_endian::lsb, pack_mode::dense) == 0x123);

        CHECK(bp::unpack(data, 1, pack_endian::lsb, pack_mode::dense) == 0x456);
    }

    SUBCASE("runtime width + runtime endian + runtime mode") {
        using bp = bit_packer<word_t, value_t, 0, pack_endian::rt, pack_mode::rt>;

        word_t data[4]{};

        constexpr std::uint8_t width = 13;

        bp::pack(data, 0, value_t{0x123}, width, pack_endian::lsb, pack_mode::dense);

        bp::pack(data, 1, value_t{0x456}, width, pack_endian::lsb, pack_mode::dense);

        CHECK(bp::unpack(data, 0, width, pack_endian::lsb, pack_mode::dense) == 0x123);

        CHECK(bp::unpack(data, 1, width, pack_endian::lsb, pack_mode::dense) == 0x456);
    }
}
