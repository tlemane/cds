#pragma once

#include <cstdint>
#include <tuple>

#include <doctest.h>

#include <cds/core/packed/type.hpp>
#include <cds/io/format.hpp>

TEST_CASE("io/format — resolve_packing_params") {
    using cds::pack_endian;
    using cds::pack_mode;
    using cds::detail::packed_save_header;
    using cds::detail::resolve_packing_params;

    const packed_save_header h{cds::detail::packed_save_magic,
                               std::uint8_t{4},
                               static_cast<std::uint8_t>(pack_endian::lsb),
                               static_cast<std::uint8_t>(pack_mode::sparse),
                               0,
                               100};

    SUBCASE("all fixed and matching") {
        auto r = resolve_packing_params<4, pack_endian::lsb, pack_mode::sparse>(h);
        REQUIRE(r.has_value());
        const auto [w, e, m] = *r;
        CHECK(w == 4);
        CHECK(e == pack_endian::lsb);
        CHECK(m == pack_mode::sparse);
    }

    SUBCASE("fixed mismatches return nullopt") {
        CHECK_FALSE(resolve_packing_params<8, pack_endian::lsb, pack_mode::sparse>(h).has_value());
        CHECK_FALSE(resolve_packing_params<4, pack_endian::msb, pack_mode::sparse>(h).has_value());
        CHECK_FALSE(resolve_packing_params<4, pack_endian::lsb, pack_mode::dense>(h).has_value());
    }

    SUBCASE("runtime axes adopt the saved value") {

        {
            auto r = resolve_packing_params<0, pack_endian::lsb, pack_mode::sparse>(h);
            REQUIRE(r.has_value());
            CHECK(std::get<0>(*r) == 4);
        }

        {
            auto r = resolve_packing_params<4, pack_endian::rt, pack_mode::sparse>(h);
            REQUIRE(r.has_value());
            CHECK(std::get<1>(*r) == pack_endian::lsb);
        }

        {
            auto r = resolve_packing_params<4, pack_endian::lsb, pack_mode::rt>(h);
            REQUIRE(r.has_value());
            CHECK(std::get<2>(*r) == pack_mode::sparse);
        }

        {
            auto r = resolve_packing_params<0, pack_endian::rt, pack_mode::rt>(h);
            REQUIRE(r.has_value());
            CHECK(std::get<0>(*r) == 4);
            CHECK(std::get<1>(*r) == pack_endian::lsb);
            CHECK(std::get<2>(*r) == pack_mode::sparse);
        }
    }
}
