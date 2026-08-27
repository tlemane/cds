#include <nanobench.h>

#include <cds/bit/vector.hpp>
#include <cds/rank/rank9.hpp>
#include <cds/select/scan.hpp>
#include <cds/select/select9.hpp>
#include <cds/select/darray.hpp>
#include <cds/select/poppy.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace {

    using bit_vector_t = cds::bit_vector<std::uint64_t, cds::pack_endian::lsb>;
    using cds::select_target;

    [[nodiscard]] const char* target_name(select_target t) {
        switch (t) {
            case select_target::ones: return "ones";
            case select_target::zeros: return "zeros";
            case select_target::both: return "both";
        }
        return "?";
    }

    template <select_target Target>
    void run_target_case(ankerl::nanobench::Rng& rng, const bit_vector_t& v, std::size_t n,
                         const std::vector<std::size_t>& one_positions,
                         const std::vector<std::size_t>& zero_positions, const std::string& label) {
        constexpr bool has_ones = (Target == select_target::ones || Target == select_target::both);
        constexpr bool has_zeros =
            (Target == select_target::zeros || Target == select_target::both);

        const std::string tlabel = label + " [" + target_name(Target) + "]";

        {
            cds::select_scan<bit_vector_t> ss(v);
            cds::rank9<bit_vector_t> r9(v);
            cds::select9<bit_vector_t, Target> s9(r9);
            cds::darray<bit_vector_t, Target> da(v);
            cds::rank_poppy<bit_vector_t> rp(v);
            cds::select_poppy<bit_vector_t, Target> sp(rp);

            if constexpr (has_ones) {
                for (std::size_t r = 0; r < one_positions.size();
                     r += std::max<std::size_t>(1, one_positions.size() / 1000)) {
                    const std::size_t expected = one_positions[r];
                    const std::size_t ss_got = ss.select1(r);
                    const std::size_t s9_got = s9.select1(r);
                    const std::size_t da_got = da.select1(r);
                    const std::size_t sp_got = sp.select1(r);
                    if (ss_got != expected || s9_got != expected || da_got != expected ||
                        sp_got != expected) {
                        std::fprintf(stderr,
                                     "%s select1 MISMATCH at r=%zu: expected=%zu select_scan=%zu "
                                     "select9=%zu darray=%zu poppy=%zu\n",
                                     tlabel.c_str(), r, expected, ss_got, s9_got, da_got, sp_got);
                        std::abort();
                    }
                }
            }
            if constexpr (has_zeros) {
                for (std::size_t r = 0; r < zero_positions.size();
                     r += std::max<std::size_t>(1, zero_positions.size() / 1000)) {
                    const std::size_t expected = zero_positions[r];
                    const std::size_t ss_got = ss.select0(r);
                    const std::size_t s9_got = s9.select0(r);
                    const std::size_t da_got = da.select0(r);
                    const std::size_t sp_got = sp.select0(r);
                    if (ss_got != expected || s9_got != expected || da_got != expected ||
                        sp_got != expected) {
                        std::fprintf(stderr,
                                     "%s select0 MISMATCH at r=%zu: expected=%zu select_scan=%zu "
                                     "select9=%zu darray=%zu poppy=%zu\n",
                                     tlabel.c_str(), r, expected, ss_got, s9_got, da_got, sp_got);
                        std::abort();
                    }
                }
            }
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(tlabel + ": construction")
                .unit("element")
                .warmup(5)
                .relative(true)
                .minEpochIterations(10);

            bench.batch(n).run(tlabel + ": select_scan construction", [&] {
                cds::select_scan<bit_vector_t> ss(v);
                ankerl::nanobench::doNotOptimizeAway(ss);
            });

            bench.batch(n).run(tlabel + ": rank9 construction (select9's dependency)", [&] {
                cds::rank9<bit_vector_t> r9(v);
                ankerl::nanobench::doNotOptimizeAway(r9);
            });

            cds::rank9<bit_vector_t> r9_for_hints(v);
            bench.batch(n).run(tlabel + ": select9 hints-only construction", [&] {
                cds::select9<bit_vector_t, Target> s9(r9_for_hints);
                ankerl::nanobench::doNotOptimizeAway(s9);
            });

            bench.batch(n).run(tlabel + ": darray construction (self-contained)", [&] {
                cds::darray<bit_vector_t, Target> da(v);
                ankerl::nanobench::doNotOptimizeAway(da);
            });

            bench.batch(n).run(label + ": rank_poppy construction (select_poppy dependency)", [&] {
                cds::rank_poppy<bit_vector_t> rpoppy(v);
                ankerl::nanobench::doNotOptimizeAway(rpoppy);
            });

            cds::rank_poppy<bit_vector_t> rpoppy(v);
            bench.batch(n).run(tlabel + ": select_poppy only construction", [&] {
                cds::select_poppy<bit_vector_t, Target> spoppy(rpoppy);
                ankerl::nanobench::doNotOptimizeAway(spoppy);
            });
        }

        cds::select_scan<bit_vector_t> ss(v);
        cds::rank9<bit_vector_t> r9(v);
        cds::select9<bit_vector_t, Target> s9(r9);
        cds::darray<bit_vector_t, Target> da(v);
        cds::rank_poppy<bit_vector_t> rp(v);
        cds::select_poppy<bit_vector_t, Target> sp(rp);

        if constexpr (has_ones) {
            std::vector<std::size_t> queries(one_positions.size());
            for (auto& q : queries)
                q = static_cast<std::size_t>(
                    rng.bounded(static_cast<std::uint32_t>(one_positions.size())));

            ankerl::nanobench::Bench bench;
            bench.title(tlabel + ": random select1")
                .unit("element")
                .warmup(5)
                .relative(true)
                .minEpochIterations(10);

            bench.batch(queries.size()).run(tlabel + ": select_scan select1", [&] {
                std::size_t sum = 0;
                for (auto q : queries)
                    sum += ss.select1(q);
                ankerl::nanobench::doNotOptimizeAway(sum);
            });
            bench.batch(queries.size()).run(tlabel + ": select9 select1", [&] {
                std::size_t sum = 0;
                for (auto q : queries)
                    sum += s9.select1(q);
                ankerl::nanobench::doNotOptimizeAway(sum);
            });
            bench.batch(queries.size()).run(tlabel + ": darray select1", [&] {
                std::size_t sum = 0;
                for (auto q : queries)
                    sum += da.select1(q);
                ankerl::nanobench::doNotOptimizeAway(sum);
            });
            bench.batch(queries.size()).run(tlabel + ": poppy select1", [&] {
                std::size_t sum = 0;
                for (auto q : queries)
                    sum += sp.select1(q);
                ankerl::nanobench::doNotOptimizeAway(sum);
            });
        }

        if constexpr (has_zeros) {
            std::vector<std::size_t> queries(zero_positions.size());
            for (auto& q : queries)
                q = static_cast<std::size_t>(
                    rng.bounded(static_cast<std::uint32_t>(zero_positions.size())));

            ankerl::nanobench::Bench bench;
            bench.title(tlabel + ": random select0")
                .unit("element")
                .warmup(5)
                .relative(true)
                .minEpochIterations(10);

            bench.batch(queries.size()).run(tlabel + ": select_scan select0", [&] {
                std::size_t sum = 0;
                for (auto q : queries)
                    sum += ss.select0(q);
                ankerl::nanobench::doNotOptimizeAway(sum);
            });
            bench.batch(queries.size()).run(tlabel + ": select9 select0", [&] {
                std::size_t sum = 0;
                for (auto q : queries)
                    sum += s9.select0(q);
                ankerl::nanobench::doNotOptimizeAway(sum);
            });
            bench.batch(queries.size()).run(tlabel + ": darray select0", [&] {
                std::size_t sum = 0;
                for (auto q : queries)
                    sum += da.select0(q);
                ankerl::nanobench::doNotOptimizeAway(sum);
            });
            bench.batch(queries.size()).run(tlabel + ": poppy select0", [&] {
                std::size_t sum = 0;
                for (auto q : queries)
                    sum += sp.select0(q);
                ankerl::nanobench::doNotOptimizeAway(sum);
            });
        }

        const std::size_t vec_bytes = v.nb_words() * sizeof(std::uint64_t);
        const std::size_t select9_total = r9.memory_size() + s9.memory_size();
        const std::size_t select_p_total = rp.memory_size() + sp.memory_size();

        std::printf("%s: bit_vector (source)        size=%zu bytes\n", tlabel.c_str(), vec_bytes);
        std::printf(
            "%s: select_scan                memory_size=%zu bytes (%.2f%% overhead vs source)\n",
            tlabel.c_str(), ss.memory_size(),
            100.0 * static_cast<double>(ss.memory_size()) / static_cast<double>(vec_bytes));
        std::printf(
            "%s: rank9 (select9 dependency) memory_size=%zu bytes (%.2f%% overhead vs source)\n",
            tlabel.c_str(), r9.memory_size(),
            100.0 * static_cast<double>(r9.memory_size()) / static_cast<double>(vec_bytes));
        std::printf(
            "%s: select9 (hints only)       memory_size=%zu bytes (%.2f%% overhead vs source)\n",
            tlabel.c_str(), s9.memory_size(),
            100.0 * static_cast<double>(s9.memory_size()) / static_cast<double>(vec_bytes));
        std::printf("%s: select9 TOTAL (rank9 + select9 hints) = %zu bytes (%.2f%% overhead vs "
                    "source)  <-- the real cost of using select9\n",
                    tlabel.c_str(), select9_total,
                    100.0 * static_cast<double>(select9_total) / static_cast<double>(vec_bytes));
        std::printf(
            "%s: darray (self-contained)    memory_size=%zu bytes (%.2f%% overhead vs source)\n",
            tlabel.c_str(), da.memory_size(),
            100.0 * static_cast<double>(da.memory_size()) / static_cast<double>(vec_bytes));
        std::printf("%s: rank_poppy (select_poppy dependency) memory_size=%zu bytes (%.2f%% "
                    "overhead vs source)\n",
                    tlabel.c_str(), rp.memory_size(),
                    100.0 * static_cast<double>(rp.memory_size()) / static_cast<double>(vec_bytes));
        std::printf(
            "%s: select_poppy ( only)       memory_size=%zu bytes (%.2f%% overhead vs source)\n",
            tlabel.c_str(), sp.memory_size(),
            100.0 * static_cast<double>(sp.memory_size()) / static_cast<double>(vec_bytes));
        std::printf("%s: select_poppy TOTAL (rank + select) = %zu bytes (%.2f%% overhead vs "
                    "source)  <-- the real cost of using select_poppy\n",
                    tlabel.c_str(), select_p_total,
                    100.0 * static_cast<double>(select_p_total) / static_cast<double>(vec_bytes));
    }

    void run_comparison(ankerl::nanobench::Rng& rng, std::size_t n, double density,
                        const std::string& label) {
        std::mt19937_64 gen(rng());
        std::bernoulli_distribution dist(density);

        bit_vector_t v;
        std::vector<std::size_t> one_positions;
        std::vector<std::size_t> zero_positions;
        for (std::size_t i = 0; i < n; ++i) {
            const bool bit = dist(gen);
            v.push_back(bit ? std::uint8_t{1} : std::uint8_t{0});
            (bit ? one_positions : zero_positions).push_back(i);
        }

        if (one_positions.empty() || zero_positions.empty()) {
            std::fprintf(stderr,
                         "%s: all-one or all-zero at this density/size, skipping (need both sides "
                         "for the ::both/::zeros cases)\n",
                         label.c_str());
            return;
        }

        run_target_case<select_target::ones>(rng, v, n, one_positions, zero_positions, label);
        run_target_case<select_target::zeros>(rng, v, n, one_positions, zero_positions, label);
        run_target_case<select_target::both>(rng, v, n, one_positions, zero_positions, label);
    }

} // namespace

int main() {
    ankerl::nanobench::Rng rng;

    run_comparison(rng, 100'000, 0.5, "100k elements, medium density");
    run_comparison(rng, 100'000, 0.02, "100k elements, sparse");
    run_comparison(rng, 100'000, 0.98, "100k elements, dense");

    return 0;
}
