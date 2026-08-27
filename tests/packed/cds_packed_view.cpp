#include <doctest.h>

#include <iterator>

#include <cds/packed/view.hpp>

#include "packed_check.hpp"

using namespace cds;

using view_t = packed_view<word_t, value_t, 4, pack_endian::lsb, pack_mode::sparse>;
using cview_t = const_packed_view<word_t, value_t, 4, pack_endian::lsb, pack_mode::sparse>;

TEST_CASE("core/packed/view") {

    SUBCASE("fixed width / sparse / lsb — write and read") {
        word_t data[2]{};
        view_t v(data, 6);

        v[0] = value_t{0x5};
        v[1] = value_t{0xa};
        v[2] = value_t{0xf};

        cview_t cv(data, 6);
        CHECK(cv[0] == 0x5);
        CHECK(cv[1] == 0xa);
        CHECK(cv[2] == 0xf);
    }

    SUBCASE("fixed width / dense / msb — write and read across word boundary") {
        using view_md = packed_view<word_t, value_t, 13, pack_endian::msb, pack_mode::dense>;
        using cview_md = const_packed_view<word_t, value_t, 13, pack_endian::msb, pack_mode::dense>;

        word_t data[4]{};
        view_md v(data, 6);

        constexpr value_t values[] = {0x001, 0x123, 0x456, 0x789, 0xabc, 0x1fff};

        for (std::size_t i = 0; i < std::size(values); ++i)
            v[i] = values[i];

        cview_md cv(data, 6);
        for (std::size_t i = 0; i < std::size(values); ++i)
            CHECK(cv[i] == values[i]);
    }

    SUBCASE("front / back") {
        word_t data[1]{};
        view_t v(data, 4);

        v[0] = value_t{0x1};
        v[3] = value_t{0xe};

        CHECK(v.front() == 0x1);
        CHECK(v.back() == 0xe);
    }

    SUBCASE("iteration via begin/end") {
        word_t data[1]{};
        view_t v(data, 5);

        value_t next = 1;
        for (auto&& r : v) {
            r = next;
            ++next;
        }

        next = 1;
        for (auto it = v.begin(); it != v.end(); ++it, ++next)
            CHECK(*it == next);
    }

    SUBCASE("const iteration via cbegin/cend") {
        word_t data[1]{};
        view_t v(data, 3);
        v[0] = value_t{0x1};
        v[1] = value_t{0x2};
        v[2] = value_t{0x3};

        value_t next = 1;
        for (auto it = v.cbegin(); it != v.cend(); ++it, ++next)
            CHECK(*it == next);
    }

    SUBCASE("view is non-owning — writes are visible through another view") {
        word_t data[1]{};

        view_t writer(data, 4);
        view_t reader(data, 4);

        writer[2] = value_t{0xd};

        CHECK(reader[2] == 0xd);
    }

    SUBCASE("mutable view converts to const view over the same storage") {
        word_t data[1]{};
        view_t v(data, 3);
        v[0] = value_t{0x7};

        cview_t cv = v;
        CHECK(cv[0] == value_t{0x7});
        CHECK(cv.size() == v.size());
    }

    SUBCASE("from_words / sparse") {
        word_t data[2]{};

        auto v = view_t::from_words(data, 2);

        CHECK(v.size() == 32);

        for (std::size_t i = 0; i < v.size(); ++i)
            v[i] = static_cast<value_t>(i % 16);

        for (std::size_t i = 0; i < v.size(); ++i)
            CHECK(v[i] == static_cast<value_t>(i % 16));
    }

    SUBCASE("first() returns a prefix over the same buffer") {
        word_t data[1]{};
        view_t v(data, 6);
        for (std::size_t i = 0; i < 6; ++i)
            v[i] = static_cast<value_t>(i);

        auto pre = v.first(3);
        CHECK(pre.size() == 3);
        CHECK(pre[0] == 0);
        CHECK(pre[1] == 1);
        CHECK(pre[2] == 2);

        pre[0] = value_t{0xf};
        CHECK(v[0] == 0xf);
    }

    SUBCASE("subview() with an explicit count") {
        word_t data[1]{};
        view_t v(data, 6);
        for (std::size_t i = 0; i < 6; ++i)
            v[i] = static_cast<value_t>(i);

        auto mid = v.subview(2, 3);
        CHECK(mid.size() == 3);
        CHECK(mid[0] == 2);
        CHECK(mid[1] == 3);
        CHECK(mid[2] == 4);
    }

    SUBCASE("subview() to the end (npos)") {
        word_t data[1]{};
        view_t v(data, 6);
        for (std::size_t i = 0; i < 6; ++i)
            v[i] = static_cast<value_t>(i);

        auto tail = v.subview(4);
        CHECK(tail.size() == 2);
        CHECK(tail[0] == 4);
        CHECK(tail[1] == 5);
    }

    SUBCASE("subview() on dense width that crosses word boundaries") {
        using view_ld13 = packed_view<word_t, value_t, 13, pack_endian::lsb, pack_mode::dense>;

        word_t data[4]{};
        view_ld13 v(data, 6);

        constexpr value_t values[] = {0x001, 0x123, 0x456, 0x789, 0xabc, 0x1fff};
        for (std::size_t i = 0; i < std::size(values); ++i)
            v[i] = values[i];

        for (std::size_t off = 0; off < std::size(values); ++off) {
            auto sub = v.subview(off);
            CHECK(sub.size() == std::size(values) - off);
            for (std::size_t i = 0; i < sub.size(); ++i)
                CHECK(sub[i] == values[off + i]);
        }

        auto sub = v.subview(3);
        sub[0] = value_t{0x1eee};
        CHECK(v[3] == 0x1eee);
    }

    SUBCASE("data/offset/nb_words/span") {
        word_t data[2]{};
        view_t v(data, 6);

        CHECK(v.data() == data);
        CHECK(v.offset() == 0);
        CHECK(v.nb_words() == 1);

        auto sp = v.span();
        CHECK(sp.data() == data);
        CHECK(sp.size() == 1);

        auto sub = v.subview(2);
        CHECK(sub.offset() == 2);
    }

    SUBCASE("runtime width / sparse") {
        using view_rtw = packed_view<word_t, value_t, 0, pack_endian::lsb, pack_mode::sparse>;

        word_t data[2]{};
        constexpr std::uint8_t width = 5;
        view_rtw v(data, 6, width);

        v[0] = value_t{0x1a};
        v[1] = value_t{0x03};
        v[2] = value_t{0x11};

        CHECK(v[0] == 0x1a);
        CHECK(v[1] == 0x03);
        CHECK(v[2] == 0x11);
    }

    SUBCASE("runtime endian + runtime mode") {
        using view_rtem = packed_view<word_t, value_t, 13, pack_endian::rt, pack_mode::rt>;

        word_t data[4]{};
        view_rtem v(data, 6, 13, pack_endian::msb, pack_mode::dense);

        v[0] = value_t{0x123};
        v[1] = value_t{0x456};
        v[2] = value_t{0x789};

        CHECK(v[0] == 0x123);
        CHECK(v[1] == 0x456);
        CHECK(v[2] == 0x789);
    }

    SUBCASE("runtime width + runtime endian + runtime mode") {
        using view_rt = packed_view<word_t, value_t, 0, pack_endian::rt, pack_mode::rt>;

        word_t data[4]{};
        view_rt v(data, 6, 13, pack_endian::lsb, pack_mode::dense);

        v[0] = value_t{0x123};
        v[1] = value_t{0x456};

        CHECK(v[0] == 0x123);
        CHECK(v[1] == 0x456);

        auto sub = v.subview(1);
        CHECK(sub[0] == 0x456);
    }
}

TEST_CASE("core/packed/view — cross-layout round-trip") {
    SUBCASE("Word = uint64_t") {
        packed_view_sweep<std::uint64_t>();
    }
    SUBCASE("Word = uint32_t") {
        packed_view_sweep<std::uint32_t>();
    }
    SUBCASE("Word = uint16_t") {
        packed_view_sweep<std::uint16_t>();
    }
    SUBCASE("Word = uint8_t") {
        packed_view_sweep<std::uint8_t>();
    }
}
