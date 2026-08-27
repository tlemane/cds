#include <nanobench.h>

#include <cds/wavelet_matrix.hpp>
#include <cds/rank/poppy.hpp>
#include <cds/select/select9.hpp>
#include <cds/select/poppy.hpp>
#include <cds/select/darray.hpp>

#include <sdsl/wm_int.hpp>
#include <sdsl/construct.hpp>
#include <sdsl/util.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

    using u64 = std::uint64_t;
    using bvv = cds::bit_vector<u64, cds::pack_endian::lsb>;

    using lvl_darray =
        cds::bit_dict<bvv, cds::rank9<bvv>, cds::darray<bvv, cds::select_target::both>>;
    using lvl_sel9 =
        cds::bit_dict<bvv, cds::rank9<bvv>, cds::select9<bvv, cds::select_target::both>>;
    using lvl_poppy =
        cds::bit_dict<bvv, cds::rank_poppy<bvv>, cds::select_poppy<bvv, cds::select_target::both>>;

    using wm_darray = cds::wavelet_matrix<u64, 0, lvl_darray>;
    using wm_sel9 = cds::wavelet_matrix<u64, 0, lvl_sel9>;
    using wm_poppy = cds::wavelet_matrix<u64, 0, lvl_poppy>;

    struct queries {
        std::vector<std::size_t> idx;
        std::vector<u64> rc;
        std::vector<u64> sc;
        std::vector<std::size_t> sr;
    };

    [[nodiscard]] queries make_queries(ankerl::nanobench::Rng& rng, const std::vector<u64>& v,
                                       u64 sigma, std::size_t nq) {
        std::vector<std::size_t> count(static_cast<std::size_t>(sigma), 0);
        for (u64 x : v)
            ++count[static_cast<std::size_t>(x)];

        queries q;
        q.idx.resize(nq);
        q.rc.resize(nq);
        q.sc.resize(nq);
        q.sr.resize(nq);
        for (std::size_t k = 0; k < nq; ++k) {
            q.idx[k] = static_cast<std::size_t>(rng.bounded(static_cast<std::uint32_t>(v.size())));
            q.rc[k] = static_cast<u64>(rng.bounded(static_cast<std::uint32_t>(sigma)));
            u64 c = 0;
            do {
                c = static_cast<u64>(rng.bounded(static_cast<std::uint32_t>(sigma)));
            } while (count[static_cast<std::size_t>(c)] == 0);
            q.sc[k] = c;
            q.sr[k] = static_cast<std::size_t>(
                rng.bounded(static_cast<std::uint32_t>(count[static_cast<std::size_t>(c)])));
        }
        return q;
    }

    void run_wavelet(ankerl::nanobench::Rng& rng, std::size_t n, std::size_t bits,
                     const std::string& label) {
        const u64 sigma = u64{1} << bits;
        std::vector<u64> v(n);
        for (auto& x : v)
            x = static_cast<u64>(rng.bounded(static_cast<std::uint32_t>(sigma)));
        const std::size_t nq = 20'000;
        const queries q = make_queries(rng, v, sigma, nq);
        const std::span<const u64> span(v);

        sdsl::int_vector<> iv(n, 0, static_cast<std::uint8_t>(bits));
        for (std::size_t i = 0; i < n; ++i)
            iv[i] = v[i];

        sdsl::wm_int<> sdsl_wm;
        sdsl::construct_im(sdsl_wm, iv);
        const wm_darray wda(span, bits);
        const wm_sel9 ws9(span, bits);
        const wm_poppy wpp(span, bits);

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": build")
                .unit("symbol")
                .warmup(1)
                .relative(true)
                .minEpochIterations(1)
                .epochs(3);
            bench.batch(n).run("sdsl", [&] {
                sdsl::wm_int<> w;
                sdsl::construct_im(w, iv);
                ankerl::nanobench::doNotOptimizeAway(sdsl::size_in_bytes(w));
            });
            bench.batch(n).run("r9+darray", [&] {
                wm_darray w(span, bits);
                ankerl::nanobench::doNotOptimizeAway(std::as_const(w));
            });
            bench.batch(n).run("r9+select9", [&] {
                wm_sel9 w(span, bits);
                ankerl::nanobench::doNotOptimizeAway(std::as_const(w));
            });
            bench.batch(n).run("poppy+selpop", [&] {
                wm_poppy w(span, bits);
                ankerl::nanobench::doNotOptimizeAway(std::as_const(w));
            });
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": access")
                .unit("op")
                .batch(nq)
                .warmup(1)
                .relative(true)
                .minEpochIterations(1)
                .epochs(5);
            bench.run("sdsl", [&] {
                u64 s = 0;
                for (auto i : q.idx)
                    s += sdsl_wm[i];
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.run("r9+darray", [&] {
                u64 s = 0;
                for (auto i : q.idx)
                    s += wda.access(i);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.run("r9+select9", [&] {
                u64 s = 0;
                for (auto i : q.idx)
                    s += ws9.access(i);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.run("poppy+selpop", [&] {
                u64 s = 0;
                for (auto i : q.idx)
                    s += wpp.access(i);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": rank")
                .unit("op")
                .batch(nq)
                .warmup(1)
                .relative(true)
                .minEpochIterations(1)
                .epochs(5);
            bench.run("sdsl", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < nq; ++k)
                    s += sdsl_wm.rank(q.idx[k], q.rc[k]);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.run("r9+darray", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < nq; ++k)
                    s += wda.rank(q.rc[k], q.idx[k]);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.run("r9+select9", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < nq; ++k)
                    s += ws9.rank(q.rc[k], q.idx[k]);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.run("poppy+selpop", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < nq; ++k)
                    s += wpp.rank(q.rc[k], q.idx[k]);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": select")
                .unit("op")
                .batch(nq)
                .warmup(1)
                .relative(true)
                .minEpochIterations(1)
                .epochs(5);
            bench.run("sdsl", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < nq; ++k)
                    s += sdsl_wm.select(q.sr[k] + 1, q.sc[k]);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.run("r9+darray", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < nq; ++k)
                    s += wda.select(q.sc[k], q.sr[k]);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.run("r9+select9", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < nq; ++k)
                    s += ws9.select(q.sc[k], q.sr[k]);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            bench.run("poppy+selpop", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < nq; ++k)
                    s += wpp.select(q.sc[k], q.sr[k]);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
        }

        const auto bps = [n](std::size_t bytes) {
            return static_cast<double>(bytes) * 8.0 / static_cast<double>(n);
        };
        std::printf("%s: memory (bits/symbol)  sdsl %.2f | r9+darray %.2f | r9+select9 %.2f | "
                    "poppy+selpop %.2f\n\n",
                    label.c_str(), bps(sdsl::size_in_bytes(sdsl_wm)), bps(wda.memory_size()),
                    bps(ws9.memory_size()), bps(wpp.memory_size()));
    }

} // namespace

int main() {
    ankerl::nanobench::Rng rng;
    constexpr std::size_t n = 1'000'000;

    run_wavelet(rng, n, 2, "wavelet 2-bit (DNA/ACGT)");
    run_wavelet(rng, n, 8, "wavelet 8-bit");
    run_wavelet(rng, n, 16, "wavelet 16-bit");

    return 0;
}
