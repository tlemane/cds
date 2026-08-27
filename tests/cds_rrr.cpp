#include <doctest.h>

#include <cds/rrr.hpp>
#include <cds/bit/vector.hpp>

#include <bit>
#include <cstdint>
#include <random>
#include <vector>

namespace {

    bool naive_bit(std::uint64_t pattern, std::size_t pos) {
        return (pattern >> pos) & std::uint64_t{1};
    }

    std::size_t naive_prefix_rank(std::uint64_t pattern, std::size_t off) {
        const std::uint64_t mask =
            (off >= 64) ? ~std::uint64_t{0} : ((std::uint64_t{1} << off) - 1);
        return static_cast<std::size_t>(std::popcount(pattern & mask));
    }

    std::size_t naive_select(std::uint64_t pattern, std::size_t need) {
        std::size_t seen = 0;
        for (std::size_t p = 0;; ++p)
            if ((pattern >> p) & std::uint64_t{1}) {
                if (seen == need)
                    return p;
                ++seen;
            }
    }

}

TEST_CASE("rrr combinadic decode helpers round-trip vs naive") {
    constexpr std::size_t B = 63;
    std::mt19937_64 rng(1);
    for (int iter = 0; iter < 20000; ++iter) {
        const std::size_t len = 1 + rng() % B;
        const std::uint64_t len_mask =
            (len >= 64) ? ~std::uint64_t{0} : ((std::uint64_t{1} << len) - 1);
        const std::uint64_t pattern = rng() & len_mask;
        const std::size_t k = static_cast<std::size_t>(std::popcount(pattern));
        const std::uint64_t nr = cds::detail::rrr_rank_of_block<B>(pattern, len);

        CHECK(cds::detail::rrr_unrank_to_block<B>(nr, len, k) == pattern);

        for (std::size_t pos = 0; pos < len; ++pos)
            CHECK(cds::detail::rrr_decode_bit<B>(nr, len, k, pos) == naive_bit(pattern, pos));
        for (std::size_t off = 0; off <= len; ++off)
            CHECK(cds::detail::rrr_decode_prefix_rank<B>(nr, len, k, off) ==
                  naive_prefix_rank(pattern, off));
        for (std::size_t need = 0; need < k; ++need)
            CHECK(cds::detail::rrr_decode_select<B>(nr, len, k, need) ==
                  naive_select(pattern, need));
    }
}

namespace {
    using src_bv = cds::bit_vector<std::uint64_t, cds::pack_endian::lsb>;

    template <typename RRR> void check_access_rank(const RRR& r, const std::vector<char>& bits) {
        const std::size_t n = bits.size();
        std::size_t ones = 0;
        for (std::size_t i = 0; i < n; ++i) {
            CHECK(r[i] == (bits[i] != 0));
            CHECK(r.rank1(i) == ones);
            CHECK(r.rank0(i) == i - ones);
            ones += (bits[i] != 0);
        }
        CHECK(r.rank1(n) == ones);
    }
} // namespace

TEST_CASE("rrr operator[]/rank vs oracle") {
    for (double density : {0.01, 0.5, 0.99}) {
        for (std::size_t n : {std::size_t{1}, std::size_t{63}, std::size_t{64}, std::size_t{200},
                              std::size_t{5000}}) {
            std::mt19937_64 rng(n * 100 + static_cast<std::size_t>(density * 1000));
            std::bernoulli_distribution d(density);
            std::vector<char> bits(n);
            src_bv bv;
            bv.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                bits[i] = d(rng) ? 1 : 0;
                bv.push_back(static_cast<std::uint8_t>(bits[i]));
            }
            cds::rrr<> r(bv);
            CHECK(r.size() == n);
            check_access_rank(r, bits);
        }
    }
}

TEST_CASE("rrr select vs oracle") {
    for (double density : {0.05, 0.5, 0.95}) {
        for (std::size_t n : {std::size_t{63}, std::size_t{200}, std::size_t{5000}}) {
            std::mt19937_64 rng(n + static_cast<std::size_t>(density * 777));
            std::bernoulli_distribution d(density);
            std::vector<std::size_t> ones_pos, zeros_pos;
            src_bv bv;
            bv.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                const bool b = d(rng);
                bv.push_back(static_cast<std::uint8_t>(b));
                (b ? ones_pos : zeros_pos).push_back(i);
            }
            if (ones_pos.empty() || zeros_pos.empty())
                continue;
            cds::rrr<> r(bv);
            for (std::size_t k = 0; k < ones_pos.size(); ++k)
                CHECK(r.select1(k) == ones_pos[k]);
            for (std::size_t k = 0; k < zeros_pos.size(); ++k)
                CHECK(r.select0(k) == zeros_pos[k]);
        }
    }
}

#include <cds/rank/concepts.hpp>
#include <cds/select/concepts.hpp>
static_assert(cds::rank1_structure<cds::rrr<>>);
static_assert(cds::rank0_structure<cds::rrr<>>);
static_assert(cds::select1_structure<cds::rrr<>>);
static_assert(cds::select0_structure<cds::rrr<>>);

TEST_CASE("rrr memory_size populated and beats raw for compressible input") {
    std::mt19937_64 rng(9);
    std::bernoulli_distribution d(0.02);
    const std::size_t n = 100000;
    src_bv bv;
    bv.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        bv.push_back(static_cast<std::uint8_t>(d(rng) ? 1 : 0));
    cds::rrr<> r(bv);
    CHECK(r.memory_size() > sizeof(cds::rrr<>));
    CHECK(r.memory_size() < n / 8);
}

#include <cds/io/buffer.hpp>

TEST_CASE("rrr save/load round-trip") {
    std::mt19937_64 rng(7);
    std::bernoulli_distribution d(0.3);
    const std::size_t n = 5000;
    std::vector<char> bits(n);
    src_bv bv;
    bv.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        bits[i] = d(rng) ? 1 : 0;
        bv.push_back(static_cast<std::uint8_t>(bits[i]));
    }
    cds::rrr<> r(bv);

    cds::io::buffer_sink sink;
    REQUIRE(r.save(sink));
    auto bytes = sink.release();
    cds::io::buffer_source src(bytes);
    auto loaded = cds::rrr<>::load(src);
    REQUIRE(loaded.has_value());
    CHECK(loaded->size() == n);
    for (std::size_t i = 0; i < n; ++i) {
        CHECK((*loaded)[i] == (bits[i] != 0));
        CHECK(loaded->rank1(i) == r.rank1(i));
    }
}

TEST_CASE("rrr_view matches rrr over the same serialized bytes") {
    std::mt19937_64 rng(11);
    std::bernoulli_distribution d(0.4);
    const std::size_t n = 5000;
    src_bv bv;
    bv.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        bv.push_back(static_cast<std::uint8_t>(d(rng) ? 1 : 0));
    cds::rrr<> r(bv);

    cds::io::buffer_sink sink;
    REQUIRE(r.save(sink));
    auto bytes = sink.release();
    cds::io::buffer_source vsrc(bytes);
    auto ev = cds::rrr_view<>::load(vsrc);
    REQUIRE(ev.has_value());
    REQUIRE(ev->size() == n);

    std::size_t ones = 0;
    for (std::size_t i = 0; i < n; ++i) {
        CHECK((*ev)[i] == r[i]);
        CHECK(ev->rank1(i) == r.rank1(i));
        CHECK(ev->rank0(i) == r.rank0(i));
        ones += r[i];
    }
    for (std::size_t k = 0; k < ones; ++k)
        CHECK(ev->select1(k) == r.select1(k));
    for (std::size_t k = 0; k < n - ones; ++k)
        CHECK(ev->select0(k) == r.select0(k));
}
