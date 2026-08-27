#include <doctest.h>

#include <iterator>
#include <utility>

#include <cds/packed/vector.hpp>

#include "packed_check.hpp"

using namespace cds;

using vec_t = packed_vector<word_t, value_t, 4, pack_endian::lsb, pack_mode::sparse>;

TEST_CASE("core/packed/vector") {

    SUBCASE("fixed width / sparse / lsb — push_back grows and preserves data") {
        vec_t v;
        CHECK(v.empty());
        CHECK(v.size() == 0);
        CHECK(v.capacity() == 0);

        for (std::size_t i = 0; i < 50; ++i)
            v.push_back(static_cast<value_t>(i % 16));

        CHECK(v.size() == 50);
        CHECK(v.capacity() >= 50);

        for (std::size_t i = 0; i < 50; ++i)
            CHECK(v[i] == static_cast<value_t>(i % 16));
    }

    SUBCASE("fixed width / dense / msb — push_back across word boundaries") {
        using vec_md13 = packed_vector<word_t, value_t, 13, pack_endian::msb, pack_mode::dense>;

        vec_md13 v;
        constexpr value_t values[] = {0x001, 0x123, 0x456, 0x789, 0xabc, 0x1fff};

        for (auto val : values)
            v.push_back(val);

        for (std::size_t i = 0; i < std::size(values); ++i)
            CHECK(v[i] == values[i]);
    }

    SUBCASE("reserve avoids growth up to the reserved amount") {
        vec_t v;
        v.reserve(20);

        const std::size_t cap_after_reserve = v.capacity();
        CHECK(cap_after_reserve >= 20);

        for (std::size_t i = 0; i < 20; ++i)
            v.push_back(static_cast<value_t>(i % 16));

        CHECK(v.capacity() == cap_after_reserve);
        CHECK(v.size() == 20);
    }

    SUBCASE("shrink_to_fit reduces capacity to what's needed") {
        vec_t v;
        v.reserve(100);
        v.push_back(value_t{0x1});
        v.push_back(value_t{0x2});

        REQUIRE(v.capacity() >= 100);

        v.shrink_to_fit();

        CHECK(v.capacity() >= v.size());
        CHECK(v.capacity() < 100);
        CHECK(v[0] == 0x1);
        CHECK(v[1] == 0x2);
    }

    SUBCASE("resize grows and fills with the given value") {
        vec_t v;
        v.push_back(value_t{0x1});
        v.resize(5, value_t{0x9});

        CHECK(v.size() == 5);
        CHECK(v[0] == 0x1);
        CHECK(v[1] == 0x9);
        CHECK(v[2] == 0x9);
        CHECK(v[3] == 0x9);
        CHECK(v[4] == 0x9);
    }

    SUBCASE("resize down just truncates size") {
        vec_t v;
        for (std::size_t i = 0; i < 5; ++i)
            v.push_back(static_cast<value_t>(i));

        v.resize(2);
        CHECK(v.size() == 2);
        CHECK(v[0] == 0);
        CHECK(v[1] == 1);
    }

    SUBCASE("pop_back and clear") {
        vec_t v;
        v.push_back(value_t{0x1});
        v.push_back(value_t{0x2});
        v.push_back(value_t{0x3});

        v.pop_back();
        CHECK(v.size() == 2);
        CHECK(v.back() == 0x2);

        v.clear();
        CHECK(v.empty());
        CHECK(v.size() == 0);

        CHECK(v.capacity() > 0);
    }

    SUBCASE("front / back") {
        vec_t v;
        v.push_back(value_t{0x1});
        v.push_back(value_t{0x2});
        v.push_back(value_t{0xe});

        CHECK(v.front() == 0x1);
        CHECK(v.back() == 0xe);
    }

    SUBCASE("iteration via begin/end") {
        vec_t v;
        for (std::size_t i = 0; i < 10; ++i)
            v.push_back(static_cast<value_t>(i % 16));

        std::size_t i = 0;
        for (auto it = v.begin(); it != v.end(); ++it, ++i)
            CHECK(*it == static_cast<value_t>(i % 16));

        CHECK(std::distance(v.begin(), v.end()) == 10);
    }

    SUBCASE("mutation through iterator") {
        vec_t v;
        for (std::size_t i = 0; i < 5; ++i)
            v.push_back(0);

        for (auto&& r : v)
            r = value_t{0x7};

        for (std::size_t i = 0; i < 5; ++i)
            CHECK(v[i] == 0x7);
    }

    SUBCASE("const iteration via cbegin/cend") {
        vec_t v;
        v.push_back(value_t{0x1});
        v.push_back(value_t{0x2});
        v.push_back(value_t{0x3});

        const vec_t& cv = v;

        value_t expected = 1;
        for (auto it = cv.cbegin(); it != cv.cend(); ++it, ++expected)
            CHECK(*it == expected);
    }

    SUBCASE("range constructor") {
        value_t values[] = {0x1, 0x2, 0x3, 0x4};
        vec_t v(std::begin(values), std::end(values));

        CHECK(v.size() == 4);
        CHECK(v[0] == 0x1);
        CHECK(v[1] == 0x2);
        CHECK(v[2] == 0x3);
        CHECK(v[3] == 0x4);
    }

    SUBCASE("initializer_list constructor") {
        vec_t v{value_t{0x1}, value_t{0x2}, value_t{0x3}, value_t{0x4}};

        CHECK(v.size() == 4);
        CHECK(v[0] == 0x1);
        CHECK(v[1] == 0x2);
        CHECK(v[2] == 0x3);
        CHECK(v[3] == 0x4);
    }

    SUBCASE("copy construction is a deep copy") {
        vec_t a{value_t{0x1}, value_t{0x2}};
        vec_t b = a;

        b[0] = value_t{0xf};
        b.push_back(value_t{0x3});

        CHECK(a.size() == 2);
        CHECK(a[0] == 0x1);
        CHECK(b.size() == 3);
        CHECK(b[0] == 0xf);
        CHECK(a.data() != b.data());
    }

    SUBCASE("copy assignment is a deep copy") {
        vec_t a{value_t{0x1}, value_t{0x2}};
        vec_t b{value_t{0x9}};

        b = a;
        b[0] = value_t{0xf};

        CHECK(a[0] == 0x1);
        CHECK(b[0] == 0xf);
        CHECK(b.size() == 2);
    }

    SUBCASE("move construction steals the buffer") {
        vec_t a{value_t{0x1}, value_t{0x2}, value_t{0x3}};
        word_t* original_data = a.data();

        vec_t b = std::move(a);

        CHECK(b.size() == 3);
        CHECK(b[0] == 0x1);
        CHECK(b[1] == 0x2);
        CHECK(b[2] == 0x3);
        CHECK(b.data() == original_data);
        CHECK(a.size() == 0);
    }

    SUBCASE("move assignment steals the buffer") {
        vec_t a{value_t{0x1}, value_t{0x2}};
        word_t* original_data = a.data();

        vec_t b{value_t{0x9}};
        b = std::move(a);

        CHECK(b.size() == 2);
        CHECK(b[0] == 0x1);
        CHECK(b[1] == 0x2);
        CHECK(b.data() == original_data);
    }

    SUBCASE("swap exchanges contents") {
        vec_t a{value_t{0x1}, value_t{0x2}};
        vec_t b{value_t{0x9}, value_t{0xa}, value_t{0xb}};

        word_t* a_data = a.data();
        word_t* b_data = b.data();

        using std::swap;
        swap(a, b);

        CHECK(a.size() == 3);
        CHECK(a[0] == 0x9);
        CHECK(b.size() == 2);
        CHECK(b[0] == 0x1);

        CHECK(a.data() == b_data);
        CHECK(b.data() == a_data);
    }

    SUBCASE("runtime width / sparse") {
        using vec_rtw = packed_vector<word_t, value_t, 0, pack_endian::lsb, pack_mode::sparse>;

        constexpr std::uint8_t width = 5;
        vec_rtw v(width);

        for (std::size_t i = 0; i < 40; ++i)
            v.push_back(static_cast<value_t>(i % 32));

        CHECK(v.size() == 40);
        for (std::size_t i = 0; i < 40; ++i)
            CHECK(v[i] == static_cast<value_t>(i % 32));
    }

    SUBCASE("runtime endian + runtime mode") {
        using vec_rtem = packed_vector<word_t, value_t, 13, pack_endian::rt, pack_mode::rt>;

        vec_rtem v(13, pack_endian::msb, pack_mode::dense);

        for (value_t val : {value_t{0x123}, value_t{0x456}, value_t{0x789}, value_t{0xabc}})
            v.push_back(val);

        CHECK(v[0] == 0x123);
        CHECK(v[1] == 0x456);
        CHECK(v[2] == 0x789);
        CHECK(v[3] == 0xabc);
    }

    SUBCASE("runtime width + runtime endian + runtime mode, with growth") {
        using vec_rt = packed_vector<word_t, value_t, 0, pack_endian::rt, pack_mode::rt>;

        vec_rt v(13, pack_endian::lsb, pack_mode::dense);

        for (std::size_t i = 0; i < 30; ++i)
            v.push_back(static_cast<value_t>(i * 7 % 0x1fff));

        CHECK(v.size() == 30);
        for (std::size_t i = 0; i < 30; ++i)
            CHECK(v[i] == static_cast<value_t>(i * 7 % 0x1fff));
    }
}

TEST_CASE("core/packed/vector — cross-layout round-trip") {
    SUBCASE("Word = uint64_t") {
        packed_vector_sweep<std::uint64_t>();
    }
    SUBCASE("Word = uint32_t") {
        packed_vector_sweep<std::uint32_t>();
    }
    SUBCASE("Word = uint16_t") {
        packed_vector_sweep<std::uint16_t>();
    }
    SUBCASE("Word = uint8_t") {
        packed_vector_sweep<std::uint8_t>();
    }
}
