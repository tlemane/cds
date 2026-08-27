#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

#include <doctest.h>

#include <cds/core/common.hpp>
#include <cds/core/packed/type.hpp>

namespace {

    template <typename Type> [[nodiscard]] Type oracle_lsb_mask(std::size_t m) noexcept {
        Type v = 0;
        for (std::size_t b = 0; b < m; ++b)
            v = static_cast<Type>(v | (Type{1} << b));
        return v;
    }

    template <typename Type>
    [[nodiscard]] Type oracle_msb_mask(std::size_t m, std::size_t d) noexcept {
        Type v = 0;
        for (std::size_t b = d - m; b < d; ++b)
            v = static_cast<Type>(v | (Type{1} << b));
        return v;
    }

    template <typename Type> void run_mask_suite() {
        constexpr std::size_t d = std::numeric_limits<Type>::digits;

        for (std::size_t m = 0; m < d; ++m) {
            CAPTURE(m);
            CHECK(cds::lsb_mask<Type>(m) == oracle_lsb_mask<Type>(m));
        }

        for (std::size_t m = 1; m < d; ++m) {
            CAPTURE(m);
            CHECK(cds::msb_mask<Type>(m) == oracle_msb_mask<Type>(m, d));
        }
    }

}

TEST_CASE("core/common — bit masks") {
    SUBCASE("Type = uint64_t") {
        run_mask_suite<std::uint64_t>();
    }
    SUBCASE("Type = uint32_t") {
        run_mask_suite<std::uint32_t>();
    }
    SUBCASE("Type = uint16_t") {
        run_mask_suite<std::uint16_t>();
    }
    SUBCASE("Type = uint8_t") {
        run_mask_suite<std::uint8_t>();
    }
}

TEST_CASE("core/packed/type — to_nb_words / to_capacity") {
    using cds::pack_mode;
    using cds::to_capacity;
    using cds::to_nb_words;
    using w64 = std::uint64_t;

    SUBCASE("to_nb_words") {
        CHECK(to_nb_words<w64>(50, 4, pack_mode::none) == 50);

        CHECK(to_nb_words<w64>(100, 4, pack_mode::dense) == 7);
        CHECK(to_nb_words<w64>(64, 1, pack_mode::dense) == 1);
        CHECK(to_nb_words<w64>(65, 1, pack_mode::dense) == 2);

        CHECK(to_nb_words<w64>(100, 4, pack_mode::sparse) == 7);
        CHECK(to_nb_words<w64>(64, 1, pack_mode::sparse) == 1);
        CHECK(to_nb_words<w64>(65, 1, pack_mode::sparse) == 2);
    }

    SUBCASE("to_capacity") {

        CHECK(to_capacity<w64>(5, 4, pack_mode::none) == 5);

        CHECK(to_capacity<w64>(1, 4, pack_mode::sparse) == 16);
        CHECK(to_capacity<w64>(2, 4, pack_mode::sparse) == 32);
        CHECK(to_capacity<w64>(1, 3, pack_mode::sparse) == 21);

        CHECK(to_capacity<w64>(1, 4, pack_mode::dense) == 16);
        CHECK(to_capacity<w64>(2, 4, pack_mode::dense) == 32);
        CHECK(to_capacity<w64>(1, 3, pack_mode::dense) == 21);
    }
}
