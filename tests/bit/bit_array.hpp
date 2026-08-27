#pragma once

#include <cstdint>

#include <doctest.h>

#include <cds/io/buffer.hpp>
#include <cds/bit/array.hpp>

#include "bit_check.hpp"

namespace {

    template <typename Word, std::size_t Capacity, cds::pack_endian Endian>
    void run_bit_array_case(std::size_t n, double density, std::uint64_t seed) {
        using ba_t = cds::bit_array<Word, Capacity, Endian>;
        static_assert(cds::bit_source<ba_t>, "bit_array must satisfy bit_source");

        REQUIRE(n <= Capacity);
        const auto bits = make_bits(n, density, seed);

        ba_t a;
        CHECK(a.empty());
        CHECK(a.capacity() == Capacity);

        for (std::uint8_t b : bits)
            a.push_back(b);

        REQUIRE(a.size() == n);
        check_bits_equal(a, bits);
        CHECK(a.popcount() == popcount_of(bits));

        if (n > 0) {
            CHECK(static_cast<std::uint8_t>(a.front()) == bits.front());
            CHECK(static_cast<std::uint8_t>(a.back()) == bits.back());
        }

        {
            ba_t c = a;
            REQUIRE(c.size() == n);
            check_bits_equal(c, bits);
            if (n > 0) {
                c.flip(0);
                CHECK(static_cast<std::uint8_t>(c[0]) != static_cast<std::uint8_t>(a[0]));
            }
        }

        {
            ba_t c = a;
            c.clear();
            CHECK(c.empty());
            CHECK(c.size() == 0);
        }

        {
            cds::io::buffer_sink sink;
            REQUIRE(a.save(sink));

            auto bytes = sink.release();
            cds::io::buffer_source source(bytes);

            auto loaded = ba_t::load(source);
            REQUIRE(loaded.has_value());
            check_bits_equal(*loaded, bits);
        }
    }

    template <typename Word, std::size_t Capacity>
    void run_bit_array_all_layouts(std::size_t n, double density, std::uint64_t seed) {
        using cds::pack_endian;
        using cds::pack_mode;

        run_bit_array_case<Word, Capacity, pack_endian::lsb>(n, density, seed + 1);
        run_bit_array_case<Word, Capacity, pack_endian::msb>(n, density, seed + 3);
    }

    template <typename Word> void run_bit_array_word_suite(std::uint64_t seed_base) {
        run_bit_array_all_layouts<Word, 256>(0, 0.5, seed_base + 1);
        run_bit_array_all_layouts<Word, 256>(1, 0.5, seed_base + 2);
        run_bit_array_all_layouts<Word, 256>(65, 0.5, seed_base + 3);
        run_bit_array_all_layouts<Word, 256>(200, 0.02, seed_base + 4);
        run_bit_array_all_layouts<Word, 256>(200, 0.98, seed_base + 5);
        run_bit_array_all_layouts<Word, 256>(256, 0.5, seed_base + 6);
    }

} // namespace

TEST_CASE("bit/bit_array") {
    SUBCASE("Word = uint64_t") {
        run_bit_array_word_suite<std::uint64_t>(34000);
    }
    SUBCASE("Word = uint32_t") {
        run_bit_array_word_suite<std::uint32_t>(35000);
    }
    SUBCASE("Word = uint16_t") {
        run_bit_array_word_suite<std::uint16_t>(36000);
    }
    SUBCASE("Word = uint8_t") {
        run_bit_array_word_suite<std::uint8_t>(37000);
    }
}
