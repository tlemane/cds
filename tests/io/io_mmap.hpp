#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

#include <doctest.h>

#include <cds/io/mmap.hpp>

#include "io_temp.hpp"

TEST_CASE("io/mmap — align_down / page_size") {
    using namespace cds::io;

    CHECK(align_down(0, 4096) == 0);
    CHECK(align_down(1, 4096) == 0);
    CHECK(align_down(4095, 4096) == 0);
    CHECK(align_down(4096, 4096) == 4096);
    CHECK(align_down(4097, 4096) == 4096);
    CHECK(align_down(8191, 4096) == 4096);

    CHECK(page_size() > 0);
}

TEST_CASE("io/mmap — create, write, sync, reopen read-only") {
    using namespace cds::io;
    using T = std::uint64_t;

    const auto path = io_temp_path("mmap");
    constexpr std::size_t n = 1000;

    {
        auto m = ommap<T>::create(path.string(), n);
        REQUIRE(m.has_value());
        CHECK(m->is_mapped());
        CHECK(m->size() == n);

        auto sp = m->view();
        REQUIRE(sp.size() == n);
        for (std::size_t i = 0; i < n; ++i)
            sp[i] = static_cast<T>(i * 2654435761u + 1u);

        REQUIRE(m->sync().has_value());
    }

    {
        auto m = immap<T>::open_whole_file(path.string());
        REQUIRE(m.has_value());
        CHECK(m->size() == n);

        auto sp = m->view();
        REQUIRE(sp.size() == n);
        for (std::size_t i = 0; i < n; ++i) {
            CAPTURE(i);
            CHECK(sp[i] == static_cast<T>(i * 2654435761u + 1u));
        }
    }

    std::filesystem::remove(path);
}

TEST_CASE("io/mmap — element_offset maps a sub-range") {
    using namespace cds::io;
    using T = std::uint32_t;

    const auto path = io_temp_path("mmap_off");
    constexpr std::size_t n = 4096;

    {
        auto m = ommap<T>::create(path.string(), n);
        REQUIRE(m.has_value());
        auto sp = m->view();
        for (std::size_t i = 0; i < n; ++i)
            sp[i] = static_cast<T>(i);
        REQUIRE(m->sync().has_value());
    }

    {

        constexpr std::size_t off = n / 2;
        auto m = immap<T>::open(path.string(), n - off, off);
        REQUIRE(m.has_value());
        CHECK(m->size() == n - off);
        auto sp = m->view();
        for (std::size_t i = 0; i < sp.size(); ++i) {
            CAPTURE(i);
            CHECK(sp[i] == static_cast<T>(off + i));
        }
    }

    std::filesystem::remove(path);
}

TEST_CASE("io/mmap — opening a missing file fails") {
    using namespace cds::io;
    auto m = immap<std::uint64_t>::open_whole_file("/nonexistent/cds/definitely/not/here.bin");
    CHECK_FALSE(m.has_value());
}

TEST_CASE("io/mmap — default-constructed is empty and unmapped") {
    using namespace cds::io;
    immap<std::uint64_t> m;
    CHECK_FALSE(m.is_mapped());
    CHECK(m.size() == 0);
    CHECK(m.view().empty());
}
