#pragma once

#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include <doctest.h>

#include <cds/io/buffer.hpp>
#include <cds/bit/vector.hpp>
#include <cds/rank/poppy.hpp>
#include <cds/select/poppy.hpp>

namespace {

    template <typename Word, cds::pack_endian Endian, std::uint64_t UpperBlockBits>
    void run_select_poppy_case(std::size_t n, double density, std::uint64_t seed) {
        using source_type = cds::bit_vector_impl<Word, Endian>;
        using cds::select_target;

        std::mt19937_64 rng(seed);
        std::bernoulli_distribution dist(density);

        source_type v;
        std::vector<std::size_t> one_positions;
        std::vector<std::size_t> zero_positions;

        for (std::size_t i = 0; i < n; ++i) {
            const bool bit = dist(rng);
            v.push_back(bit ? std::uint8_t{1} : std::uint8_t{0});
            (bit ? one_positions : zero_positions).push_back(i);
        }

        REQUIRE(v.size() == n);
        REQUIRE(one_positions.size() + zero_positions.size() == n);

        cds::rank_poppy<source_type, UpperBlockBits> rp(v);

        cds::select_poppy<source_type, select_target::both, UpperBlockBits> sp(rp);
        CHECK(sp.size() == n);

        check_select_sampled([&](std::size_t r) { return sp.select1(r); }, one_positions);
        check_select_sampled([&](std::size_t r) { return sp.select0(r); }, zero_positions);

        cds::io::buffer_sink sink;
        REQUIRE(sp.save(sink));

        auto bytes = sink.release();
        cds::io::buffer_source source(bytes);

        auto sp_loaded_result =
            cds::select_poppy<source_type, select_target::both, UpperBlockBits>::load(source, rp);
        REQUIRE(sp_loaded_result.has_value());
        auto& sp_loaded = *sp_loaded_result;

        check_select_sampled([&](std::size_t r) { return sp_loaded.select1(r); }, one_positions);
        check_select_sampled([&](std::size_t r) { return sp_loaded.select0(r); }, zero_positions);

        if (!one_positions.empty()) {
            cds::select_poppy<source_type, select_target::ones, UpperBlockBits> sp_ones(rp);
            check_select_sampled([&](std::size_t r) { return sp_ones.select1(r); }, one_positions);
        }
        if (!zero_positions.empty()) {
            cds::select_poppy<source_type, select_target::zeros, UpperBlockBits> sp_zeros(rp);
            check_select_sampled([&](std::size_t r) { return sp_zeros.select0(r); },
                                 zero_positions);
        }
    }

    template <typename Word, std::uint64_t UpperBlockBits>
    void run_select_poppy_all_layouts(std::size_t n, double density, std::uint64_t seed) {
        using cds::pack_endian;
        using cds::pack_mode;

        run_select_poppy_case<Word, pack_endian::lsb, UpperBlockBits>(n, density, seed + 1);
        run_select_poppy_case<Word, pack_endian::msb, UpperBlockBits>(n, density, seed + 3);
    }

    template <typename Word> void run_select_poppy_word_suite(std::uint64_t seed_base) {
        constexpr std::size_t digits = std::numeric_limits<Word>::digits;
        constexpr std::size_t bb_bits = 512;
        constexpr std::size_t lb_bits = 2048;

        constexpr std::uint64_t default_upper = std::uint64_t{1} << 32;

        run_select_poppy_all_layouts<Word, default_upper>(0, 0.5, seed_base + 1);
        run_select_poppy_all_layouts<Word, default_upper>(1, 0.5, seed_base + 2);

        run_select_poppy_all_layouts<Word, default_upper>(digits - 1, 0.5, seed_base + 3);
        run_select_poppy_all_layouts<Word, default_upper>(digits, 0.5, seed_base + 4);
        run_select_poppy_all_layouts<Word, default_upper>(digits + 1, 0.5, seed_base + 5);

        run_select_poppy_all_layouts<Word, default_upper>(bb_bits - 1, 0.5, seed_base + 6);
        run_select_poppy_all_layouts<Word, default_upper>(bb_bits, 0.5, seed_base + 7);
        run_select_poppy_all_layouts<Word, default_upper>(bb_bits + 1, 0.5, seed_base + 8);

        run_select_poppy_all_layouts<Word, default_upper>(lb_bits - 1, 0.5, seed_base + 9);
        run_select_poppy_all_layouts<Word, default_upper>(lb_bits, 0.5, seed_base + 10);
        run_select_poppy_all_layouts<Word, default_upper>(lb_bits + 1, 0.5, seed_base + 11);

        run_select_poppy_all_layouts<Word, default_upper>(lb_bits * 3 + 17, 0.5, seed_base + 12);

        run_select_poppy_all_layouts<Word, default_upper>(lb_bits * 4, 0.0, seed_base + 13);
        run_select_poppy_all_layouts<Word, default_upper>(lb_bits * 4, 1.0, seed_base + 14);

        run_select_poppy_all_layouts<Word, default_upper>(200000, 0.02, seed_base + 15);
        run_select_poppy_all_layouts<Word, default_upper>(200000, 0.5, seed_base + 16);
        run_select_poppy_all_layouts<Word, default_upper>(200000, 0.98, seed_base + 17);

        constexpr std::uint64_t small_upper = 4096;

        run_select_poppy_all_layouts<Word, small_upper>(small_upper - 1, 0.5, seed_base + 18);
        run_select_poppy_all_layouts<Word, small_upper>(small_upper, 0.5, seed_base + 19);
        run_select_poppy_all_layouts<Word, small_upper>(small_upper + 1, 0.5, seed_base + 20);

        run_select_poppy_all_layouts<Word, small_upper>(small_upper * 3 + 123, 0.5, seed_base + 21);
        run_select_poppy_all_layouts<Word, small_upper>(small_upper * 4, 0.02, seed_base + 22);
        run_select_poppy_all_layouts<Word, small_upper>(small_upper * 4, 0.98, seed_base + 23);
    }

} // namespace

TEST_CASE("select/select_poppy") {
    SUBCASE("Word = uint64_t") {
        run_select_poppy_word_suite<std::uint64_t>(21000);
    }
    SUBCASE("Word = uint32_t") {
        run_select_poppy_word_suite<std::uint32_t>(22000);
    }
    SUBCASE("Word = uint16_t") {
        run_select_poppy_word_suite<std::uint16_t>(23000);
    }
    SUBCASE("Word = uint8_t") {
        run_select_poppy_word_suite<std::uint8_t>(24000);
    }
}
