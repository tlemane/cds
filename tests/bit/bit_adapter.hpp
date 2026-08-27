#pragma once

#include <cstdint>

#include <doctest.h>

#include <cds/bit/array.hpp>
#include <cds/bit/vector.hpp>

#include "bit_check.hpp"

namespace {

    using adapter_word = std::uint64_t;
    inline constexpr cds::pack_endian adapter_endian = cds::pack_endian::lsb;

} // namespace

TEST_CASE("bit/bit_dynamic_adapter") {
    using bv_t = cds::bit_vector<adapter_word, adapter_endian>;

    cds::bit_dynamic_adapter<bv_t> a;
    cds::bit_dynamic_interface& itf = a;

    CHECK(itf.empty());

    itf.push_back(std::uint8_t{1});
    itf.push_back(std::uint8_t{0});
    itf.push_back(std::uint8_t{1});
    itf.push_back(std::uint8_t{0});
    CHECK(itf.size() == 4);
    CHECK_FALSE(itf.empty());
    CHECK(itf.get(0) == 1);
    CHECK(itf.get(1) == 0);

    itf.set(1);
    CHECK(itf.get(1) == 1);
    itf.set(1, std::uint8_t{0});
    CHECK(itf.get(1) == 0);

    itf.clear(0);
    CHECK(itf.get(0) == 0);

    itf.flip(2);
    CHECK(itf.get(2) == 0);
    itf.flip(2);
    CHECK(itf.get(2) == 1);

    itf.pop_back();
    CHECK(itf.size() == 3);

    CHECK(a.underlying().size() == 3);
}

TEST_CASE("bit/bit_adapter (writable, non-growable)") {
    using ba_t = cds::bit_array<adapter_word, 128, adapter_endian>;

    cds::bit_adapter<ba_t> a;

    for (std::size_t i = 0; i < 8; ++i)
        a.underlying().push_back(std::uint8_t{0});

    cds::bit_interface& itf = a;
    CHECK(itf.size() == 8);
    CHECK_FALSE(itf.empty());

    itf.set(3);
    CHECK(itf.get(3) == 1);
    itf.set(4, std::uint8_t{1});
    CHECK(itf.get(4) == 1);
    itf.clear(3);
    CHECK(itf.get(3) == 0);
    itf.flip(4);
    CHECK(itf.get(4) == 0);
}

TEST_CASE("bit/const_bit_adapter (read-only)") {
    using ba_t = cds::bit_array<adapter_word, 128, adapter_endian>;

    ba_t src;
    const std::uint8_t pattern[] = {1, 0, 1, 1, 0, 0, 1, 0};
    for (std::uint8_t b : pattern)
        src.push_back(b);

    cds::const_bit_adapter<ba_t> a(src);
    const cds::bit_const_interface& itf = a;

    CHECK(itf.size() == 8);
    CHECK_FALSE(itf.empty());
    for (std::size_t i = 0; i < 8; ++i) {
        CAPTURE(i);
        CHECK(itf.get(i) == pattern[i]);
    }
}
