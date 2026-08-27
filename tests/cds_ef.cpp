#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

#include <doctest.h>

#include <cds/ef.hpp>
#include <cds/io/buffer.hpp>

namespace {

    struct ef_oracle {
        std::vector<std::uint64_t> values;

        static constexpr std::size_t npos = static_cast<std::size_t>(-1);
        static constexpr std::uint64_t novalue = static_cast<std::uint64_t>(-1);
        struct rv {
            std::size_t pos;
            std::uint64_t val;
        };

        [[nodiscard]] std::size_t rank(std::uint64_t k) const {
            return static_cast<std::size_t>(std::lower_bound(values.begin(), values.end(), k) -
                                            values.begin());
        }

        [[nodiscard]] rv nge(std::uint64_t x) const {
            if (values.empty())
                return {npos, novalue};
            if (x > values.back())
                return {values.size() - 1, values.back()};
            const auto it = std::lower_bound(values.begin(), values.end(), x);
            const auto pos = static_cast<std::size_t>(it - values.begin());
            return {pos, values[pos]};
        }

        [[nodiscard]] rv ple(std::uint64_t x) const {
            if (values.empty())
                return {npos, novalue};
            if (x >= values.back())
                return {values.size() - 1, values.back()};
            const auto it = std::upper_bound(values.begin(), values.end(), x);
            const auto count_leq = static_cast<std::size_t>(it - values.begin());
            if (count_leq == 0)
                return {npos, novalue};
            const auto pos = count_leq - 1;
            return {pos, values[pos]};
        }

        [[nodiscard]] std::pair<rv, rv> locate(std::uint64_t x) const {
            const rv lo = ple(x);
            rv hi{npos, novalue};
            if (lo.pos != npos && lo.pos + 1 < values.size())
                hi = {lo.pos + 1, values[lo.pos + 1]};
            else if (lo.pos == npos && !values.empty())
                hi = {0, values[0]};
            return {lo, hi};
        }
    };

    std::vector<std::uint64_t> make_sorted_random(std::size_t n, std::uint64_t universe,
                                                  std::uint64_t seed) {
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<std::uint64_t> dist(0, universe > 0 ? universe - 1 : 0);
        std::vector<std::uint64_t> values(n);
        for (auto& v : values)
            v = dist(rng);
        std::sort(values.begin(), values.end());
        return values;
    }

    template <typename EF> void run_ef_case(const EF& e, const std::vector<std::uint64_t>& values) {
        const ef_oracle oracle{values};
        const std::size_t n = values.size();

        REQUIRE(e.size() == n);

        for (std::size_t i = 0; i < n; ++i) {
            CAPTURE(i);
            CHECK(e[i] == values[i]);
            CHECK(e.select1(i) == values[i]);
        }

        if (n == 0)
            return;

        CHECK(e.back() == values.back());

        const std::uint64_t lo_probe = values.front() > 5 ? values.front() - 5 : 0;
        const std::uint64_t hi_probe = values.back() + 5;

        std::vector<std::uint64_t> probes;
        probes.reserve(values.size() * 3 + 80);
        probes.push_back(lo_probe);
        probes.push_back(hi_probe);
        for (const std::uint64_t v : values) {
            if (v > 0)
                probes.push_back(v - 1);
            probes.push_back(v);
            probes.push_back(v + 1);
        }
        {
            std::mt19937_64 prng(0xC0FFEEull);
            std::uniform_int_distribution<std::uint64_t> pick(lo_probe, hi_probe);
            for (int k = 0; k < 64; ++k)
                probes.push_back(pick(prng));
        }

        for (const std::uint64_t x : probes) {
            CAPTURE(x);
            CHECK(e.rank(x) == oracle.rank(x));

            const auto ng = e.nge(x);
            const auto ong = oracle.nge(x);
            CHECK(ng.pos == ong.pos);
            CHECK(ng.val == ong.val);

            const auto pl = e.ple(x);
            const auto opl = oracle.ple(x);
            CHECK(pl.pos == opl.pos);
            CHECK(pl.val == opl.val);

            const auto [lo, hi] = e.locate(x);
            const auto [olo, ohi] = oracle.locate(x);
            CHECK(lo.pos == olo.pos);
            CHECK(lo.val == olo.val);
            CHECK(hi.pos == ohi.pos);
            CHECK(hi.val == ohi.val);
        }

        {
            const std::vector<std::uint64_t> collected(e.begin(), e.end());
            CHECK(collected == values);

            std::vector<std::uint64_t> collected2;
            for (auto v : e)
                collected2.push_back(v);
            CHECK(collected2 == values);
        }

        CHECK(e.overhead_bits() > 0);
    }

    void run_ef_case(const std::vector<std::uint64_t>& values,
                     std::uint64_t universe_override = 0) {
        cds::ef<> e =
            (universe_override != 0) ? cds::ef<>(values, universe_override) : cds::ef<>(values);
        run_ef_case(e, values);

        cds::io::buffer_sink sink;
        REQUIRE(e.save(sink));

        auto bytes = sink.release();
        cds::io::buffer_source src(bytes);

        auto e2_result = cds::ef<>::load(src);
        REQUIRE(e2_result.has_value());
        auto& e2 = *e2_result;
        REQUIRE(e2.size() == values.size());
        CHECK(e2.universe() == e.universe());
        CHECK(e2.back() == e.back());
        for (std::size_t i = 0; i < values.size(); ++i) {
            CAPTURE(i);
            CHECK(e2[i] == values[i]);
        }

        cds::io::buffer_source vsrc(bytes);
        auto ev_result = cds::ef_view<>::load(vsrc);
        REQUIRE(ev_result.has_value());
        auto& ev = *ev_result;
        REQUIRE(ev.size() == values.size());
        CHECK(ev.universe() == e.universe());
        for (std::size_t i = 0; i < values.size(); ++i) {
            CAPTURE(i);
            CHECK(ev[i] == values[i]);
        }
        if (!values.empty()) {
            CHECK(ev.nge(values.front()).val == e.nge(values.front()).val);
            CHECK(ev.nge(values.back()).val == e.nge(values.back()).val);
        }
    }

} // namespace

TEST_CASE("select/ef") {
    SUBCASE("hand-verified reference sequence — nge/ple/locate") {

        const std::vector<std::uint64_t> values = {1, 3, 3, 4, 5, 6, 6, 9, 12, 14, 17, 17};
        cds::ef<> e(values);
        using rv = cds::ef<>::return_value;

        auto check = [](rv got, std::size_t pos, std::uint64_t val) {
            CHECK(got.pos == pos);
            CHECK(got.val == val);
        };

        check(e.nge(0), 0, 1);
        check(e.nge(3), 1, 3);
        check(e.nge(6), 5, 6);
        check(e.nge(7), 7, 9);
        check(e.nge(17), 10, 17);
        check(e.nge(23), 11, 17);

        CHECK(e.ple(0).pos == cds::ef<>::npos);
        check(e.ple(3), 2, 3);
        check(e.ple(6), 6, 6);
        check(e.ple(7), 6, 6);
        check(e.ple(17), 11, 17);
        check(e.ple(23), 11, 17);

        {
            auto [lo, hi] = e.locate(0);
            CHECK(lo.pos == cds::ef<>::npos);
            check(hi, 0, 1);
        }
        {
            auto [lo, hi] = e.locate(3);
            check(lo, 2, 3);
            check(hi, 3, 4);
        }
        {

            auto [lo, hi] = e.locate(17);
            check(lo, 11, 17);
            CHECK(hi.pos == cds::ef<>::npos);
        }
    }

    SUBCASE("empty") {
        run_ef_case({});
    }
    SUBCASE("single element") {
        run_ef_case({42});
    }
    SUBCASE("single element, explicit universe much larger than the value") {
        run_ef_case({42}, 1'000'000);
    }
    SUBCASE("all duplicates") {
        run_ef_case(std::vector<std::uint64_t>(500, 7));
    }
    SUBCASE("small, dense-ish values") {
        run_ef_case(make_sorted_random(200, 500, 1));
    }
    SUBCASE("small, sparse values (large universe)") {
        run_ef_case(make_sorted_random(200, 1'000'000, 2));
    }
    SUBCASE("medium, explicit universe far exceeding actual max value") {
        run_ef_case(make_sorted_random(1000, 5000, 3), 1'000'000);
    }
    SUBCASE("large") {
        run_ef_case(make_sorted_random(20000, 10'000'000, 4));
    }
    SUBCASE("many duplicates from a small universe") {
        std::mt19937_64 rng(5);
        std::uniform_int_distribution<std::uint64_t> dist(0, 99);
        std::vector<std::uint64_t> values(2000);
        for (auto& v : values)
            v = dist(rng);
        std::sort(values.begin(), values.end());
        run_ef_case(values);
    }

    SUBCASE("from_deltas / diff") {
        const std::vector<std::uint64_t> deltas = {3, 2, 5, 1, 16};
        auto e = cds::ef<>::from_deltas(deltas);

        REQUIRE(e.size() == deltas.size() + 1);
        CHECK(e[0] == 0);
        CHECK(e[1] == 3);
        CHECK(e[2] == 5);
        CHECK(e[3] == 10);
        CHECK(e[4] == 11);
        CHECK(e[5] == 27);

        for (std::size_t i = 0; i < deltas.size(); ++i) {
            CAPTURE(i);
            CHECK(e.diff(i) == deltas[i]);
        }
    }
    SUBCASE("from_deltas, randomized, against a running-sum oracle built independently") {
        std::mt19937_64 rng(6);
        std::uniform_int_distribution<std::uint64_t> dist(0, 1000);
        std::vector<std::uint64_t> deltas(500);
        for (auto& d : deltas)
            d = dist(rng);

        auto e = cds::ef<>::from_deltas(deltas);
        REQUIRE(e.size() == deltas.size() + 1);

        std::uint64_t running = 0;
        CHECK(e[0] == 0);
        for (std::size_t i = 0; i < deltas.size(); ++i) {
            running += deltas[i];
            CAPTURE(i);
            CHECK(e[i + 1] == running);
            CHECK(e.diff(i) == deltas[i]);
        }
    }

    SUBCASE("IndexZeros=false — nge/ple still work (neither actually needs select0)") {
        using narrow_ef =
            cds::ef<std::uint64_t, cds::pack_endian::lsb, cds::pack_mode::sparse, false>;
        const std::vector<std::uint64_t> values = {2, 5, 5, 8};
        narrow_ef e(values);

        run_ef_case(e, values);

        CHECK(e.rank(5) == 1);
        CHECK(e.rank(8) == 3);
        CHECK(e.rank(0) == 0);
        CHECK(e.rank(9) == 4);

        CHECK(e.nge(6).pos == 3);
        CHECK(e.nge(6).val == 8);
        CHECK(e.ple(6).pos == 2);
        CHECK(e.ple(6).val == 5);
    }
}
