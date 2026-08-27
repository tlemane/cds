#pragma once

#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include <doctest.h>

#include <cds/io/buffer.hpp>
#include <cds/bit/vector.hpp>
#include <cds/rank/rank9.hpp>
#include <cds/select/select9.hpp>

namespace {

    template <typename Word, cds::pack_endian Endian>
    void run_select9_case(std::size_t n, double density, std::uint64_t seed) {
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

        cds::rank9<source_type> r9(v);

        cds::select9<source_type, select_target::both> s9(r9);

        check_select_sampled([&](std::size_t r) { return s9.select1(r); }, one_positions);
        check_select_sampled([&](std::size_t r) { return s9.select0(r); }, zero_positions);

        cds::rank9_view<source_type> r9v(r9.superblocks(), v);
        cds::select9_view<source_type, select_target::both> s9v(s9.hints(), s9.hints0(), r9v);

        check_select_sampled([&](std::size_t r) { return s9v.select1(r); }, one_positions);
        check_select_sampled([&](std::size_t r) { return s9v.select0(r); }, zero_positions);

        cds::io::buffer_sink sink;
        REQUIRE(s9.save(sink));

        auto bytes = sink.release();
        cds::io::buffer_source source(bytes);

        auto s9_loaded_result = cds::select9<source_type, select_target::both>::load(source, r9);
        REQUIRE(s9_loaded_result.has_value());
        auto& s9_loaded = *s9_loaded_result;

        check_select_sampled([&](std::size_t r) { return s9_loaded.select1(r); }, one_positions);
        check_select_sampled([&](std::size_t r) { return s9_loaded.select0(r); }, zero_positions);

        if (!one_positions.empty()) {
            cds::select9<source_type, select_target::ones> s9_ones(r9);
            check_select_sampled([&](std::size_t r) { return s9_ones.select1(r); }, one_positions);
        }
        if (!zero_positions.empty()) {
            cds::select9<source_type, select_target::zeros> s9_zeros(r9);
            check_select_sampled([&](std::size_t r) { return s9_zeros.select0(r); },
                                 zero_positions);
        }
    }

    template <typename Word>
    void run_select9_all_layouts(std::size_t n, double density, std::uint64_t seed) {
        using cds::pack_endian;
        using cds::pack_mode;

        run_select9_case<Word, pack_endian::lsb>(n, density, seed + 1);
        run_select9_case<Word, pack_endian::msb>(n, density, seed + 3);
    }

    template <typename Word> void run_select9_word_suite(std::uint64_t seed_base) {
        constexpr std::size_t digits = std::numeric_limits<Word>::digits;
        constexpr std::size_t sb_bits = 8 * digits;

        run_select9_all_layouts<Word>(0, 0.5, seed_base + 1);
        run_select9_all_layouts<Word>(1, 0.5, seed_base + 2);

        run_select9_all_layouts<Word>(sb_bits - 1, 0.5, seed_base + 3);
        run_select9_all_layouts<Word>(sb_bits, 0.5, seed_base + 4);
        run_select9_all_layouts<Word>(sb_bits + 1, 0.5, seed_base + 5);
        run_select9_all_layouts<Word>(sb_bits * 3 + 17, 0.5, seed_base + 6);

        run_select9_all_layouts<Word>(sb_bits * 4, 0.0, seed_base + 7);
        run_select9_all_layouts<Word>(sb_bits * 4, 1.0, seed_base + 8);

        run_select9_all_layouts<Word>(200000, 0.02, seed_base + 9);
        run_select9_all_layouts<Word>(200000, 0.5, seed_base + 10);
        run_select9_all_layouts<Word>(200000, 0.98, seed_base + 11);
    }

} // namespace

TEST_CASE("select/select9") {
    SUBCASE("Word = uint64_t") {
        run_select9_word_suite<std::uint64_t>(13000);
    }
    SUBCASE("Word = uint32_t") {
        run_select9_word_suite<std::uint32_t>(14000);
    }
    SUBCASE("Word = uint16_t") {
        run_select9_word_suite<std::uint16_t>(15000);
    }
    SUBCASE("Word = uint8_t") {
        run_select9_word_suite<std::uint8_t>(16000);
    }
}
