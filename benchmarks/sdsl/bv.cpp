
#include <nanobench.h>

#include <cds/bit/vector.hpp>

#include <sdsl/int_vector.hpp>

#include <bit>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

    using u64 = std::uint64_t;

    template <typename CdsT>
    void add_cds_build(ankerl::nanobench::Bench& bench, std::size_t n,
                       const std::vector<std::uint8_t>& bits, const char* name) {
        bench.batch(n).run(name, [&] {
            CdsT v;
            v.reserve(n);
            for (std::size_t i = 0; i < n; ++i)
                v.push_back(bits[i]);
            ankerl::nanobench::doNotOptimizeAway(v.data());
        });
    }

    template <typename CdsT>
    [[nodiscard]] CdsT make_filled(std::size_t n, const std::vector<std::uint8_t>& bits) {
        CdsT v;
        v.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            v.push_back(bits[i]);
        return v;
    }

    template <typename CdsT>
    void add_cds_read(ankerl::nanobench::Bench& bench, std::size_t n,
                      const std::vector<std::uint8_t>& bits, const std::vector<std::size_t>& idx,
                      const char* name) {
        CdsT v = make_filled<CdsT>(n, bits);
        bench.batch(n).run(name, [&] {
            u64 s = 0;
            for (std::size_t k = 0; k < n; ++k)
                s += static_cast<u64>(v[idx[k]]);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
    }

    template <typename CdsT, bool Unsafe>
    void add_cds_write(ankerl::nanobench::Bench& bench, std::size_t n,
                       const std::vector<std::uint8_t>& bits, const std::vector<std::size_t>& idx,
                       const char* name) {
        CdsT v = make_filled<CdsT>(n, bits);
        bench.batch(n).run(name, [&] {
            for (std::size_t k = 0; k < n; ++k) {
                if constexpr (Unsafe)
                    v[idx[k]] = cds::unsafe(bits[k]);
                else
                    v[idx[k]] = bits[k];
            }
            ankerl::nanobench::doNotOptimizeAway(v.data());
        });
    }

    template <typename CdsT>
    void add_cds_seq(ankerl::nanobench::Bench& bench, std::size_t n,
                     const std::vector<std::uint8_t>& bits, const char* name) {
        CdsT v = make_filled<CdsT>(n, bits);
        bench.batch(n).run(name, [&] {
            u64 s = 0;
            for (auto x : v)
                s += static_cast<u64>(x);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
    }

    template <typename CdsT>
    void add_cds_popcount(ankerl::nanobench::Bench& bench, std::size_t n,
                          const std::vector<std::uint8_t>& bits, const char* name) {
        CdsT v = make_filled<CdsT>(n, bits);
        bench.batch(n).run(name, [&] {
            auto c = v.popcount();
            ankerl::nanobench::doNotOptimizeAway(c);
        });
    }

    [[nodiscard]] sdsl::bit_vector make_sdsl(std::size_t n, const std::vector<std::uint8_t>& bits) {
        sdsl::bit_vector v(n);
        for (std::size_t i = 0; i < n; ++i)
            v[i] = bits[i];
        return v;
    }

    void run_bit_vector(ankerl::nanobench::Rng& rng, std::size_t n) {
        using cds_lsb = cds::bit_vector<u64, cds::pack_endian::lsb>;
        using cds_msb = cds::bit_vector<u64, cds::pack_endian::msb>;

        std::vector<std::uint8_t> bits(n);
        for (auto& b : bits)
            b = static_cast<std::uint8_t>(rng() & 1);

        std::vector<std::size_t> idx(n);
        for (auto& i : idx)
            i = static_cast<std::size_t>(rng.bounded(static_cast<std::uint32_t>(n)));

        {
            ankerl::nanobench::Bench bench;
            bench.title("bit_vector: build")
                .unit("bit")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);
            bench.batch(n).run("sdsl", [&] {
                sdsl::bit_vector v(n);
                for (std::size_t i = 0; i < n; ++i)
                    v[i] = bits[i];
                ankerl::nanobench::doNotOptimizeAway(std::as_const(v));
            });
            add_cds_build<cds_lsb>(bench, n, bits, "cds lsb");
            add_cds_build<cds_msb>(bench, n, bits, "cds msb");
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title("bit_vector: random read")
                .unit("bit")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);
            sdsl::bit_vector sv = make_sdsl(n, bits);
            bench.batch(n).run("sdsl", [&] {
                u64 s = 0;
                for (std::size_t k = 0; k < n; ++k)
                    s += sv[idx[k]];
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            add_cds_read<cds_lsb>(bench, n, bits, idx, "cds lsb");
            add_cds_read<cds_msb>(bench, n, bits, idx, "cds msb");
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title("bit_vector: random write")
                .unit("bit")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);
            sdsl::bit_vector sv = make_sdsl(n, bits);
            bench.batch(n).run("sdsl", [&] {
                for (std::size_t k = 0; k < n; ++k)
                    sv[idx[k]] = bits[k];
                ankerl::nanobench::doNotOptimizeAway(std::as_const(sv));
            });
            add_cds_write<cds_lsb, false>(bench, n, bits, idx, "cds lsb");
            add_cds_write<cds_lsb, true>(bench, n, bits, idx, "cds lsb (u)");
            add_cds_write<cds_msb, false>(bench, n, bits, idx, "cds msb");
            add_cds_write<cds_msb, true>(bench, n, bits, idx, "cds msb (u)");
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title("bit_vector: sequential sum")
                .unit("bit")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);
            sdsl::bit_vector sv = make_sdsl(n, bits);
            bench.batch(n).run("sdsl", [&] {
                u64 s = 0;
                for (auto x : sv)
                    s += static_cast<u64>(x);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            add_cds_seq<cds_lsb>(bench, n, bits, "cds lsb");
            add_cds_seq<cds_msb>(bench, n, bits, "cds msb");
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title("bit_vector: popcount")
                .unit("bit")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);
            sdsl::bit_vector sv = make_sdsl(n, bits);
            bench.batch(n).run("sdsl", [&] {
                const u64* d = sv.data();
                const std::size_t words = (sv.bit_size() + 63) / 64;
                std::size_t c = 0;
                for (std::size_t w = 0; w < words; ++w)
                    c += static_cast<std::size_t>(std::popcount(d[w]));
                ankerl::nanobench::doNotOptimizeAway(c);
            });
            add_cds_popcount<cds_lsb>(bench, n, bits, "cds lsb");
            add_cds_popcount<cds_msb>(bench, n, bits, "cds msb");
        }
    }

} // namespace

int main() {
    ankerl::nanobench::Rng rng;
    constexpr std::size_t n = 1'000'000;

    run_bit_vector(rng, n);

    return 0;
}
