#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <doctest.h>

#include <cds/packed/array.hpp>
#include <cds/packed/vector.hpp>
#include <cds/packed/view.hpp>

namespace {

    using word_t = std::uint64_t;
    using value_t = std::uint64_t;

    [[nodiscard]] inline value_t packed_pattern(std::size_t i, std::uint8_t width) noexcept {
        const value_t mask = (width >= 64) ? ~value_t{0} : ((value_t{1} << width) - 1);
        value_t x = static_cast<value_t>(i) + 1;
        x *= 0x9E3779B97F4A7C15ull;
        x ^= x >> 29;
        return x & mask;
    }

    template <typename Word, std::uint8_t Width, cds::pack_endian Endian, cds::pack_mode Mode>
    void check_packed_vector_roundtrip(std::size_t n) {
        using vec_t = cds::packed_vector<Word, value_t, Width, Endian, Mode>;

        vec_t v;
        for (std::size_t i = 0; i < n; ++i)
            v.push_back(packed_pattern(i, Width));

        REQUIRE(v.size() == n);

        for (std::size_t i = 0; i < n; ++i) {
            CAPTURE(i);
            CHECK(v[i] == packed_pattern(i, Width));
        }

        std::size_t it_i = 0;
        for (auto val : v) {
            CAPTURE(it_i);
            CHECK(val == packed_pattern(it_i, Width));
            ++it_i;
        }
        CHECK(it_i == n);

        for (std::size_t i = 0; i < n; ++i)
            v[i] = packed_pattern(i + 1, Width);
        for (std::size_t i = 0; i < n; ++i) {
            CAPTURE(i);
            CHECK(v[i] == packed_pattern(i + 1, Width));
        }
    }

    template <typename Word, std::size_t Capacity, std::uint8_t Width, cds::pack_endian Endian,
              cds::pack_mode Mode>
    void check_packed_array_roundtrip() {
        using arr_t = cds::packed_array<Word, value_t, Capacity, Width, Endian, Mode>;

        arr_t a;
        const std::size_t n = a.capacity();
        for (std::size_t i = 0; i < n; ++i)
            a.push_back(packed_pattern(i, Width));

        REQUIRE(a.size() == n);

        for (std::size_t i = 0; i < n; ++i) {
            CAPTURE(i);
            CHECK(a[i] == packed_pattern(i, Width));
        }

        if (n > 0) {
            const std::size_t m = n / 2;
            a[m] = packed_pattern(m + 1, Width);
            CHECK(a[m] == packed_pattern(m + 1, Width));
        }
    }

    template <typename Word, std::uint8_t Width, cds::pack_endian Endian, cds::pack_mode Mode>
    void check_packed_view_roundtrip(std::size_t n) {
        using view_t = cds::packed_view<Word, value_t, Width, Endian, Mode>;
        using cview_t = cds::const_packed_view<Word, value_t, Width, Endian, Mode>;

        std::vector<Word> buf(n + 2, Word{0});

        view_t v(buf.data(), n);
        for (std::size_t i = 0; i < n; ++i)
            v[i] = packed_pattern(i, Width);

        cview_t cv(buf.data(), n);
        for (std::size_t i = 0; i < n; ++i) {
            CAPTURE(i);
            CHECK(cv[i] == packed_pattern(i, Width));
        }

        const std::size_t step = std::max<std::size_t>(1, n / 8);
        for (std::size_t off = 0; off <= n; off += step) {
            auto sub = v.subview(off);
            REQUIRE(sub.size() == n - off);
            for (std::size_t i = 0; i < sub.size(); ++i) {
                CAPTURE(off);
                CAPTURE(i);
                CHECK(sub[i] == packed_pattern(off + i, Width));
            }
        }
    }

    template <typename Word, std::uint8_t Width> void vector_sweep_width(std::size_t n) {
        if constexpr (Width <= std::numeric_limits<Word>::digits) {
            using cds::pack_endian;
            using cds::pack_mode;
            check_packed_vector_roundtrip<Word, Width, pack_endian::lsb, pack_mode::sparse>(n);
            check_packed_vector_roundtrip<Word, Width, pack_endian::lsb, pack_mode::dense>(n);
            check_packed_vector_roundtrip<Word, Width, pack_endian::msb, pack_mode::sparse>(n);
            check_packed_vector_roundtrip<Word, Width, pack_endian::msb, pack_mode::dense>(n);
        }
    }

    template <typename Word, std::uint8_t Width> void array_sweep_width() {
        if constexpr (Width <= std::numeric_limits<Word>::digits) {
            using cds::pack_endian;
            using cds::pack_mode;
            check_packed_array_roundtrip<Word, 97, Width, pack_endian::lsb, pack_mode::sparse>();
            check_packed_array_roundtrip<Word, 97, Width, pack_endian::lsb, pack_mode::dense>();
            check_packed_array_roundtrip<Word, 97, Width, pack_endian::msb, pack_mode::sparse>();
            check_packed_array_roundtrip<Word, 97, Width, pack_endian::msb, pack_mode::dense>();
        }
    }

    template <typename Word, std::uint8_t Width> void view_sweep_width(std::size_t n) {
        if constexpr (Width <= std::numeric_limits<Word>::digits) {
            using cds::pack_endian;
            using cds::pack_mode;
            check_packed_view_roundtrip<Word, Width, pack_endian::lsb, pack_mode::sparse>(n);
            check_packed_view_roundtrip<Word, Width, pack_endian::lsb, pack_mode::dense>(n);
            check_packed_view_roundtrip<Word, Width, pack_endian::msb, pack_mode::sparse>(n);
            check_packed_view_roundtrip<Word, Width, pack_endian::msb, pack_mode::dense>(n);
        }
    }

    template <typename Word> void packed_vector_sweep(std::size_t n = 130) {
        vector_sweep_width<Word, 1>(n);
        vector_sweep_width<Word, 2>(n);
        vector_sweep_width<Word, 3>(n);
        vector_sweep_width<Word, 5>(n);
        vector_sweep_width<Word, 8>(n);
        vector_sweep_width<Word, 13>(n);
        vector_sweep_width<Word, 31>(n);
        vector_sweep_width<Word, 64>(n);
    }

    template <typename Word> void packed_array_sweep() {
        array_sweep_width<Word, 1>();
        array_sweep_width<Word, 2>();
        array_sweep_width<Word, 3>();
        array_sweep_width<Word, 5>();
        array_sweep_width<Word, 8>();
        array_sweep_width<Word, 13>();
        array_sweep_width<Word, 31>();
        array_sweep_width<Word, 64>();
    }

    template <typename Word> void packed_view_sweep(std::size_t n = 97) {
        view_sweep_width<Word, 1>(n);
        view_sweep_width<Word, 2>(n);
        view_sweep_width<Word, 3>(n);
        view_sweep_width<Word, 5>(n);
        view_sweep_width<Word, 8>(n);
        view_sweep_width<Word, 13>(n);
        view_sweep_width<Word, 31>(n);
        view_sweep_width<Word, 64>(n);
    }

}
