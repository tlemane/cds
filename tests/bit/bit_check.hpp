#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include <doctest.h>

namespace {

    [[nodiscard]] inline std::vector<std::uint8_t> make_bits(std::size_t n, double density,
                                                             std::uint64_t seed) {
        std::mt19937_64 rng(seed);
        std::bernoulli_distribution dist(density);
        std::vector<std::uint8_t> bits(n);
        for (std::size_t i = 0; i < n; ++i)
            bits[i] = dist(rng) ? std::uint8_t{1} : std::uint8_t{0};
        return bits;
    }

    [[nodiscard]] inline std::size_t popcount_of(const std::vector<std::uint8_t>& bits) noexcept {
        std::size_t c = 0;
        for (std::uint8_t b : bits)
            c += (b != 0) ? std::size_t{1} : std::size_t{0};
        return c;
    }

    template <typename Bit>
    void check_bits_equal(const Bit& b, const std::vector<std::uint8_t>& bits) {
        REQUIRE(b.size() == bits.size());
        for (std::size_t i = 0; i < bits.size(); ++i) {
            CAPTURE(i);
            CHECK(static_cast<std::uint8_t>(b[i]) == bits[i]);
        }
    }

}
