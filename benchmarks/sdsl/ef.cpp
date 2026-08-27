#include <nanobench.h>

#include <cds/ef.hpp>
#include <cds/rank/poppy.hpp>
#include <cds/rank/rank9.hpp>
#include <cds/select/poppy.hpp>
#include <cds/select/rank_backed_select.hpp>
#include <cds/select/select9.hpp>

#include <sdsl/sd_vector.hpp>
#include <sdsl/util.hpp>

#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

    using u64 = std::uint64_t;
    using sd_vector = sdsl::sd_vector<>;
    using sd_rank = sd_vector::rank_1_type;
    using sd_select = sd_vector::select_1_type;

    using ef_hb = cds::bit_vector<u64, cds::pack_endian::lsb>;
    using poppy_index = cds::rank_backed_select<ef_hb, cds::rank_poppy<ef_hb>,
                                                cds::select_poppy<ef_hb, cds::select_target::both>>;
    using ef_poppy = cds::ef<u64, cds::pack_endian::lsb, cds::pack_mode::dense, true, poppy_index>;

    using select9_index = cds::rank_backed_select<ef_hb, cds::rank9<ef_hb>,
                                                  cds::select9<ef_hb, cds::select_target::both>>;
    using ef_select9 =
        cds::ef<u64, cds::pack_endian::lsb, cds::pack_mode::dense, true, select9_index>;

    [[nodiscard]] std::vector<u64> make_increasing(ankerl::nanobench::Rng& rng, std::size_t n,
                                                   u64 avg_gap) {
        std::vector<u64> values(n);
        u64 running = 0;
        for (std::size_t i = 0; i < n; ++i) {
            running += 1 + rng.bounded(static_cast<std::uint32_t>(2 * avg_gap));
            values[i] = running;
        }
        return values;
    }

    [[nodiscard]] sd_vector build_sd(const std::vector<u64>& values, u64 universe) {
        sdsl::sd_vector_builder builder(universe, values.size());
        for (u64 v : values)
            builder.set(v);
        return sd_vector(builder);
    }

    void run_ef(ankerl::nanobench::Rng& rng, std::size_t n, u64 avg_gap, const std::string& label) {
        const std::vector<u64> values = make_increasing(rng, n, avg_gap);
        const u64 universe = values.back() + 1;

        std::vector<std::size_t> idx(n);
        for (auto& i : idx)
            i = static_cast<std::size_t>(rng.bounded(static_cast<std::uint32_t>(n)));
        std::vector<u64> qs(n);
        for (auto& q : qs)
            q = rng.bounded(static_cast<std::uint32_t>(universe > 0 ? universe : 1));

        const sd_vector sd = build_sd(values, universe);
        const sd_rank srank(&sd);
        const sd_select sselect(&sd);
        const cds::ef<> ours(values, universe);

        using ef_nz = cds::ef<u64, cds::pack_endian::lsb, cds::pack_mode::dense, false>;
        const ef_nz ours_nz(values, universe);
        const ef_poppy ours_poppy(values, universe);
        const ef_select9 ours_s9(values, universe);

        for (std::size_t i = 0; i < n; i += (n / 8) + 1) {
            if (ours[i] != values[i] || sselect(i + 1) != values[i])
                std::fprintf(stderr, "ef sanity MISMATCH at i=%zu: cds=%llu sdsl=%llu src=%llu\n",
                             i, (unsigned long long)ours[i], (unsigned long long)sselect(i + 1),
                             (unsigned long long)values[i]);
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": build")
                .unit("element")
                .warmup(3)
                .relative(true)
                .minEpochIterations(1);
            bench.batch(n).run("sdsl::sd_vector", [&] {
                sd_vector sd2 = build_sd(values, universe);
                sd_rank r2(&sd2);
                sd_select s2(&sd2);
                ankerl::nanobench::doNotOptimizeAway(std::as_const(sd2));
                ankerl::nanobench::doNotOptimizeAway(std::as_const(r2));
                ankerl::nanobench::doNotOptimizeAway(std::as_const(s2));
            });
            bench.batch(n).run("cds ef<darray<both>>", [&] {
                cds::ef<> ef(values, universe);
                ankerl::nanobench::doNotOptimizeAway(std::as_const(ef));
            });
            bench.batch(n).run("cds ef<darray<ones>>", [&] {
                ef_nz ef(values, universe);
                ankerl::nanobench::doNotOptimizeAway(std::as_const(ef));
            });
            bench.batch(n).run("cds ef<poppy>", [&] {
                ef_poppy ef(values, universe);
                ankerl::nanobench::doNotOptimizeAway(std::as_const(ef));
            });
            bench.batch(n).run("cds ef<select9>", [&] {
                ef_select9 ef(values, universe);
                ankerl::nanobench::doNotOptimizeAway(std::as_const(ef));
            });
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": random access")
                .unit("element")
                .warmup(5)
                .relative(true)
                .minEpochIterations(3);
            bench.batch(n).run("sdsl::sd_vector", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < n; ++k)
                    s += sselect(idx[k] + 1);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.batch(n).run("cds ef<darray<both>>", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < n; ++k)
                    s += ours[idx[k]];
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.batch(n).run("cds ef<darray<ones>>", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < n; ++k)
                    s += ours_nz[idx[k]];
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.batch(n).run("cds ef<poppy>", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < n; ++k)
                    s += ours_poppy[idx[k]];
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.batch(n).run("cds ef<select9>", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < n; ++k)
                    s += ours_s9[idx[k]];
                ankerl::nanobench::doNotOptimizeAway(s);
            });
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": next-geq")
                .unit("element")
                .warmup(3)
                .relative(true)
                .minEpochIterations(1);
            bench.batch(n).run("sdsl::sd_vector", [&] {
                u64 s = 0;
                for (u64 q : qs) {
                    const std::size_t r = srank(q);
                    s += (r < n) ? sselect(r + 1) : values.back();
                }
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.batch(n).run("cds ef<darray<both>>", [&] {
                u64 s = 0;
                for (u64 q : qs)
                    s += ours.nge(q).val;
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.batch(n).run("cds ef<darray<ones>>", [&] {
                u64 s = 0;
                for (u64 q : qs)
                    s += ours_nz.nge(q).val;
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.batch(n).run("cds ef<poppy>", [&] {
                u64 s = 0;
                for (u64 q : qs)
                    s += ours_poppy.nge(q).val;
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.batch(n).run("cds ef<select9>", [&] {
                u64 s = 0;
                for (u64 q : qs)
                    s += ours_s9.nge(q).val;
                ankerl::nanobench::doNotOptimizeAway(s);
            });
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": sequential decode")
                .unit("element")
                .warmup(5)
                .relative(true)
                .minEpochIterations(3);
            bench.batch(n).run("sdsl::sd_vector", [&] {
                u64 s = 0;
                for (std::size_t i = 0; i < n; ++i)
                    s += sselect(i + 1);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.batch(n).run("cds ef<darray<both>>", [&] {
                u64 s = 0;
                for (auto v : ours)
                    s += v;
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.batch(n).run("cds ef<darray<ones>>", [&] {
                u64 s = 0;
                for (auto v : ours_nz)
                    s += v;
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.batch(n).run("cds ef<poppy>", [&] {
                u64 s = 0;
                for (auto v : ours_poppy)
                    s += v;
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.batch(n).run("cds ef<select9>", [&] {
                u64 s = 0;
                for (auto v : ours_s9)
                    s += v;
                ankerl::nanobench::doNotOptimizeAway(s);
            });
        }

        const double sdsl_bytes = static_cast<double>(
            sdsl::size_in_bytes(sd) + sdsl::size_in_bytes(srank) + sdsl::size_in_bytes(sselect));
        const double cds_bytes = static_cast<double>(ours.memory_size());
        const double cds_nz_bytes = static_cast<double>(ours_nz.memory_size());
        const double cds_poppy_bytes = static_cast<double>(ours_poppy.memory_size());
        const double cds_s9_bytes = static_cast<double>(ours_s9.memory_size());
        const double nn = static_cast<double>(n);
        std::printf("  %s memory (bits/elem): sdsl::sd_vector %.2f | cds<darray<both>> %.2f "
                    "(%.2fx) | cds<darray<ones>> %.2f (%.2fx) | cds ef<poppy> %.2f (%.2fx) | cds "
                    "ef<select9> %.2f (%.2fx)\n\n",
                    label.c_str(), sdsl_bytes * 8.0 / nn, cds_bytes * 8.0 / nn,
                    sdsl_bytes / cds_bytes, cds_nz_bytes * 8.0 / nn, sdsl_bytes / cds_nz_bytes,
                    cds_poppy_bytes * 8.0 / nn, sdsl_bytes / cds_poppy_bytes,
                    cds_s9_bytes * 8.0 / nn, sdsl_bytes / cds_s9_bytes);
    }

} // namespace

int main() {
    ankerl::nanobench::Rng rng;
    constexpr std::size_t n = 1'000'000;

    run_ef(rng, n, 2, "dense (avg gap ~2)");
    run_ef(rng, n, 16, "medium (avg gap ~16)");
    run_ef(rng, n, 128, "sparse (avg gap ~128)");

    return 0;
}
