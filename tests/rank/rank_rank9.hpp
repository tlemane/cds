#pragma once

#include <cstdint>
#include <limits>
#include <random>
#include <sstream>
#include <vector>

#include <doctest.h>

#include <cds/io/buffer.hpp>
#include <cds/bit/vector.hpp>
#include <cds/rank/rank9.hpp>

namespace {

    template <typename Word, cds::pack_endian Endian>
    void run_rank9_case(std::size_t n, double density, std::uint64_t seed) {
        using source_type = cds::bit_vector_impl<Word, Endian>;

        std::mt19937_64 rng(seed);
        std::bernoulli_distribution dist(density);

        source_type v;
        std::vector<std::size_t> oracle_rank1(n + 1, 0);

        for (std::size_t i = 0; i < n; ++i) {
            const bool bit = dist(rng);
            v.push_back(bit ? std::uint8_t{1} : std::uint8_t{0});
            oracle_rank1[i + 1] = oracle_rank1[i] + (bit ? 1u : 0u);
        }

        REQUIRE(v.size() == n);

        cds::rank9<source_type> r9(v);
        CHECK(r9.size() == n);
        CHECK(r9.data() == cds::bit_source_traits<source_type>::data(v));

        check_rank_sampled(r9, oracle_rank1, n);

        cds::rank9_view<source_type> r9v(r9.superblocks(), v);
        CHECK(r9v.size() == n);

        check_rank_sampled(r9v, oracle_rank1, n);

        cds::io::buffer_sink sink;
        REQUIRE(r9.save(sink));

        auto bytes = sink.release();
        cds::io::buffer_source source(bytes);

        auto r9_loaded_result = cds::rank9<source_type>::load(source, v);
        REQUIRE(r9_loaded_result.has_value());
        auto& r9_loaded = *r9_loaded_result;

        CHECK(r9_loaded.size() == n);

        check_rank_sampled(r9_loaded, oracle_rank1, n);
    }

    template <typename Word>
    void run_rank9_all_layouts(std::size_t n, double density, std::uint64_t seed) {
        using cds::pack_endian;
        using cds::pack_mode;

        run_rank9_case<Word, pack_endian::lsb>(n, density, seed + 1);
        run_rank9_case<Word, pack_endian::msb>(n, density, seed + 3);
    }

    template <typename Word> void run_rank9_word_suite(std::uint64_t seed_base) {
        constexpr std::size_t digits = std::numeric_limits<Word>::digits;
        constexpr std::size_t sb_bits = 8 * digits;

        run_rank9_all_layouts<Word>(0, 0.5, seed_base + 1);
        run_rank9_all_layouts<Word>(1, 0.5, seed_base + 2);

        run_rank9_all_layouts<Word>(digits - 1, 0.5, seed_base + 3);
        run_rank9_all_layouts<Word>(digits, 0.5, seed_base + 4);
        run_rank9_all_layouts<Word>(digits + 1, 0.5, seed_base + 5);

        run_rank9_all_layouts<Word>(sb_bits - 1, 0.5, seed_base + 6);
        run_rank9_all_layouts<Word>(sb_bits, 0.5, seed_base + 7);
        run_rank9_all_layouts<Word>(sb_bits + 1, 0.5, seed_base + 8);
        run_rank9_all_layouts<Word>(sb_bits * 3 - 1, 0.5, seed_base + 9);
        run_rank9_all_layouts<Word>(sb_bits * 3, 0.5, seed_base + 10);
        run_rank9_all_layouts<Word>(sb_bits * 3 + 17, 0.5, seed_base + 11);

        run_rank9_all_layouts<Word>(sb_bits * 4, 0.0, seed_base + 12);
        run_rank9_all_layouts<Word>(sb_bits * 4, 1.0, seed_base + 13);

        run_rank9_all_layouts<Word>(sb_bits * 5 + 31, 0.02, seed_base + 14);
        run_rank9_all_layouts<Word>(sb_bits * 5 + 31, 0.5, seed_base + 15);
        run_rank9_all_layouts<Word>(sb_bits * 5 + 31, 0.98, seed_base + 16);
    }

}

TEST_CASE("rank/rank9") {
    SUBCASE("Word = uint64_t") {
        run_rank9_word_suite<std::uint64_t>(9000);
    }
    SUBCASE("Word = uint32_t") {
        run_rank9_word_suite<std::uint32_t>(10000);
    }
    SUBCASE("Word = uint16_t") {
        run_rank9_word_suite<std::uint16_t>(11000);
    }
    SUBCASE("Word = uint8_t") {
        run_rank9_word_suite<std::uint8_t>(12000);
    }
}
