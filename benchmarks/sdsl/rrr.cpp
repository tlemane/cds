#include <nanobench.h>

#include <cds/rrr.hpp>
#include <cds/bit/vector.hpp>

#include <sdsl/rrr_vector.hpp>
#include <sdsl/util.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

    using u64 = std::uint64_t;

    template <std::uint16_t BS>
    void run_rrr(ankerl::nanobench::Rng& rng, std::size_t n, double density,
                 const std::string& label) {
        using cds_rrr = cds::rrr<BS>;
        using sdsl_rrr = sdsl::rrr_vector<BS>;

        cds::bit_vector<u64, cds::pack_endian::lsb> cbv;
        cbv.reserve(n);
        sdsl::bit_vector sbv(n);
        std::size_t ones = 0;
        const std::uint32_t thresh = static_cast<std::uint32_t>(density * 4294967296.0);
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint8_t b = (static_cast<std::uint32_t>(rng()) < thresh) ? 1 : 0;
            cbv.push_back(b);
            sbv[i] = b;
            ones += b;
        }
        if (ones == 0)
            return;

        std::vector<std::size_t> idx(n);
        for (auto& x : idx)
            x = static_cast<std::size_t>(rng.bounded(static_cast<std::uint32_t>(n)));
        std::vector<std::size_t> qr(n);
        for (auto& x : qr)
            x = static_cast<std::size_t>(rng.bounded(static_cast<std::uint32_t>(ones)));

        const cds_rrr cr(cbv);
        sdsl_rrr sr(sbv);
        typename sdsl_rrr::rank_1_type srank(&sr);
        typename sdsl_rrr::select_1_type sselect(&sr);

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": build")
                .unit("bit")
                .warmup(3)
                .relative(true)
                .minEpochIterations(1);
            bench.batch(n).run("sdsl", [&] {
                sdsl_rrr s(sbv);
                typename sdsl_rrr::rank_1_type rk(&s);
                typename sdsl_rrr::select_1_type sl(&s);
                ankerl::nanobench::doNotOptimizeAway(std::as_const(s));
                ankerl::nanobench::doNotOptimizeAway(std::as_const(rk));
                ankerl::nanobench::doNotOptimizeAway(std::as_const(sl));
            });
            bench.batch(n).run("cds", [&] {
                cds_rrr c(cbv);
                ankerl::nanobench::doNotOptimizeAway(std::as_const(c));
            });
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": access")
                .unit("op")
                .warmup(5)
                .relative(true)
                .minEpochIterations(3);
            bench.batch(n).run("sdsl", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < n; ++k)
                    s += sr[idx[k]];
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.batch(n).run("cds", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < n; ++k)
                    s += cr[idx[k]];
                ankerl::nanobench::doNotOptimizeAway(s);
            });
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": rank1")
                .unit("op")
                .warmup(5)
                .relative(true)
                .minEpochIterations(3);
            bench.batch(n).run("sdsl", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < n; ++k)
                    s += srank(idx[k]);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.batch(n).run("cds", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < n; ++k)
                    s += cr.rank1(idx[k]);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": select1")
                .unit("op")
                .warmup(5)
                .relative(true)
                .minEpochIterations(3);
            bench.batch(n).run("sdsl", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < n; ++k)
                    s += sselect(qr[k] + 1);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.batch(n).run("cds", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < n; ++k)
                    s += cr.select1(qr[k]);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
        }

        const double sdsl_bytes = static_cast<double>(
            sdsl::size_in_bytes(sr) + sdsl::size_in_bytes(srank) + sdsl::size_in_bytes(sselect));
        const double cds_bytes = static_cast<double>(cr.memory_size());
        const double nn = static_cast<double>(n);
        std::printf("  %s memory (bits/bit): sdsl %.3f | cds %.3f (%.2fx) [raw = 1.0]\n\n",
                    label.c_str(), sdsl_bytes * 8.0 / nn, cds_bytes * 8.0 / nn,
                    (cds_bytes > 0.0) ? sdsl_bytes / cds_bytes : 0.0);
    }

} // namespace

int main() {
    ankerl::nanobench::Rng rng;
    constexpr std::size_t n = 1'000'000;

    for (double density : {0.05, 0.5}) {
        char lbl[64];
        std::snprintf(lbl, sizeof lbl, "rrr<15> %.0f%%", density * 100.0);
        run_rrr<15>(rng, n, density, lbl);
        std::snprintf(lbl, sizeof lbl, "rrr<31> %.0f%%", density * 100.0);
        run_rrr<31>(rng, n, density, lbl);
        std::snprintf(lbl, sizeof lbl, "rrr<63> %.0f%%", density * 100.0);
        run_rrr<63>(rng, n, density, lbl);
    }

    return 0;
}
