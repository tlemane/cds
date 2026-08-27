#pragma once

#include <cstdint>
#include <limits>
#include <random>
#include <sstream>
#include <vector>

#include <doctest.h>

#include <cds/io/buffer.hpp>
#include <cds/bit/vector.hpp>
#include <cds/rank/poppy.hpp>

namespace {

    template <typename Word, cds::pack_endian Endian, std::uint64_t UpperBlockBits>
    void run_poppy_case(std::size_t n, double density, std::uint64_t seed) {
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

        cds::rank_poppy<source_type, UpperBlockBits> rp(v);
        CHECK(rp.size() == n);
        CHECK(rp.data() == cds::bit_source_traits<source_type>::data(v));

        check_rank_sampled(rp, oracle_rank1, n);

        cds::rank_poppy_view<source_type, UpperBlockBits> rpv(rp.l0(), rp.l1l2(), v);
        CHECK(rpv.size() == n);

        check_rank_sampled(rpv, oracle_rank1, n);

        cds::io::buffer_sink sink;
        REQUIRE(rp.save(sink));

        auto bytes = sink.release();
        cds::io::buffer_source source(bytes);

        auto rp_loaded_result = cds::rank_poppy<source_type, UpperBlockBits>::load(source, v);
        REQUIRE(rp_loaded_result.has_value());
        auto& rp_loaded = *rp_loaded_result;

        CHECK(rp_loaded.size() == n);

        check_rank_sampled(rp_loaded, oracle_rank1, n);

        cds::io::buffer_source view_source(bytes);
        auto rpv_loaded_result =
            cds::rank_poppy_view<source_type, UpperBlockBits>::load(view_source, v);
        REQUIRE(rpv_loaded_result.has_value());
        auto& rpv_loaded = *rpv_loaded_result;

        CHECK(rpv_loaded.size() == n);

        check_rank_sampled(rpv_loaded, oracle_rank1, n);
    }

    template <typename Word, std::uint64_t UpperBlockBits>
    void run_poppy_all_layouts(std::size_t n, double density, std::uint64_t seed) {
        using cds::pack_endian;
        using cds::pack_mode;

        run_poppy_case<Word, pack_endian::lsb, UpperBlockBits>(n, density, seed + 1);
        run_poppy_case<Word, pack_endian::msb, UpperBlockBits>(n, density, seed + 3);
    }

    template <typename Word> void run_poppy_word_suite(std::uint64_t seed_base) {
        constexpr std::size_t digits = std::numeric_limits<Word>::digits;
        constexpr std::size_t bb_bits = 512;
        constexpr std::size_t lb_bits = 2048;

        constexpr std::uint64_t default_upper = std::uint64_t{1} << 32;

        run_poppy_all_layouts<Word, default_upper>(0, 0.5, seed_base + 1);
        run_poppy_all_layouts<Word, default_upper>(1, 0.5, seed_base + 2);

        run_poppy_all_layouts<Word, default_upper>(digits - 1, 0.5, seed_base + 3);
        run_poppy_all_layouts<Word, default_upper>(digits, 0.5, seed_base + 4);
        run_poppy_all_layouts<Word, default_upper>(digits + 1, 0.5, seed_base + 5);

        run_poppy_all_layouts<Word, default_upper>(bb_bits - 1, 0.5, seed_base + 6);
        run_poppy_all_layouts<Word, default_upper>(bb_bits, 0.5, seed_base + 7);
        run_poppy_all_layouts<Word, default_upper>(bb_bits + 1, 0.5, seed_base + 8);

        run_poppy_all_layouts<Word, default_upper>(lb_bits - 1, 0.5, seed_base + 9);
        run_poppy_all_layouts<Word, default_upper>(lb_bits, 0.5, seed_base + 10);
        run_poppy_all_layouts<Word, default_upper>(lb_bits + 1, 0.5, seed_base + 11);

        run_poppy_all_layouts<Word, default_upper>(lb_bits * 3 - 1, 0.5, seed_base + 12);
        run_poppy_all_layouts<Word, default_upper>(lb_bits * 3, 0.5, seed_base + 13);
        run_poppy_all_layouts<Word, default_upper>(lb_bits * 3 + 17, 0.5, seed_base + 14);

        run_poppy_all_layouts<Word, default_upper>(lb_bits * 4, 0.0, seed_base + 15);
        run_poppy_all_layouts<Word, default_upper>(lb_bits * 4, 1.0, seed_base + 16);

        run_poppy_all_layouts<Word, default_upper>(lb_bits * 5 + 31, 0.02, seed_base + 17);
        run_poppy_all_layouts<Word, default_upper>(lb_bits * 5 + 31, 0.5, seed_base + 18);
        run_poppy_all_layouts<Word, default_upper>(lb_bits * 5 + 31, 0.98, seed_base + 19);

        constexpr std::uint64_t small_upper = 4096;

        run_poppy_all_layouts<Word, small_upper>(small_upper - 1, 0.5, seed_base + 20);
        run_poppy_all_layouts<Word, small_upper>(small_upper, 0.5, seed_base + 21);
        run_poppy_all_layouts<Word, small_upper>(small_upper + 1, 0.5, seed_base + 22);

        run_poppy_all_layouts<Word, small_upper>(small_upper * 2, 0.5, seed_base + 23);
        run_poppy_all_layouts<Word, small_upper>(small_upper * 3 + 123, 0.5, seed_base + 24);

        run_poppy_all_layouts<Word, small_upper>(small_upper * 3, 0.02, seed_base + 25);
        run_poppy_all_layouts<Word, small_upper>(small_upper * 3, 0.98, seed_base + 26);
    }

}

TEST_CASE("rank/rank_poppy") {
    SUBCASE("Word = uint64_t") {
        run_poppy_word_suite<std::uint64_t>(13000);
    }
    SUBCASE("Word = uint32_t") {
        run_poppy_word_suite<std::uint32_t>(14000);
    }
    SUBCASE("Word = uint16_t") {
        run_poppy_word_suite<std::uint16_t>(15000);
    }
    SUBCASE("Word = uint8_t") {
        run_poppy_word_suite<std::uint8_t>(16000);
    }
}
