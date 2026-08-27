
#include <nanobench.h>

#include <cds/packed/vector.hpp>

#include <sdsl/int_vector.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

    using u64 = std::uint64_t;

    [[nodiscard]] std::vector<u64> random_values(ankerl::nanobench::Rng& rng, std::size_t n,
                                                 std::uint8_t width) {
        const u64 mask = (width >= 64) ? ~u64{0} : ((u64{1} << width) - 1);
        std::vector<u64> values(n);
        for (auto& v : values)
            v = rng() & mask;
        return values;
    }

    [[nodiscard]] std::vector<std::size_t> random_indices(ankerl::nanobench::Rng& rng,
                                                          std::size_t n) {
        std::vector<std::size_t> idx(n);
        for (auto& i : idx)
            i = static_cast<std::size_t>(rng.bounded(static_cast<std::uint32_t>(n)));
        return idx;
    }

    template <typename CdsT>
    void add_cds_build(ankerl::nanobench::Bench& bench, std::size_t n,
                       const std::vector<u64>& values, const char* name) {
        bench.batch(n).run(name, [&] {
            CdsT v;
            v.reserve(n);
            for (u64 x : values)
                v.push_back(x);
            ankerl::nanobench::doNotOptimizeAway(std::as_const(v));
        });
    }

    template <typename CdsT>
    void add_cds_read(ankerl::nanobench::Bench& bench, std::size_t n,
                      const std::vector<u64>& values, const std::vector<std::size_t>& idx,
                      const char* name) {
        CdsT v;
        v.reserve(n);
        for (u64 x : values)
            v.push_back(x);
        bench.batch(n).run(name, [&] {
            u64 s = 0;
            for (std::size_t k = 0; k < n; ++k)
                s += v[idx[k]];
            ankerl::nanobench::doNotOptimizeAway(s);
        });
    }

    template <typename CdsT, bool Unsafe>
    void add_cds_write(ankerl::nanobench::Bench& bench, std::size_t n,
                       const std::vector<u64>& values, const std::vector<std::size_t>& idx,
                       const char* name) {
        CdsT v;
        v.reserve(n);
        for (u64 x : values)
            v.push_back(x);
        bench.batch(n).run(name, [&] {
            for (std::size_t k = 0; k < n; ++k) {
                if constexpr (Unsafe)
                    v[idx[k]] = cds::unsafe(values[k]);
                else
                    v[idx[k]] = values[k];
            }
            ankerl::nanobench::doNotOptimizeAway(std::as_const(v));
        });
    }

    template <typename CdsT>
    void add_cds_seq(ankerl::nanobench::Bench& bench, std::size_t n, const std::vector<u64>& values,
                     const char* name) {
        CdsT v;
        v.reserve(n);
        for (u64 x : values)
            v.push_back(x);
        bench.batch(n).run(name, [&] {
            u64 s = 0;
            for (auto x : v)
                s += static_cast<u64>(x);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
    }

    template <std::uint8_t W>
    void run_width(ankerl::nanobench::Rng& rng, std::size_t n, const std::string& label) {
        using sparse_lsb =
            cds::packed_vector<u64, u64, W, cds::pack_endian::lsb, cds::pack_mode::sparse>;
        using dense_lsb =
            cds::packed_vector<u64, u64, W, cds::pack_endian::lsb, cds::pack_mode::dense>;
        using sparse_msb =
            cds::packed_vector<u64, u64, W, cds::pack_endian::msb, cds::pack_mode::sparse>;
        using dense_msb =
            cds::packed_vector<u64, u64, W, cds::pack_endian::msb, cds::pack_mode::dense>;
        using sdsl_t = sdsl::int_vector<W>;

        const std::vector<u64> values = random_values(rng, n, W);
        const std::vector<std::size_t> idx = random_indices(rng, n);

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": build")
                .unit("element")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);

            bench.batch(n).run("sdsl", [&] {
                sdsl_t v(n);
                for (std::size_t i = 0; i < n; ++i)
                    v[i] = values[i];
                ankerl::nanobench::doNotOptimizeAway(std::as_const(v));
            });
            add_cds_build<sparse_lsb>(bench, n, values, "cds lsb/sparse");
            add_cds_build<dense_lsb>(bench, n, values, "cds lsb/dense");
            add_cds_build<sparse_msb>(bench, n, values, "cds msb/sparse");
            add_cds_build<dense_msb>(bench, n, values, "cds msb/dense");
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": random read")
                .unit("element")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);

            sdsl_t sv(n);
            for (std::size_t i = 0; i < n; ++i)
                sv[i] = values[i];
            bench.batch(n).run("sdsl", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < n; ++k)
                    s += sv[idx[k]];
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            add_cds_read<sparse_lsb>(bench, n, values, idx, "cds lsb/sparse");
            add_cds_read<dense_lsb>(bench, n, values, idx, "cds lsb/dense");
            add_cds_read<sparse_msb>(bench, n, values, idx, "cds msb/sparse");
            add_cds_read<dense_msb>(bench, n, values, idx, "cds msb/dense");
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": random write")
                .unit("element")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);

            sdsl_t sv(n);
            for (std::size_t i = 0; i < n; ++i)
                sv[i] = values[i];
            bench.batch(n).run("sdsl", [&] {
                for (std::size_t k = 0; k < n; ++k)
                    sv[idx[k]] = values[k];
                ankerl::nanobench::doNotOptimizeAway(std::as_const(sv));
            });
            add_cds_write<sparse_lsb, false>(bench, n, values, idx, "cds lsb/sparse");
            add_cds_write<sparse_lsb, true>(bench, n, values, idx, "cds lsb/sparse (u)");
            add_cds_write<dense_lsb, false>(bench, n, values, idx, "cds lsb/dense");
            add_cds_write<dense_lsb, true>(bench, n, values, idx, "cds lsb/dense (u)");
            add_cds_write<sparse_msb, false>(bench, n, values, idx, "cds msb/sparse");
            add_cds_write<sparse_msb, true>(bench, n, values, idx, "cds msb/sparse (u)");
            add_cds_write<dense_msb, false>(bench, n, values, idx, "cds msb/dense");
            add_cds_write<dense_msb, true>(bench, n, values, idx, "cds msb/dense (u)");
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": sequential sum")
                .unit("element")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);

            sdsl_t sv(n);
            for (std::size_t i = 0; i < n; ++i)
                sv[i] = values[i];
            bench.batch(n).run("sdsl", [&] {
                u64 s = 0;
                for (auto x : sv)
                    s += static_cast<u64>(x);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            add_cds_seq<sparse_lsb>(bench, n, values, "cds lsb/sparse");
            add_cds_seq<dense_lsb>(bench, n, values, "cds lsb/dense");
            add_cds_seq<sparse_msb>(bench, n, values, "cds msb/sparse");
            add_cds_seq<dense_msb>(bench, n, values, "cds msb/dense");
        }
    }

} // namespace

int main() {
    ankerl::nanobench::Rng rng;
    constexpr std::size_t n = 1'000'000;

    run_width<4>(rng, n, "W4");
    run_width<12>(rng, n, "W12");
    run_width<17>(rng, n, "W17");
    run_width<20>(rng, n, "W20");
    run_width<40>(rng, n, "W40");

    return 0;
}
