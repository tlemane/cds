#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <doctest.h>

#include <cds/io/byte.hpp>
#include <cds/io/buffer.hpp>
#include <cds/version.hpp>

TEST_CASE("io/byte — concepts") {
    using namespace cds::io;

    static_assert(byte_sink<buffer_sink>);
    static_assert(!byte_source<buffer_sink>);
    static_assert(byte_source<buffer_source>);
    static_assert(span_source<buffer_source>);
    static_assert(!byte_sink<buffer_source>);
    static_assert(mutable_span_source<mutable_buffer_source>);
    static_assert(span_source<mutable_buffer_source>);

    CHECK(true);
}

TEST_CASE("io/byte — cds version round-trip") {
    using namespace cds::io;

    buffer_sink sink;
    REQUIRE(write_cds_version(sink));

    auto bytes = sink.release();
    buffer_source src(bytes);

    auto v = read_cds_version_compatible(src);
    REQUIRE(v.has_value());
    CHECK(v->major == CDS_VERSION_MAJOR);
    CHECK(v->minor == CDS_VERSION_MINOR);
    CHECK(v->patch == CDS_VERSION_PATCH);
    CHECK(src.remaining() == 0);
}

TEST_CASE("io/byte — cds version errors") {
    using namespace cds::io;

    SUBCASE("wrong major -> bad_version") {
        cds_version_header h{static_cast<std::uint16_t>(CDS_VERSION_MAJOR + 1), 0, 0};
        buffer_sink sink;
        REQUIRE(sink.write(&h, sizeof(h)));
        auto bytes = sink.release();
        buffer_source src(bytes);

        auto v = read_cds_version_compatible(src);
        REQUIRE_FALSE(v.has_value());
        CHECK(v.error() == load_error::bad_version);
    }

    SUBCASE("truncated -> io_failure") {
        std::vector<std::byte> empty;
        buffer_source src(empty);
        auto v = read_cds_version_compatible(src);
        REQUIRE_FALSE(v.has_value());
        CHECK(v.error() == load_error::io_failure);
    }
}

TEST_CASE("io/byte — format header magic/version") {
    using namespace cds::io;

    constexpr std::uint32_t magic = 0xDEADBEEFu;
    constexpr std::uint32_t version = 3;

    buffer_sink sink;
    REQUIRE(write_header(sink, magic, version));
    auto bytes = sink.release();

    {
        buffer_source src(bytes);
        CHECK(read_header(src, magic, version));
    }
    {
        buffer_source src(bytes);
        CHECK_FALSE(read_header(src, 0x12345678u, version));
    }
    {
        buffer_source src(bytes);
        CHECK_FALSE(read_header(src, magic, version + 1));
    }
    {
        std::vector<std::byte> empty;
        buffer_source src(empty);
        CHECK_FALSE(read_header(src, magic, version));
    }
}

TEST_CASE("io/byte — length-prefixed vector helpers") {
    using namespace cds::io;
    using T = std::uint32_t;

    const std::vector<T> data = {1u, 2u, 3u, 5u, 8u, 13u, 21u, 34u};

    buffer_sink sink;
    REQUIRE(write_vector(sink, data));
    auto bytes = sink.release();

    SUBCASE("read_vector copies back an equal vector") {
        buffer_source src(bytes);
        auto r = read_vector<T>(src);
        REQUIRE(r.has_value());
        CHECK(*r == data);
        CHECK(src.remaining() == 0);
    }

    SUBCASE("skip_vector advances past the whole record") {
        buffer_source src(bytes);
        CHECK(skip_vector<T>(src));
        CHECK(src.remaining() == 0);
    }

    SUBCASE("view_vector is zero-copy: span points into the source buffer") {
        buffer_source src(bytes);
        auto r = view_vector<T>(src);
        REQUIRE(r.has_value());
        REQUIRE(r->size() == data.size());
        for (std::size_t i = 0; i < data.size(); ++i) {
            CAPTURE(i);
            CHECK((*r)[i] == data[i]);
        }

        const auto* p = reinterpret_cast<const std::byte*>(r->data());
        CHECK(p >= bytes.data());
        CHECK(p < bytes.data() + bytes.size());
    }
}

TEST_CASE("io/byte — empty vector round-trips") {
    using namespace cds::io;
    using T = std::uint64_t;

    const std::vector<T> empty;
    buffer_sink sink;
    REQUIRE(write_vector(sink, empty));
    auto bytes = sink.release();

    buffer_source src(bytes);
    auto r = read_vector<T>(src);
    REQUIRE(r.has_value());
    CHECK(r->empty());

    buffer_source src2(bytes);
    auto v = view_vector<T>(src2);
    REQUIRE(v.has_value());
    CHECK(v->empty());
}

TEST_CASE("io/byte — truncated payload fails") {
    using namespace cds::io;
    using T = std::uint32_t;

    const std::uint64_t n = 4;
    buffer_sink sink;
    REQUIRE(sink.write(&n, sizeof(n)));
    auto bytes = sink.release();

    {
        buffer_source src(bytes);
        auto r = read_vector<T>(src);
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error() == load_error::io_failure);
    }
    {
        buffer_source src(bytes);
        auto r = view_vector<T>(src);
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error() == load_error::io_failure);
    }
}
