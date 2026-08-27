#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include <doctest.h>

#include <cds/io/byte.hpp>
#include <cds/io/file.hpp>

#include "io_temp.hpp"

TEST_CASE("io/file — concepts") {
    using namespace cds::io;
    static_assert(byte_sink<file_sink>);
    static_assert(byte_source<file_source>);
    static_assert(!span_source<file_source>);
    CHECK(true);
}

TEST_CASE("io/file — vector round-trip") {
    using namespace cds::io;
    using T = std::uint32_t;

    const auto path = io_temp_path("file_rt");
    const std::vector<T> data = {11u, 22u, 33u, 44u, 55u, 66u};

    {
        auto sink = file_sink::open(path.string());
        REQUIRE(sink.has_value());
        REQUIRE(write_vector(*sink, data));
    }

    {
        auto src = file_source::open(path.string());
        REQUIRE(src.has_value());
        auto r = read_vector<T>(*src);
        REQUIRE(r.has_value());
        CHECK(*r == data);
    }

    std::filesystem::remove(path);
}

TEST_CASE("io/file — skip past the first record then read the second") {
    using namespace cds::io;
    using T = std::uint64_t;

    const auto path = io_temp_path("file_skip");
    const std::vector<T> first = {1u, 2u, 3u};
    const std::vector<T> second = {100u, 200u};

    {
        auto sink = file_sink::open(path.string());
        REQUIRE(sink.has_value());
        REQUIRE(write_vector(*sink, first));
        REQUIRE(write_vector(*sink, second));
    }

    {
        auto src = file_source::open(path.string());
        REQUIRE(src.has_value());
        REQUIRE(skip_vector<T>(*src));
        auto r = read_vector<T>(*src);
        REQUIRE(r.has_value());
        CHECK(*r == second);
    }

    std::filesystem::remove(path);
}

TEST_CASE("io/file — opening a missing file fails") {
    using namespace cds::io;
    auto src = file_source::open("/nonexistent/cds/definitely/not/here.bin");
    CHECK_FALSE(src.has_value());
}
