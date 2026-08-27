#include <nanobench.h>

#include <cds/packed/vector.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

    template <typename Word>
    void run_none_mode_group(ankerl::nanobench::Rng& rng, std::size_t n, const std::string& label) {
        constexpr auto width = static_cast<std::uint8_t>(std::numeric_limits<Word>::digits);
        using packed_t =
            cds::packed_vector<Word, Word, width, cds::pack_endian::lsb, cds::pack_mode::none>;

        std::vector<Word> values(n);
        for (auto& v : values)
            v = static_cast<Word>(rng());

        std::vector<std::size_t> indices(n);
        for (auto& i : indices)
            i = static_cast<std::size_t>(rng.bounded(static_cast<std::uint32_t>(n)));

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": fill")
                .unit("element")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);

            bench.batch(n).run(label + ": std::vector fill", [&] {
                std::vector<Word> v;
                v.reserve(n);
                for (auto x : values)
                    v.push_back(x);
                ankerl::nanobench::doNotOptimizeAway(v);
            });

            bench.batch(n).run(label + ": packed_vector(none) fill", [&] {
                packed_t v;
                v.reserve(n);
                for (auto x : values)
                    v.push_back(x);
                ankerl::nanobench::doNotOptimizeAway(v);
            });

            bench.batch(n).run(label + ": packed_vector(none) fill (unsafe)", [&] {
                packed_t v;
                v.reserve(n);
                for (auto x : values)
                    v.push_back(cds::unsafe(x));
                ankerl::nanobench::doNotOptimizeAway(v);
            });
        }

        std::vector<Word> sv(values.begin(), values.end());
        packed_t pv(values.begin(), values.end());

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": random read")
                .unit("element")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);

            bench.batch(n).run(label + ": std::vector random read", [&] {
                Word sum = 0;
                for (auto i : indices)
                    sum += sv[i];
                ankerl::nanobench::doNotOptimizeAway(sum);
            });

            bench.batch(n).run(label + ": packed_vector(none) random read", [&] {
                Word sum = 0;
                for (auto i : indices)
                    sum += static_cast<Word>(pv[i]);
                ankerl::nanobench::doNotOptimizeAway(sum);
            });
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": random write")
                .unit("element")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);

            bench.batch(n).run(label + ": std::vector random write", [&] {
                for (std::size_t k = 0; k < n; ++k)
                    sv[indices[k]] = values[k];
                ankerl::nanobench::doNotOptimizeAway(sv);
            });

            bench.batch(n).run(label + ": packed_vector(none) random write", [&] {
                for (std::size_t k = 0; k < n; ++k)
                    pv[indices[k]] = values[k];
                ankerl::nanobench::doNotOptimizeAway(pv);
            });

            bench.batch(n).run(
                label + ": packed_vector(none) random write (unsafe, INVALID USAGE - timing only)",
                [&] {
                    for (std::size_t k = 0; k < n; ++k)
                        pv[indices[k]] = cds::unsafe(values[k]);
                    ankerl::nanobench::doNotOptimizeAway(pv);
                });
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(label + ": sequential sum")
                .unit("element")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);

            bench.batch(n).run(label + ": std::vector sequential sum", [&] {
                Word sum = 0;
                for (auto x : sv)
                    sum += x;
                ankerl::nanobench::doNotOptimizeAway(sum);
            });

            bench.batch(n).run(label + ": packed_vector(none) sequential sum", [&] {
                Word sum = 0;
                for (auto x : pv)
                    sum += static_cast<Word>(x);
                ankerl::nanobench::doNotOptimizeAway(sum);
            });
        }
    }

    template <std::uint8_t Width, cds::pack_endian Endian, cds::pack_mode Mode,
              typename NaturalType>
    void run_fill_case(ankerl::nanobench::Bench& bench, std::size_t n,
                       const std::vector<NaturalType>& values, const std::string& label) {
        using packed_t = cds::packed_vector<std::uint64_t, NaturalType, Width, Endian, Mode>;
        bench.batch(n).run(label + " fill", [&] {
            packed_t v;
            v.reserve(n);
            for (auto x : values)
                v.push_back(x);
            ankerl::nanobench::doNotOptimizeAway(v);
        });
    }

    template <std::uint8_t Width, cds::pack_endian Endian, cds::pack_mode Mode,
              typename NaturalType>
    void run_fill_unsafe_case(ankerl::nanobench::Bench& bench, std::size_t n,
                              const std::vector<NaturalType>& values, const std::string& label) {
        using packed_t = cds::packed_vector<std::uint64_t, NaturalType, Width, Endian, Mode>;
        bench.batch(n).run(label + " fill (unsafe)", [&] {
            packed_t v;
            v.reserve(n);
            for (auto x : values)
                v.push_back(cds::unsafe(x));
            ankerl::nanobench::doNotOptimizeAway(v);
        });
    }

    template <std::uint8_t Width, cds::pack_endian Endian, cds::pack_mode Mode,
              typename NaturalType>
    void run_read_case(ankerl::nanobench::Bench& bench, std::size_t n,
                       const std::vector<NaturalType>& values,
                       const std::vector<std::size_t>& indices, const std::string& label) {
        using packed_t = cds::packed_vector<std::uint64_t, NaturalType, Width, Endian, Mode>;
        packed_t pv(values.begin(), values.end());
        bench.batch(n).run(label + " random read", [&] {
            std::uint64_t sum = 0;
            for (auto i : indices)
                sum += static_cast<NaturalType>(pv[i]);
            ankerl::nanobench::doNotOptimizeAway(sum);
        });
    }

    template <std::uint8_t Width, cds::pack_endian Endian, cds::pack_mode Mode,
              typename NaturalType>
    void run_write_case(ankerl::nanobench::Bench& bench, std::size_t n,
                        const std::vector<NaturalType>& values,
                        const std::vector<std::size_t>& indices, const std::string& label) {
        using packed_t = cds::packed_vector<std::uint64_t, NaturalType, Width, Endian, Mode>;
        packed_t pv(values.begin(), values.end());
        bench.batch(n).run(label + " random write", [&] {
            for (std::size_t k = 0; k < n; ++k)
                pv[indices[k]] = values[k];
            ankerl::nanobench::doNotOptimizeAway(pv);
        });
    }

    template <std::uint8_t Width, cds::pack_endian Endian, cds::pack_mode Mode,
              typename NaturalType>
    void run_write_unsafe_case(ankerl::nanobench::Bench& bench, std::size_t n,
                               const std::vector<NaturalType>& values,
                               const std::vector<std::size_t>& indices, const std::string& label) {
        using packed_t = cds::packed_vector<std::uint64_t, NaturalType, Width, Endian, Mode>;
        packed_t pv(values.begin(), values.end());
        bench.batch(n).run(label + " random write (unsafe)", [&] {
            for (std::size_t k = 0; k < n; ++k)
                pv[indices[k]] = cds::unsafe(values[k]);
            ankerl::nanobench::doNotOptimizeAway(pv);
        });
    }

    template <std::uint8_t Width, typename NaturalType>
    void run_width_group(ankerl::nanobench::Rng& rng, std::size_t n,
                         const std::string& width_label) {
        using cds::pack_endian;
        using cds::pack_mode;

        constexpr std::uint64_t mask =
            (Width >= 64) ? ~std::uint64_t{0} : ((std::uint64_t{1} << Width) - 1);

        std::vector<NaturalType> values(n);
        for (auto& v : values)
            v = static_cast<NaturalType>(rng() & mask);

        std::vector<std::size_t> indices(n);
        for (auto& i : indices)
            i = static_cast<std::size_t>(rng.bounded(static_cast<std::uint32_t>(n)));

        {
            ankerl::nanobench::Bench bench;
            bench.title(width_label + ": fill")
                .unit("element")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);

            bench.batch(n).run(width_label + ": std::vector fill", [&] {
                std::vector<NaturalType> v;
                v.reserve(n);
                for (auto x : values)
                    v.push_back(x);
                ankerl::nanobench::doNotOptimizeAway(v);
            });

            run_fill_case<Width, pack_endian::lsb, pack_mode::sparse, NaturalType>(
                bench, n, values, width_label + ": lsb/sparse");
            run_fill_unsafe_case<Width, pack_endian::lsb, pack_mode::sparse, NaturalType>(
                bench, n, values, width_label + ": lsb/sparse");
            run_fill_case<Width, pack_endian::lsb, pack_mode::dense, NaturalType>(
                bench, n, values, width_label + ": lsb/dense");
            run_fill_unsafe_case<Width, pack_endian::lsb, pack_mode::dense, NaturalType>(
                bench, n, values, width_label + ": lsb/dense");
            run_fill_case<Width, pack_endian::msb, pack_mode::sparse, NaturalType>(
                bench, n, values, width_label + ": msb/sparse");
            run_fill_unsafe_case<Width, pack_endian::msb, pack_mode::sparse, NaturalType>(
                bench, n, values, width_label + ": msb/sparse");
            run_fill_case<Width, pack_endian::msb, pack_mode::dense, NaturalType>(
                bench, n, values, width_label + ": msb/dense");
            run_fill_unsafe_case<Width, pack_endian::msb, pack_mode::dense, NaturalType>(
                bench, n, values, width_label + ": msb/dense");
        }

        std::vector<NaturalType> sv(values.begin(), values.end());

        {
            ankerl::nanobench::Bench bench;
            bench.title(width_label + ": random read")
                .unit("element")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);

            bench.batch(n).run(width_label + ": std::vector random read", [&] {
                std::uint64_t sum = 0;
                for (auto i : indices)
                    sum += sv[i];
                ankerl::nanobench::doNotOptimizeAway(sum);
            });

            run_read_case<Width, pack_endian::lsb, pack_mode::sparse, NaturalType>(
                bench, n, values, indices, width_label + ": lsb/sparse");
            run_read_case<Width, pack_endian::lsb, pack_mode::dense, NaturalType>(
                bench, n, values, indices, width_label + ": lsb/dense");
            run_read_case<Width, pack_endian::msb, pack_mode::sparse, NaturalType>(
                bench, n, values, indices, width_label + ": msb/sparse");
            run_read_case<Width, pack_endian::msb, pack_mode::dense, NaturalType>(
                bench, n, values, indices, width_label + ": msb/dense");
        }

        {
            ankerl::nanobench::Bench bench;
            bench.title(width_label + ": random write")
                .unit("element")
                .warmup(10)
                .relative(true)
                .minEpochIterations(10);

            bench.batch(n).run(width_label + ": std::vector random write", [&] {
                for (std::size_t k = 0; k < n; ++k)
                    sv[indices[k]] = values[k];
                ankerl::nanobench::doNotOptimizeAway(sv);
            });

            run_write_case<Width, pack_endian::lsb, pack_mode::sparse, NaturalType>(
                bench, n, values, indices, width_label + ": lsb/sparse");
            run_write_unsafe_case<Width, pack_endian::lsb, pack_mode::sparse, NaturalType>(
                bench, n, values, indices, width_label + ": lsb/sparse");
            run_write_case<Width, pack_endian::lsb, pack_mode::dense, NaturalType>(
                bench, n, values, indices, width_label + ": lsb/dense");
            run_write_unsafe_case<Width, pack_endian::lsb, pack_mode::dense, NaturalType>(
                bench, n, values, indices, width_label + ": lsb/dense");
            run_write_case<Width, pack_endian::msb, pack_mode::sparse, NaturalType>(
                bench, n, values, indices, width_label + ": msb/sparse");
            run_write_unsafe_case<Width, pack_endian::msb, pack_mode::sparse, NaturalType>(
                bench, n, values, indices, width_label + ": msb/sparse");
            run_write_case<Width, pack_endian::msb, pack_mode::dense, NaturalType>(
                bench, n, values, indices, width_label + ": msb/dense");
            run_write_unsafe_case<Width, pack_endian::msb, pack_mode::dense, NaturalType>(
                bench, n, values, indices, width_label + ": msb/dense");
        }
    }

} // namespace

int main() {
    ankerl::nanobench::Rng rng;
    constexpr std::size_t n = 1'000'000;

    run_none_mode_group<std::uint64_t>(rng, n, "none/W64");
    run_none_mode_group<std::uint32_t>(rng, n, "none/W32");
    run_none_mode_group<std::uint16_t>(rng, n, "none/W16");
    run_none_mode_group<std::uint8_t>(rng, n, "none/W8");

    run_width_group<32, std::uint32_t>(rng, n, "W32 (divides 64)");
    run_width_group<16, std::uint16_t>(rng, n, "W16 (divides 64)");
    run_width_group<8, std::uint8_t>(rng, n, "W8 (divides 64)");
    run_width_group<4, std::uint8_t>(rng, n, "W4 (divides 64)");

    run_width_group<12, std::uint16_t>(rng, n, "W12 (non-divisor)");
    run_width_group<20, std::uint32_t>(rng, n, "W20 (non-divisor)");
    run_width_group<24, std::uint32_t>(rng, n, "W24 (non-divisor)");
    run_width_group<40, std::uint64_t>(rng, n, "W40 (non-divisor)");

    return 0;
}
