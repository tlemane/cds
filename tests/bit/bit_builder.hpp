#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include <doctest.h>

#include <cds/bit/builder.hpp>
#include <cds/bit/vector.hpp>

#include "bit_check.hpp"

namespace {

    template <typename Word, cds::pack_endian Endian> void run_bit_builder_fixed_case() {
        using cds::bit_vector;
        using cds::bit_vector_builder;

        constexpr std::size_t n = 200;
        bit_vector_builder<Word, Endian, true> b(n);
        CHECK(b.size() == n);

        std::vector<std::uint8_t> oracle(n, std::uint8_t{0});

        for (std::size_t idx : {std::size_t{1}, std::size_t{4}, std::size_t{7}, std::size_t{63},
                                std::size_t{64}, std::size_t{65}}) {
            b.set_bit(idx);
            oracle[idx] = 1;
        }

        const std::size_t pos = 120;
        const std::size_t count = 37;
        b.set_range(pos, count);
        for (std::size_t i = pos; i < pos + count; ++i)
            oracle[i] = 1;

        bit_vector<Word, Endian> v(std::move(b));
        REQUIRE(v.size() == n);
        check_bits_equal(v, oracle);
    }

    template <typename Word, cds::pack_endian Endian>
    void run_bit_builder_dynamic_case(std::uint64_t seed) {
        using cds::bit_vector;
        using cds::bit_vector_builder;

        bit_vector_builder<Word, Endian, false> b;
        std::vector<std::uint8_t> oracle;

        const auto lead = make_bits(5, 0.5, seed);
        for (std::uint8_t bit : lead) {
            b.push_bit(bit != 0);
            oracle.push_back(bit);
        }

        b.push_range(70, true);
        for (std::size_t i = 0; i < 70; ++i)
            oracle.push_back(1);

        b.push_range(50, false);
        for (std::size_t i = 0; i < 50; ++i)
            oracle.push_back(0);

        const auto tail = make_bits(33, 0.5, seed + 1);
        for (std::uint8_t bit : tail) {
            b.push_bit(bit != 0);
            oracle.push_back(bit);
        }

        CHECK(b.size() == oracle.size());

        bit_vector<Word, Endian> v(std::move(b));
        REQUIRE(v.size() == oracle.size());
        check_bits_equal(v, oracle);
    }

    template <typename Word> void run_bit_builder_all_layouts(std::uint64_t seed) {
        using cds::pack_endian;
        using cds::pack_mode;

        run_bit_builder_fixed_case<Word, pack_endian::lsb>();
        run_bit_builder_fixed_case<Word, pack_endian::msb>();

        run_bit_builder_dynamic_case<Word, pack_endian::lsb>(seed + 10);
        run_bit_builder_dynamic_case<Word, pack_endian::msb>(seed + 30);
    }

}

TEST_CASE("bit/bit_builder") {
    SUBCASE("Word = uint64_t") {
        run_bit_builder_all_layouts<std::uint64_t>(42000);
    }
    SUBCASE("Word = uint32_t") {
        run_bit_builder_all_layouts<std::uint32_t>(43000);
    }
    SUBCASE("Word = uint16_t") {
        run_bit_builder_all_layouts<std::uint16_t>(44000);
    }
    SUBCASE("Word = uint8_t") {
        run_bit_builder_all_layouts<std::uint8_t>(45000);
    }
}
