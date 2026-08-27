#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include <doctest.h>

#include <cds/io/buffer.hpp>

TEST_CASE("io/buffer — sink accumulates and releases") {
    using namespace cds::io;

    buffer_sink sink;
    CHECK(sink.size() == 0);

    const std::uint32_t a = 0xAABBCCDDu;
    const std::uint16_t b = 0x1234u;
    REQUIRE(sink.write(&a, sizeof(a)));
    REQUIRE(sink.write(&b, sizeof(b)));
    CHECK(sink.size() == sizeof(a) + sizeof(b));
    CHECK(sink.data().size() == sizeof(a) + sizeof(b));

    auto bytes = sink.release();
    CHECK(bytes.size() == sizeof(a) + sizeof(b));
    CHECK(sink.size() == 0);

    buffer_source src(bytes);
    std::uint32_t a2 = 0;
    std::uint16_t b2 = 0;
    REQUIRE(src.read(&a2, sizeof(a2)));
    REQUIRE(src.read(&b2, sizeof(b2)));
    CHECK(a2 == a);
    CHECK(b2 == b);
}

TEST_CASE("io/buffer — source read/skip/view bounds") {
    using namespace cds::io;

    std::vector<std::byte> buf(10);
    for (std::size_t i = 0; i < buf.size(); ++i)
        buf[i] = static_cast<std::byte>(i);

    SUBCASE("read within then past the end") {
        buffer_source src(buf);
        std::byte tmp[6];
        REQUIRE(src.read(tmp, 6));
        CHECK(src.remaining() == 4);
        CHECK_FALSE(src.read(tmp, 6));
        CHECK(src.remaining() == 4);
    }

    SUBCASE("skip within then past the end") {
        buffer_source src(buf);
        CHECK(src.skip(4));
        CHECK(src.remaining() == 6);
        CHECK_FALSE(src.skip(7));
        CHECK(src.remaining() == 6);
    }

    SUBCASE("view aliases the buffer and advances") {
        buffer_source src(buf);
        auto sp = src.view(4);
        REQUIRE(sp.size() == 4);
        CHECK(sp.data() == buf.data());
        CHECK(src.remaining() == 6);

        auto sp2 = src.view(4);
        REQUIRE(sp2.size() == 4);
        CHECK(sp2.data() == buf.data() + 4);

        auto bad = src.view(4);
        CHECK(bad.empty());
        CHECK(src.remaining() == 2);
    }
}

TEST_CASE("io/buffer — mutable_buffer_source writes through view_mut") {
    using namespace cds::io;

    std::vector<std::byte> buf(8, std::byte{0});
    mutable_buffer_source src(buf);

    auto w = src.view_mut(4);
    REQUIRE(w.size() == 4);
    for (std::size_t i = 0; i < 4; ++i)
        w[i] = static_cast<std::byte>(0xF0 + i);

    for (std::size_t i = 0; i < 4; ++i) {
        CAPTURE(i);
        CHECK(buf[i] == static_cast<std::byte>(0xF0 + i));
    }
    CHECK(src.remaining() == 4);

    auto r = src.view(4);
    REQUIRE(r.size() == 4);
    CHECK(r.data() == buf.data() + 4);
}
