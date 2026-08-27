#pragma once

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

#include <doctest.h>

#include <cds/io/buffer.hpp>
#include <cds/bit/vector.hpp>
#include <cds/select/darray.hpp>

namespace {

    template <typename Word, cds::pack_endian Endian>
    cds::bit_vector_impl<Word, Endian> build_scattered(std::size_t n, std::uint64_t universe,
                                                       std::uint64_t seed,
                                                       std::vector<std::size_t>& one_positions) {
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<std::uint64_t> dist(0, universe > 0 ? universe - 1 : 0);
        std::vector<std::uint64_t> values(n);
        for (auto& v : values)
            v = dist(rng);
        std::sort(values.begin(), values.end());

        cds::bit_vector_impl<Word, Endian> v;
        one_positions.clear();
        one_positions.reserve(n);

        std::uint64_t last = 0;
        for (std::size_t i = 0; i < n; ++i) {
            while (last < values[i]) {
                v.push_back(std::uint8_t{0});
                ++last;
            }
            v.push_back(std::uint8_t{1});
            one_positions.push_back(static_cast<std::size_t>(last));
            ++last;
        }

        return v;
    }

    template <typename Word, cds::pack_endian Endian>
    void run_darray_case(std::size_t n, std::uint64_t universe, std::uint64_t seed) {
        using source_type = cds::bit_vector_impl<Word, Endian>;
        using cds::select_target;

        std::vector<std::size_t> one_positions;
        auto v = build_scattered<Word, Endian>(n, universe, seed, one_positions);

        std::vector<std::size_t> zero_positions;
        zero_positions.reserve(v.size() - one_positions.size());
        {
            std::size_t next_one = 0;
            for (std::size_t p = 0; p < v.size(); ++p) {
                if (next_one < one_positions.size() && one_positions[next_one] == p)
                    ++next_one;
                else
                    zero_positions.push_back(p);
            }
        }

        REQUIRE(one_positions.size() == n);

        cds::darray<source_type, select_target::both> d(v);
        CHECK(d.size() == v.size());
        CHECK(d.data() == cds::bit_source_traits<source_type>::data(v));

        check_select_sampled([&](std::size_t r) { return d.select1(r); }, one_positions);
        check_select_sampled([&](std::size_t r) { return d.select0(r); }, zero_positions);

        cds::darray_view<source_type, select_target::both> dv(d.ones(), d.zeros(), v);
        check_select_sampled([&](std::size_t r) { return dv.select1(r); }, one_positions);
        check_select_sampled([&](std::size_t r) { return dv.select0(r); }, zero_positions);

        cds::io::buffer_sink sink;
        REQUIRE(d.save(sink));

        auto bytes = sink.release();
        cds::io::buffer_source source(bytes);

        auto d_loaded_result = cds::darray<source_type, select_target::both>::load(source, v);
        REQUIRE(d_loaded_result.has_value());
        auto& d_loaded = *d_loaded_result;

        check_select_sampled([&](std::size_t r) { return d_loaded.select1(r); }, one_positions);
        check_select_sampled([&](std::size_t r) { return d_loaded.select0(r); }, zero_positions);

        if (!one_positions.empty()) {
            cds::darray<source_type, select_target::ones> d_ones(v);
            check_select_sampled([&](std::size_t r) { return d_ones.select1(r); }, one_positions);
        }
        if (!zero_positions.empty()) {
            cds::darray<source_type, select_target::zeros> d_zeros(v);
            check_select_sampled([&](std::size_t r) { return d_zeros.select0(r); }, zero_positions);
        }
    }

    template <typename Word>
    void run_darray_all_layouts(std::size_t n, std::uint64_t universe, std::uint64_t seed) {
        using cds::pack_endian;
        using cds::pack_mode;

        run_darray_case<Word, pack_endian::lsb>(n, universe, seed + 1);
        run_darray_case<Word, pack_endian::msb>(n, universe, seed + 3);
    }

    template <typename Word> void run_darray_word_suite(std::uint64_t seed_base) {

        run_darray_all_layouts<Word>(1, 2, seed_base + 1);
        run_darray_all_layouts<Word>(100, 150, seed_base + 2);

        run_darray_all_layouts<Word>(5000, 10000, seed_base + 3);
        run_darray_all_layouts<Word>(5000 * 3 + 17, 10000 * 3, seed_base + 4);

        run_darray_all_layouts<Word>(5000, 5000ull * 1000, seed_base + 5);
        run_darray_all_layouts<Word>(5000 * 3 + 17, 5000ull * 1000 * 3, seed_base + 6);

        run_darray_all_layouts<Word>(3000, 3000ull * 20, seed_base + 7);
    }

} // namespace

TEST_CASE("select/darray") {
    SUBCASE("Word = uint64_t") {
        run_darray_word_suite<std::uint64_t>(17000);
    }
    SUBCASE("Word = uint32_t") {
        run_darray_word_suite<std::uint32_t>(18000);
    }
    SUBCASE("Word = uint16_t") {
        run_darray_word_suite<std::uint16_t>(19000);
    }
    SUBCASE("Word = uint8_t") {
        run_darray_word_suite<std::uint8_t>(20000);
    }
}
