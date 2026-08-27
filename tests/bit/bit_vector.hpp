#pragma once

#include <cstdint>
#include <limits>
#include <utility>

#include <doctest.h>

#include <cds/io/buffer.hpp>
#include <cds/bit/vector.hpp>

#include "bit_check.hpp"

namespace {

    template <typename Word, cds::pack_endian Endian>
    void run_bit_vector_case(std::size_t n, double density, std::uint64_t seed) {
        using bv_t = cds::bit_vector<Word, Endian>;
        static_assert(cds::bit_source<bv_t>, "bit_vector must satisfy bit_source");

        const auto bits = make_bits(n, density, seed);

        bv_t v;
        CHECK(v.empty());
        for (std::uint8_t b : bits)
            v.push_back(b);

        REQUIRE(v.size() == n);
        CHECK(v.empty() == (n == 0));
        check_bits_equal(v, bits);
        CHECK(v.popcount() == popcount_of(bits));

        if (n > 0) {
            CHECK(static_cast<std::uint8_t>(v.front()) == bits.front());
            CHECK(static_cast<std::uint8_t>(v.back()) == bits.back());
        }

        std::size_t it_i = 0;
        for (auto bit : v) {
            CAPTURE(it_i);
            CHECK(static_cast<std::uint8_t>(bit) == bits[it_i]);
            ++it_i;
        }
        CHECK(it_i == n);

        {
            bv_t c = v;
            REQUIRE(c.size() == n);
            check_bits_equal(c, bits);
            if (n > 0) {
                c.flip(0);
                CHECK(static_cast<std::uint8_t>(c[0]) != static_cast<std::uint8_t>(v[0]));
            }
        }

        {
            bv_t src = v;
            bv_t dst = std::move(src);
            REQUIRE(dst.size() == n);
            check_bits_equal(dst, bits);
            CHECK(src.size() == 0);
        }

        if (n > 0) {
            bv_t p = v;
            p.pop_back();
            CHECK(p.size() == n - 1);
            if (n >= 2)
                CHECK(static_cast<std::uint8_t>(p.back()) == bits[n - 2]);
        }

        {
            bv_t r = v;
            r.resize(n + 5, std::uint8_t{0});
            REQUIRE(r.size() == n + 5);
            for (std::size_t i = n; i < n + 5; ++i) {
                CAPTURE(i);
                CHECK(static_cast<std::uint8_t>(r[i]) == 0);
            }
            r.resize(n);
            CHECK(r.size() == n);
            check_bits_equal(r, bits);
        }

        {
            bv_t c = v;
            c.clear();
            CHECK(c.empty());
            CHECK(c.size() == 0);
        }

        {
            cds::io::buffer_sink sink;
            REQUIRE(v.save(sink));

            auto bytes = sink.release();
            cds::io::buffer_source source(bytes);

            auto loaded = bv_t::load(source);
            REQUIRE(loaded.has_value());
            check_bits_equal(*loaded, bits);
        }
    }

    template <typename Word>
    void run_bit_vector_all_layouts(std::size_t n, double density, std::uint64_t seed) {
        using cds::pack_endian;
        using cds::pack_mode;

        run_bit_vector_case<Word, pack_endian::lsb>(n, density, seed + 1);
        run_bit_vector_case<Word, pack_endian::msb>(n, density, seed + 3);
    }

    template <typename Word> void run_bit_vector_word_suite(std::uint64_t seed_base) {
        constexpr std::size_t digits = std::numeric_limits<Word>::digits;

        run_bit_vector_all_layouts<Word>(0, 0.5, seed_base + 1);
        run_bit_vector_all_layouts<Word>(1, 0.5, seed_base + 2);

        run_bit_vector_all_layouts<Word>(digits - 1, 0.5, seed_base + 3);
        run_bit_vector_all_layouts<Word>(digits, 0.5, seed_base + 4);
        run_bit_vector_all_layouts<Word>(digits + 1, 0.5, seed_base + 5);

        run_bit_vector_all_layouts<Word>(digits * 3 + 7, 0.5, seed_base + 6);

        run_bit_vector_all_layouts<Word>(300, 0.0, seed_base + 7);
        run_bit_vector_all_layouts<Word>(300, 1.0, seed_base + 8);
        run_bit_vector_all_layouts<Word>(300, 0.02, seed_base + 9);
        run_bit_vector_all_layouts<Word>(300, 0.5, seed_base + 10);
        run_bit_vector_all_layouts<Word>(300, 0.98, seed_base + 11);
    }

} // namespace

TEST_CASE("bit/bit_vector") {
    SUBCASE("Word = uint64_t") {
        run_bit_vector_word_suite<std::uint64_t>(30000);
    }
    SUBCASE("Word = uint32_t") {
        run_bit_vector_word_suite<std::uint32_t>(31000);
    }
    SUBCASE("Word = uint16_t") {
        run_bit_vector_word_suite<std::uint16_t>(32000);
    }
    SUBCASE("Word = uint8_t") {
        run_bit_vector_word_suite<std::uint8_t>(33000);
    }
}
