#pragma once

#include <cstdint>
#include <vector>

#include <doctest.h>

#include <cds/bit/vector.hpp>
#include <cds/bit/array.hpp>

#include "bit_check.hpp"

namespace {

    template <typename Bit> void exercise_mutation(Bit& b, std::size_t n) {
        REQUIRE(b.size() == n);

        std::vector<std::uint8_t> oracle(n, std::uint8_t{0});
        for (std::size_t i = 0; i < n; ++i) {
            CAPTURE(i);
            CHECK(b.get(i) == false);
        }
        CHECK(b.popcount() == 0);

        for (std::size_t i = 0; i < n; i += 3) {
            b.set(i);
            oracle[i] = 1;
        }
        for (std::size_t i = 0; i < n; ++i) {
            CAPTURE(i);
            CHECK(static_cast<std::uint8_t>(b[i]) == oracle[i]);
            CHECK(b.get(i) == (oracle[i] != 0));
        }
        CHECK(b.popcount() == popcount_of(oracle));

        for (std::size_t i = 0; i < n; i += 6) {
            b.clear(i);
            oracle[i] = 0;
        }
        for (std::size_t i = 0; i < n; ++i) {
            CAPTURE(i);
            CHECK(static_cast<std::uint8_t>(b[i]) == oracle[i]);
        }

        for (std::size_t i = 0; i < n; ++i) {
            b.flip(i);
            oracle[i] = static_cast<std::uint8_t>(oracle[i] ^ 1);
        }
        check_bits_equal(b, oracle);
        CHECK(b.popcount() == popcount_of(oracle));

        for (std::size_t i = 0; i < n; ++i) {
            const bool val = (i % 5 == 0);
            b.set(i, val);
            oracle[i] = val ? std::uint8_t{1} : std::uint8_t{0};
        }
        check_bits_equal(b, oracle);

        if (n > 0) {
            b.set_one(0);
            oracle[0] = 1;
            b.set_zero(n - 1);
            oracle[n - 1] = 0;
            check_bits_equal(b, oracle);
        }
    }

    template <typename Word, cds::pack_endian Endian> void run_bit_mutation_case(std::size_t n) {
        {
            cds::bit_vector<Word, Endian> v;
            v.resize(n, std::uint8_t{0});
            exercise_mutation(v, n);
        }

        {
            cds::bit_array<Word, 160, Endian> a;
            for (std::size_t i = 0; i < n; ++i)
                a.push_back(std::uint8_t{0});
            exercise_mutation(a, n);
        }
    }

    template <typename Word> void run_bit_mutation_all_layouts(std::size_t n) {
        using cds::pack_endian;
        using cds::pack_mode;

        run_bit_mutation_case<Word, pack_endian::lsb>(n);
        run_bit_mutation_case<Word, pack_endian::msb>(n);
    }

} // namespace

TEST_CASE("bit/bit_mutation") {
    SUBCASE("Word = uint64_t") {
        run_bit_mutation_all_layouts<std::uint64_t>(150);
    }
    SUBCASE("Word = uint32_t") {
        run_bit_mutation_all_layouts<std::uint32_t>(150);
    }
    SUBCASE("Word = uint16_t") {
        run_bit_mutation_all_layouts<std::uint16_t>(150);
    }
    SUBCASE("Word = uint8_t") {
        run_bit_mutation_all_layouts<std::uint8_t>(150);
    }
}
