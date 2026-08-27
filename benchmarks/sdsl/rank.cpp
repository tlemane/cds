#include <nanobench.h>

#include <cds/bit/vector.hpp>
#include <cds/rank/rank9.hpp>
#include <cds/rank/poppy.hpp>

#include <sdsl/int_vector.hpp>
#include <sdsl/rank_support_v.hpp>
#include <sdsl/rank_support_v5.hpp>
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

    // Fast peers: cds rank9 vs sdsl rank_support_v (both ~25% overhead).
    // Compact peers: cds rank_poppy (~3.5%) vs sdsl rank_support_v5 (~6.25%).
    void run_rank(ankerl::nanobench::Rng& rng, std::size_t n, double density,
                  const std::string& label) {
        std::mt19937_64 gen(rng());
        std::bernoulli_distribution dist(density);

        bv v;
        v.reserve(n);
        sdsl::bit_vector sv(n);
        for (std::size_t i = 0; i < n; ++i) {
            const bool b = dist(gen);
            v.push_back(b ? std::uint8_t{1} : std::uint8_t{0});
            sv[i] = b;
        }

        cds::rank9<bv> r9(v);
        cds::rank_poppy<bv> rp(v);
        sdsl::rank_support_v<> rv(&sv);
        sdsl::rank_support_v5<> rv5(&sv);

        const std::size_t nq = 100'000;
        std::vector<std::size_t> q(nq);
        for (auto& x : q)
            x = static_cast<std::size_t>(rng.bounded(static_cast<std::uint32_t>(n + 1)));

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": build")
                .unit("bit")
                .batch(n)
                .warmup(3)
                .relative(true)
                .minEpochIterations(5);
            bench.run("sdsl rank_v", [&] {
                sdsl::rank_support_v<> r(&sv);
                ankerl::nanobench::doNotOptimizeAway(r.rank(n));
            });
            bench.run("sdsl rank_v5", [&] {
                sdsl::rank_support_v5<> r(&sv);
                ankerl::nanobench::doNotOptimizeAway(r.rank(n));
            });
            bench.run("cds rank9", [&] {
                cds::rank9<bv> r(v);
                ankerl::nanobench::doNotOptimizeAway(r.rank1(n));
            });
            bench.run("cds rank_poppy", [&] {
                cds::rank_poppy<bv> r(v);
                ankerl::nanobench::doNotOptimizeAway(r.rank1(n));
            });
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": random rank")
                .unit("op")
                .batch(nq)
                .warmup(3)
                .relative(true)
                .minEpochIterations(10);
            bench.run("sdsl rank_v", [&] {
                std::size_t s = 0;
                for (auto x : q)
                    s += rv.rank(x);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.run("sdsl rank_v5", [&] {
                std::size_t s = 0;
                for (auto x : q)
                    s += rv5.rank(x);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.run("cds rank9", [&] {
                std::size_t s = 0;
                for (auto x : q)
                    s += r9.rank1(x);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.run("cds rank_poppy", [&] {
                std::size_t s = 0;
                for (auto x : q)
                    s += rp.rank1(x);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
        }

        const auto bps = [n](std::size_t bytes) {
            return static_cast<double>(bytes) * 8.0 / static_cast<double>(n);
        };
        std::printf("%s: index overhead (bits/bit)  sdsl rank_v %.3f | sdsl rank_v5 %.3f | cds "
                    "rank9 %.3f | cds rank_poppy %.3f\n\n",
                    label.c_str(), bps(sdsl::size_in_bytes(rv)), bps(sdsl::size_in_bytes(rv5)),
                    bps(r9.memory_size()), bps(rp.memory_size()));
    }

} // namespace

int main() {
    ankerl::nanobench::Rng rng;
    constexpr std::size_t n = 1'000'000;

    run_rank(rng, n, 0.5, "rank 1M medium (p=0.5)");
    run_rank(rng, n, 0.05, "rank 1M sparse (p=0.05)");
    run_rank(rng, n, 0.95, "rank 1M dense (p=0.95)");

    return 0;
}
