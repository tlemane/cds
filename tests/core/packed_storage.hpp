#include <doctest.h>
#include <cds/core/common.hpp>
#include <cds/core/packed/storage.hpp>

using namespace cds;

int test_f_0() {
    return 0;
}

int test_f_1(std::uint8_t w) {
    unused(w);
    return 1;
}

int test_f_2(pack_endian e) {
    return e == pack_endian::msb ? 2 : 3;
}

int test_f_3(std::uint8_t w, pack_endian e) {
    return static_cast<int>(w) + (e == pack_endian::msb ? 20 : 30);
}

int test_f_4(pack_mode m) {
    return m == pack_mode::none ? 4 : m == pack_mode::sparse ? 5 : 6;
}

int test_f_5(std::uint8_t w, pack_mode m) {
    return static_cast<int>(w) + (m == pack_mode::none ? 50 : m == pack_mode::sparse ? 60 : 70);
}

int test_f_6(pack_endian e, pack_mode m) {
    return (e == pack_endian::msb ? 10 : 20) + (m == pack_mode::none     ? 100
                                                : m == pack_mode::sparse ? 200
                                                                         : 300);
}

int test_f_7(std::uint8_t w, pack_endian e, pack_mode m) {
    return static_cast<int>(w) + (e == pack_endian::msb ? 10 : 20) +
           (m == pack_mode::none     ? 100
            : m == pack_mode::sparse ? 200
                                     : 300);
}

TEST_CASE("core/packed/storage ") {

    SUBCASE("packing_storage") {

        SUBCASE("size") {
#if defined(_MSC_VER)
            // MSVC disables [[no_unique_address]] (see cds/core/attributes.hpp),
            CHECK(sizeof(packing_storage<1, pack_endian::lsb, pack_mode::dense>) <= 3);
            CHECK(sizeof(packing_storage<0, pack_endian::lsb, pack_mode::dense>) <= 3);
            CHECK(sizeof(packing_storage<0, pack_endian::rt, pack_mode::dense>) <= 3);
#else
            CHECK(sizeof(packing_storage<1, pack_endian::lsb, pack_mode::dense>) == 1);
            CHECK(sizeof(packing_storage<0, pack_endian::lsb, pack_mode::dense>) == 1);
            CHECK(sizeof(packing_storage<0, pack_endian::rt, pack_mode::dense>) == 2);
#endif
            CHECK(sizeof(packing_storage<0, pack_endian::rt, pack_mode::rt>) == 3);
        }

        SUBCASE("width known, endian known, mode known") {
            packing_storage<2, pack_endian::lsb, pack_mode::dense> s;

            CHECK(s.visit([] { return test_f_0(); }) == 0);
        }

        SUBCASE("width runtime, endian known, mode known") {
            packing_storage<0, pack_endian::lsb, pack_mode::dense> s(5, pack_endian::lsb,
                                                                     pack_mode::dense);

            CHECK(s.visit([](std::uint8_t w) { return test_f_1(w); }) == 1);
        }

        SUBCASE("width known, endian runtime, mode known") {
            packing_storage<2, pack_endian::rt, pack_mode::dense> s(2, pack_endian::msb,
                                                                    pack_mode::dense);

            CHECK(s.visit([](pack_endian e) { return test_f_2(e); }) == 2);
        }

        SUBCASE("width runtime, endian runtime, mode known") {
            packing_storage<0, pack_endian::rt, pack_mode::dense> s(5, pack_endian::msb,
                                                                    pack_mode::dense);

            CHECK(s.visit([](std::uint8_t w, pack_endian e) { return test_f_3(w, e); }) == 25);
        }

        SUBCASE("width known, endian known, mode runtime") {
            packing_storage<2, pack_endian::lsb, pack_mode::rt> s(2, pack_endian::lsb,
                                                                  pack_mode::sparse);

            CHECK(s.visit([](pack_mode m) { return test_f_4(m); }) == 5);
        }

        SUBCASE("width runtime, endian known, mode runtime") {
            packing_storage<0, pack_endian::lsb, pack_mode::rt> s(5, pack_endian::lsb,
                                                                  pack_mode::sparse);

            CHECK(s.visit([](std::uint8_t w, pack_mode m) { return test_f_5(w, m); }) == 65);
        }

        SUBCASE("width known, endian runtime, mode runtime") {
            packing_storage<2, pack_endian::rt, pack_mode::rt> s(2, pack_endian::msb,
                                                                 pack_mode::sparse);

            CHECK(s.visit([](pack_endian e, pack_mode m) { return test_f_6(e, m); }) == 210);
        }

        SUBCASE("width runtime, endian runtime, mode runtime") {
            packing_storage<0, pack_endian::rt, pack_mode::rt> s(5, pack_endian::msb,
                                                                 pack_mode::sparse);

            CHECK(s.visit([](std::uint8_t w, pack_endian e, pack_mode m) {
                return test_f_7(w, e, m);
            }) == 215);
        }
    }
}
