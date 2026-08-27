#include <doctest.h>
#include <cds/core/packed/type.hpp>

using namespace cds;

TEST_CASE("core/packed/type ") {

    SUBCASE("static/runtime_value") {
        CHECK(sizeof(static_value<0>) == 1);
        CHECK(sizeof(runtime_value<std::uint8_t>) == 1);
        CHECK(sizeof(runtime_value<std::uint16_t>) == 2);
        CHECK(sizeof(runtime_value<std::uint32_t>) == 4);
        CHECK(sizeof(runtime_value<std::uint64_t>) == 8);
    }
}
