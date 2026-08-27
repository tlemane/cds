#include <nanobench.h>

#include <cds/bit/vector.hpp>
#include <cds/rank/rank9.hpp>
#include <cds/rank/poppy.hpp>
#include <cds/select/darray.hpp>
#include <cds/select/select9.hpp>
#include <cds/select/poppy.hpp>

#include <sdsl/int_vector.hpp>
#include <sdsl/select_support_mcl.hpp>
#include <sdsl/util.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace {

    using u64 = std::uint64_t;
    using bv = cds::bit_vector<u64, cds::pack_endian::lsb>;
    using ones_t = cds::select_target;

    // Core select1. darray is cds's standalone select (peer of sdsl
    // select_support_mcl); select9/select_poppy share the rank index instead of
    // storing an independent one.
    void run_select(ankerl::nanobench::Rng& rng, std::size_t n, double density,
                    const std::string& label) {
        std::mt19937_64 gen(rng());
        std::bernoulli_distribution dist(density);

        bv v;
        v.reserve(n);
        sdsl::bit_vector sv(n);
        std::size_t ones = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const bool b = dist(gen);
            v.push_back(b ? std::uint8_t{1} : std::uint8_t{0});
            sv[i] = b;
            ones += b ? 1u : 0u;
        }

        cds::darray<bv, ones_t::ones> da(v);
        cds::rank9<bv> r9(v);
        cds::rank_poppy<bv> rp(v);
        cds::select9<bv, ones_t::ones> s9(r9);
        cds::select_poppy<bv, ones_t::ones> sp(rp);
        sdsl::select_support_mcl<> mcl(&sv);

        const std::size_t nq = 100'000;
        std::vector<std::size_t> q(nq); // occurrence index in [0, ones)
        for (auto& x : q)
            x = static_cast<std::size_t>(rng.bounded(static_cast<std::uint32_t>(ones)));

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": build")
                .unit("bit")
                .batch(n)
                .warmup(3)
                .relative(true)
                .minEpochIterations(5);
            bench.run("sdsl mcl", [&] {
                sdsl::select_support_mcl<> s(&sv);
                ankerl::nanobench::doNotOptimizeAway(s.select(1));
            });
            bench.run("cds darray", [&] {
                cds::darray<bv, ones_t::ones> s(v);
                ankerl::nanobench::doNotOptimizeAway(s.select1(0));
            });
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": random select1")
                .unit("op")
                .batch(nq)
                .warmup(3)
                .relative(true)
                .minEpochIterations(10);
            bench.run("sdsl mcl", [&] {
                std::size_t s = 0;
                for (auto x : q)
                    s += mcl.select(x + 1);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.run("cds darray", [&] {
                std::size_t s = 0;
                for (auto x : q)
                    s += da.select1(x);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.run("cds select9", [&] {
                std::size_t s = 0;
                for (auto x : q)
                    s += s9.select1(x);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.run("cds select_poppy", [&] {
                std::size_t s = 0;
                for (auto x : q)
                    s += sp.select1(x);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
        }

        const auto bps = [n](std::size_t bytes) {
            return static_cast<double>(bytes) * 8.0 / static_cast<double>(n);
        };
        // select9/select_poppy overhead is the samples only; they reuse a rank
        // index the caller already has (not counted here).
        std::printf("%s: index overhead (bits/bit)  sdsl mcl %.3f | cds darray %.3f | cds select9 "
                    "%.3f (+rank) | cds select_poppy %.3f (+rank)\n\n",
                    label.c_str(), bps(sdsl::size_in_bytes(mcl)), bps(da.memory_size()),
                    bps(s9.memory_size()), bps(sp.memory_size()));
    }

} // namespace

int main() {
    ankerl::nanobench::Rng rng;
    constexpr std::size_t n = 1'000'000;

    run_select(rng, n, 0.5, "select 1M medium (p=0.5)");
    run_select(rng, n, 0.05, "select 1M sparse (p=0.05)");
    run_select(rng, n, 0.95, "select 1M dense (p=0.95)");

    return 0;
}
