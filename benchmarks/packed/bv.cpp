#include <nanobench.h>

#include <cds/bit/vector.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

    void run_bit_vector_benchmarks(ankerl::nanobench::Rng& rng, std::size_t n) {
        using bit_vector_t = cds::bit_vector<std::uint64_t, cds::pack_endian::lsb>;

        std::vector<std::uint8_t> values(n);
        for (auto& v : values)
            v = static_cast<std::uint8_t>(rng() & 1);

        std::vector<std::size_t> indices(n);
        for (auto& i : indices)
            i = static_cast<std::size_t>(rng.bounded(static_cast<std::uint32_t>(n)));

        {
            ankerl::nanobench::Bench bench;
            bench.title("constant-stride indexed write")
                .unit("element")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);

            bench.batch(n).run("push_back(unsafe) — constant stride, sequential cursor", [&] {
                bit_vector_t v;
                v.reserve(n);
                for (std::size_t i = 0; i < n; ++i)
                    v.push_back(cds::unsafe(std::uint8_t{1}));
                ankerl::nanobench::doNotOptimizeAway(v);
            });

            bench.batch(n).run("operator[]=unsafe — constant stride, pre-sized + indexed", [&] {
                bit_vector_t v;
                v.push_back(n, cds::unsafe{std::uint8_t{1}});
                for (std::size_t i = 0; i < n; ++i)
                    v[i] = cds::unsafe(std::uint8_t{1});
                ankerl::nanobench::doNotOptimizeAway(v);
            });
        }

        {

            ankerl::nanobench::Bench bench;
            bench.title("push_back").unit("element").warmup(10).relative(true);

            bench.batch(n).run("std::vector<bool> push_back", [&] {
                std::vector<bool> v;
                for (auto x : values)
                    v.push_back((bool)x);
                ankerl::nanobench::doNotOptimizeAway(v);
            });

            bench.batch(n).run("bv push_back", [&] {
                bit_vector_t v;
                for (auto x : values)
                    v.push_back(x != 0);
                ankerl::nanobench::doNotOptimizeAway(v);
            });

            bench.batch(n).run("bv push_back unsafe", [&] {
                bit_vector_t v;
                for (auto x : values)
                    v.push_back(cds::unsafe(x));
                ankerl::nanobench::doNotOptimizeAway(v);
            });

            bench.batch(n).run("std::vector<bool> push_back (reserve)", [&] {
                std::vector<bool> v;
                v.reserve(n);
                for (auto x : values)
                    v.push_back((bool)x);
                ankerl::nanobench::doNotOptimizeAway(v);
            });

            bench.batch(n).run("bv push_back (reserve)", [&] {
                bit_vector_t v;
                v.reserve(n);
                for (auto x : values)
                    v.push_back(x != 0);
                ankerl::nanobench::doNotOptimizeAway(v);
            });

            bench.batch(n).run("bv push_back unsafe (reserve)", [&] {
                bit_vector_t v;
                v.reserve(n);
                for (auto x : values)
                    v.push_back(cds::unsafe(x));
                ankerl::nanobench::doNotOptimizeAway(v);
            });
        }

        {

            ankerl::nanobench::Bench bench;
            bench.title("random_read").unit("element").warmup(10).relative(true);

            std::vector<bool> sv(values.begin(), values.end());
            bit_vector_t bv(values.begin(), values.end());

            bench.batch(n).run("std::vector<bool> random read", [&] {
                std::uint64_t sum = 0;
                for (auto i : indices)
                    sum += sv[i] ? 1u : 0u;
                ankerl::nanobench::doNotOptimizeAway(sum);
            });

            bench.batch(n).run("bit_vector random read", [&] {
                std::uint64_t sum = 0;
                for (auto i : indices)
                    sum += static_cast<std::uint8_t>(bv[i]);
                ankerl::nanobench::doNotOptimizeAway(sum);
            });

            ankerl::nanobench::Bench bench2;
            bench2.title("random_write").unit("element").warmup(10).relative(true);

            bench2.batch(n).run("std::vector<bool> random write", [&] {
                for (std::size_t k = 0; k < n; ++k)
                    sv[indices[k]] = (values[k] != 0);
                ankerl::nanobench::doNotOptimizeAway(sv);
            });

            bench2.batch(n).run("bit_vector random write", [&] {
                for (std::size_t k = 0; k < n; ++k)
                    bv[indices[k]] = values[k];
                ankerl::nanobench::doNotOptimizeAway(bv);
            });

            bench2.batch(n).run("bit_vector random write (unsafe)", [&] {
                for (std::size_t k = 0; k < n; ++k)
                    bv[indices[k]] = cds::unsafe(values[k]);
                ankerl::nanobench::doNotOptimizeAway(bv);
            });

            ankerl::nanobench::Bench bench4;
            bench4.title("random_flip").unit("element").warmup(10).relative(true);

            bench4.batch(n).run("std::vector<bool> random flip", [&] {
                for (auto i : indices)
                    sv[i].flip();
                ankerl::nanobench::doNotOptimizeAway(sv);
            });

            bench4.batch(n).run("bit_vector random flip", [&] {
                for (auto i : indices)
                    bv.flip(i);
                ankerl::nanobench::doNotOptimizeAway(bv);
            });

            ankerl::nanobench::Bench bench5;
            bench5.title("random_flip").unit("element").warmup(10).relative(true);

            bench5.batch(n).run("std::vector<bool> sequential sum (iterator)", [&] {
                std::uint64_t sum = 0;
                for (bool x : sv)
                    sum += x ? 1 : 0;
                ankerl::nanobench::doNotOptimizeAway(sum);
            });

            bench5.batch(n).run("bit_vector sequential sum (iterator)", [&] {
                std::uint64_t sum = 0;
                for (auto x : bv)
                    sum += static_cast<std::uint8_t>(x);
                ankerl::nanobench::doNotOptimizeAway(sum);
            });

            ankerl::nanobench::Bench bench6;
            bench6.title("popcount").unit("element").warmup(10).relative(true);

            bench6.batch(n).run("std::vector<bool> std::count(true)", [&] {
                const auto c = std::count(sv.begin(), sv.end(), true);
                ankerl::nanobench::doNotOptimizeAway(c);
            });

            bench6.batch(n).run("bit_vector std::count(true)", [&] {
                const auto c = std::count(bv.begin(), bv.end(), true);
                ankerl::nanobench::doNotOptimizeAway(c);
            });

            bench6.batch(n).run("bit_vector popcount", [&] {
                const auto c = bv.popcount();
                ankerl::nanobench::doNotOptimizeAway(c);
            });
        }
    }

} // namespace

int main() {
    ankerl::nanobench::Rng rng;
    constexpr std::size_t n = 1'000'000;

    run_bit_vector_benchmarks(rng, n);

    return 0;
}
