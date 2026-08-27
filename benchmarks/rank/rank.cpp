#include <nanobench.h>

#include <cds/bit/vector.hpp>
#include <cds/rank/rank9.hpp>
#include <cds/rank/scan.hpp>
#include <cds/rank/poppy.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace {

    using bit_vector_t = cds::bit_vector<std::uint64_t, cds::pack_endian::lsb>;

    void run_comparison(ankerl::nanobench::Rng& rng, std::size_t n, double density,
                        const std::string& label, bool with_scan = true) {
        std::mt19937_64 gen(rng());
        std::bernoulli_distribution dist(density);

        bit_vector_t v;
        std::vector<std::size_t> oracle_rank1(n + 1, 0);
        for (std::size_t i = 0; i < n; ++i) {
            const bool bit = dist(gen);
            v.push_back(bit ? std::uint8_t{1} : std::uint8_t{0});
            oracle_rank1[i + 1] = oracle_rank1[i] + (bit ? 1u : 0u);
        }

        {
            cds::rank_scan<bit_vector_t> rs(v);
            cds::rank9<bit_vector_t> r9(v);
            cds::rank_poppy<bit_vector_t> rpoppy(v);

            for (std::size_t i = 0; i <= n; i += std::max<std::size_t>(1, n / 2000)) {
                const std::size_t expected = oracle_rank1[i];
                const std::size_t rs_got = rs.rank1(i);
                const std::size_t r9_got = r9.rank1(i);
                const std::size_t rp_got = rpoppy.rank1(i);
                if (rs_got != expected || r9_got != expected || rp_got != expected) {
                    std::fprintf(
                        stderr,
                        "MISMATCH at i=%zu: expected=%zu rank_scan=%zu rank9=%zu rank_poppy=%zu\n",
                        i, expected, rs_got, r9_got, rp_got);
                    std::abort();
                }
            }
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": construction")
                .unit("element")
                .warmup(5)
                .relative(true)
                .minEpochIterations(10);

            if (with_scan) {

                bench.batch(n).run(label + ": rank_scan build", [&] {
                    cds::rank_scan<bit_vector_t> rs(v);
                    ankerl::nanobench::doNotOptimizeAway(rs);
                });
            }

            bench.batch(n).run(label + ": rank9 build", [&] {
                cds::rank9<bit_vector_t> r9(v);
                ankerl::nanobench::doNotOptimizeAway(r9);
            });

            bench.batch(n).run(label + ": rank_poppy build", [&] {
                cds::rank_poppy<bit_vector_t> rpoppy(v);
                ankerl::nanobench::doNotOptimizeAway(rpoppy);
            });
        }

        cds::rank_scan<bit_vector_t> rs(v);
        cds::rank9<bit_vector_t> r9(v);
        cds::rank_poppy<bit_vector_t> rpoppy(v);

        std::vector<std::size_t> queries(n);
        for (auto& q : queries)
            q = static_cast<std::size_t>(rng.bounded(static_cast<std::uint32_t>(n + 1)));

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": random rank1")
                .unit("element")
                .warmup(5)
                .relative(true)
                .minEpochIterations(10);

            if (with_scan) {

                bench.batch(n).run(label + ": rank_scan rank1", [&] {
                    std::size_t sum = 0;
                    for (auto q : queries)
                        sum += rs.rank1(q);
                    ankerl::nanobench::doNotOptimizeAway(sum);
                });
            }

            bench.batch(n).run(label + ": rank9 rank1", [&] {
                std::size_t sum = 0;
                for (auto q : queries)
                    sum += r9.rank1(q);
                ankerl::nanobench::doNotOptimizeAway(sum);
            });

            bench.batch(n).run(label + ": rank_poppy rank1", [&] {
                std::size_t sum = 0;
                for (auto q : queries)
                    sum += rpoppy.rank1(q);
                ankerl::nanobench::doNotOptimizeAway(sum);
            });
        }

        const std::size_t vec_bytes = v.nb_words() * sizeof(std::uint64_t);
        std::printf("%s: bit_vector (source) size=%zu bytes\n", label.c_str(), vec_bytes);
        if (with_scan) {
            std::printf("%s: rank_scan      memory_size=%zu bytes (%.2f%% overhead vs source)\n",
                        label.c_str(), rs.memory_size(),
                        100.0 * static_cast<double>(rs.memory_size()) /
                            static_cast<double>(vec_bytes));
        }
        std::printf("%s: rank9          memory_size=%zu bytes (%.3f bits/element, %.2f%% overhead "
                    "vs source)\n",
                    label.c_str(), r9.memory_size(),
                    (static_cast<double>(r9.memory_size()) * 8.0) / static_cast<double>(n),
                    100.0 * static_cast<double>(r9.memory_size()) / static_cast<double>(vec_bytes));
        std::printf("%s: rank_poppy     memory_size=%zu bytes (%.3f bits/element, %.2f%% overhead "
                    "vs source)\n",
                    label.c_str(), rpoppy.memory_size(),
                    (static_cast<double>(rpoppy.memory_size()) * 8.0) / static_cast<double>(n),
                    100.0 * static_cast<double>(rpoppy.memory_size()) /
                        static_cast<double>(vec_bytes));

        std::printf("\n\n");
    }

} // namespace

int main() {
    ankerl::nanobench::Rng rng;

    run_comparison(rng, 100'000, 0.5, "100k medium");
    run_comparison(rng, 100'000, 0.02, "100k sparse");
    run_comparison(rng, 100'000, 0.98, "100k dense");

    run_comparison(rng, 1'000'000, 0.5, "1M medium", false);
    run_comparison(rng, 1'000'000, 0.02, "1M sparse", false);
    run_comparison(rng, 1'000'000, 0.98, "1M dense", false);

    return 0;
}
