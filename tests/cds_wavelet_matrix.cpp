#include <doctest.h>

#include <cds/wavelet_matrix.hpp>
#include <cds/rrr.hpp>
#include <cds/io/buffer.hpp>

#include <cstdint>
#include <random>
#include <span>
#include <vector>

using u64 = std::uint64_t;

namespace {

    struct naive {
        std::vector<u64> v;
        [[nodiscard]] std::size_t rank(u64 c, std::size_t i) const {
            std::size_t n = 0;
            for (std::size_t j = 0; j < i; ++j)
                n += (v[j] == c) ? 1u : 0u;
            return n;
        }
        [[nodiscard]] std::size_t select(u64 c, std::size_t r) const {
            for (std::size_t j = 0; j < v.size(); ++j)
                if (v[j] == c) {
                    if (r == 0)
                        return j;
                    --r;
                }
            return v.size();
        }
    };

}

TEST_CASE("wavelet_matrix access, fixed 3-bit alphabet") {
    std::vector<u64> v = {5, 1, 7, 0, 3, 3, 6, 2, 5, 1};
    cds::wavelet_matrix<u64, 3> wm(std::span<const u64>{v});
    REQUIRE(wm.size() == v.size());
    REQUIRE(wm.bits() == 3);
    for (std::size_t i = 0; i < v.size(); ++i) {
        CAPTURE(i);
        CHECK(wm.access(i) == v[i]);
    }
}

TEST_CASE("wavelet_matrix rank vs naive (fuzz)") {
    std::mt19937_64 rng(1);
    for (int bits : {1, 3, 5, 8}) {
        const u64 sigma = u64{1} << bits;
        std::uniform_int_distribution<u64> dist(0, sigma - 1);
        std::vector<u64> v(500);
        for (auto& x : v)
            x = dist(rng);
        naive nv{v};
        cds::wavelet_matrix<u64, 0> wm(std::span<const u64>{v}, static_cast<std::size_t>(bits));
        for (int t = 0; t < 400; ++t) {
            const std::size_t i = static_cast<std::size_t>(rng()) % (v.size() + 1);
            const u64 c = dist(rng);
            CAPTURE(bits);
            CAPTURE(i);
            CAPTURE(c);
            CHECK(wm.rank(c, i) == nv.rank(c, i));
        }
    }
}

TEST_CASE("wavelet_matrix select vs naive (fuzz)") {
    std::mt19937_64 rng(2);
    for (int bits : {1, 3, 5, 8}) {
        const u64 sigma = u64{1} << bits;
        std::uniform_int_distribution<u64> dist(0, sigma - 1);
        std::vector<u64> v(500);
        for (auto& x : v)
            x = dist(rng);
        naive nv{v};
        cds::wavelet_matrix<u64, 0> wm(std::span<const u64>{v}, static_cast<std::size_t>(bits));
        for (u64 c = 0; c < sigma; ++c) {
            const std::size_t total = nv.rank(c, v.size());
            for (std::size_t r = 0; r < total; ++r) {
                CAPTURE(bits);
                CAPTURE(c);
                CAPTURE(r);
                CHECK(wm.select(c, r) == nv.select(c, r));
            }
        }
    }
}

TEST_CASE("wavelet_matrix runtime bits inferred + explicit") {
    std::vector<u64> v = {5, 1, 7, 0, 3, 3, 6, 2};
    cds::wavelet_matrix<u64, 0> inferred(std::span<const u64>{v});
    CHECK(inferred.bits() == 3);
    cds::wavelet_matrix<u64, 0> explicit_bits(std::span<const u64>{v}, 5);
    CHECK(explicit_bits.bits() == 5);
    naive nv{v};
    for (std::size_t i = 0; i < v.size(); ++i) {
        CHECK(inferred.access(i) == v[i]);
        CHECK(explicit_bits.access(i) == v[i]);
    }
    CHECK(explicit_bits.rank(3, v.size()) == nv.rank(3, v.size()));
}

TEST_CASE("wavelet_matrix edge cases") {
    cds::wavelet_matrix<u64, 4> empty(std::span<const u64>{});
    CHECK(empty.size() == 0);
    std::vector<u64> one = {9};
    cds::wavelet_matrix<u64, 4> w1(std::span<const u64>{one});
    CHECK(w1.access(0) == 9);
    CHECK(w1.select(9, 0) == 0);
    std::vector<u64> same(20, 3u);
    cds::wavelet_matrix<u64, 3> ws(std::span<const u64>{same});
    CHECK(ws.rank(3, 20) == 20);
    CHECK(ws.select(3, 19) == 19);
}

TEST_CASE("wavelet_matrix over rrr (compressed) levels, fuzz") {
    std::mt19937_64 rng(3);
    std::uniform_int_distribution<u64> dist(0, 15);
    std::vector<u64> v(400);
    for (auto& x : v)
        x = dist(rng);
    naive nv{v};
    cds::wavelet_matrix<u64, 4, cds::rrr<>> wm(std::span<const u64>{v});
    for (int t = 0; t < 300; ++t) {
        const std::size_t i = static_cast<std::size_t>(rng()) % (v.size() + 1);
        const u64 c = dist(rng);
        CHECK(wm.access(i % v.size()) == v[i % v.size()]);
        CHECK(wm.rank(c, i) == nv.rank(c, i));
    }
}

TEST_CASE("wavelet_matrix save/load (owning) round-trip") {
    std::vector<u64> v = {5, 1, 7, 0, 3, 3, 6, 2, 5, 1};
    cds::wavelet_matrix<u64, 3> wm(std::span<const u64>{v});
    cds::io::buffer_sink sink;
    REQUIRE(wm.save(sink));
    auto bytes = sink.release();
    cds::io::buffer_source src(bytes);
    auto loaded = cds::wavelet_matrix<u64, 3>::load(src);
    REQUIRE(loaded.has_value());
    for (std::size_t i = 0; i < v.size(); ++i)
        CHECK(loaded->access(i) == v[i]);
    CHECK(loaded->rank(3, v.size()) == 2);
}

TEST_CASE("wavelet_matrix as_view matches owning") {
    std::vector<u64> v = {5, 1, 7, 0, 3, 3, 6, 2, 5, 1};
    cds::wavelet_matrix<u64, 3> wm(std::span<const u64>{v});
    auto view = wm.as_view();
    for (std::size_t i = 0; i < v.size(); ++i)
        CHECK(view.access(i) == wm.access(i));
    CHECK(view.rank(5, v.size()) == wm.rank(5, v.size()));
    CHECK(view.select(5, 1) == wm.select(5, 1));
}
