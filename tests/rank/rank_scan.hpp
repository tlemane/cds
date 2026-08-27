#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include <doctest.h>

#include <cds/bit/vector.hpp>
#include <cds/rank/scan.hpp>

namespace {

    template <typename Word, cds::pack_endian Endian>
    void run_rank_scan_case(std::size_t n, double density, std::uint64_t seed) {
        std::mt19937_64 rng(seed);
        std::bernoulli_distribution dist(density);

        cds::bit_vector_impl<Word, Endian> v;
        std::vector<std::size_t> oracle_rank1(n + 1, 0);

        for (std::size_t i = 0; i < n; ++i) {
            const bool bit = dist(rng);
            v.push_back(bit ? std::uint8_t{1} : std::uint8_t{0});
            oracle_rank1[i + 1] = oracle_rank1[i] + (bit ? 1u : 0u);
        }

        REQUIRE(v.size() == n);

        cds::rank_scan rs(v);
        CHECK(rs.size() == n);

        check_rank_sampled(rs, oracle_rank1, n);
    }

    template <typename Word>
    void run_rank_scan_all_layouts(std::size_t n, double density, std::uint64_t seed) {
        using cds::pack_endian;
        using cds::pack_mode;

        run_rank_scan_case<Word, pack_endian::lsb>(n, density, seed + 1);
        run_rank_scan_case<Word, pack_endian::msb>(n, density, seed + 3);
    }

    template <typename Word> void run_rank_scan_word_suite(std::uint64_t seed_base) {
        constexpr std::size_t digits = std::numeric_limits<Word>::digits;

        run_rank_scan_all_layouts<Word>(0, 0.5, seed_base + 1);
        run_rank_scan_all_layouts<Word>(1, 0.5, seed_base + 2);
        run_rank_scan_all_layouts<Word>(digits - 1, 0.5, seed_base + 3);
        run_rank_scan_all_layouts<Word>(digits, 0.5, seed_base + 4);
        run_rank_scan_all_layouts<Word>(digits + 1, 0.5, seed_base + 5);
        run_rank_scan_all_layouts<Word>(digits * 3 - 1, 0.5, seed_base + 6);
        run_rank_scan_all_layouts<Word>(digits * 3, 0.5, seed_base + 7);

        run_rank_scan_all_layouts<Word>(digits * 8, 0.0, seed_base + 8);
        run_rank_scan_all_layouts<Word>(digits * 8, 1.0, seed_base + 9);

        run_rank_scan_all_layouts<Word>(2000, 0.02, seed_base + 10);
        run_rank_scan_all_layouts<Word>(2000, 0.5, seed_base + 11);
        run_rank_scan_all_layouts<Word>(2000, 0.98, seed_base + 12);
    }

}

TEST_CASE("rank/rank_scan") {
    SUBCASE("Word = uint64_t") {
        run_rank_scan_word_suite<std::uint64_t>(1000);
    }
    SUBCASE("Word = uint32_t") {
        run_rank_scan_word_suite<std::uint32_t>(2000);
    }
    SUBCASE("Word = uint16_t") {
        run_rank_scan_word_suite<std::uint16_t>(3000);
    }
    SUBCASE("Word = uint8_t") {
        run_rank_scan_word_suite<std::uint8_t>(4000);
    }
}
