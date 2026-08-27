#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <cds/bit/vector.hpp>
#include <cds/ef.hpp>
#include <cds/packed/vector.hpp>
#include <cds/rrr.hpp>
#include <cds/rank/poppy.hpp>
#include <cds/rank/rank9.hpp>
#include <cds/rank/scan.hpp>
#include <cds/select/darray.hpp>
#include <cds/select/poppy.hpp>
#include <cds/select/rank_backed_select.hpp>
#include <cds/select/scan.hpp>
#include <cds/select/select9.hpp>
#include <cds/wavelet_matrix.hpp>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <random>
#include <span>
#include <string>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <unistd.h>
#endif

#include <sdsl/int_vector.hpp>
#include <sdsl/rank_support_v.hpp>
#include <sdsl/rank_support_v5.hpp>
#include <sdsl/select_support_mcl.hpp>
#include <sdsl/rrr_vector.hpp>
#include <sdsl/sd_vector.hpp>
#include <sdsl/wm_int.hpp>
#include <sdsl/construct.hpp>
#include <sdsl/util.hpp>

namespace {

    using u64 = std::uint64_t;
    using cds::pack_endian;
    using cds::pack_mode;
    using cds::select_target;

    using ef_hb = cds::bit_vector<u64, pack_endian::lsb>;
    using ef_poppy =
        cds::ef<u64, pack_endian::lsb, pack_mode::dense, true,
                cds::rank_backed_select<ef_hb, cds::rank_poppy<ef_hb>,
                                        cds::select_poppy<ef_hb, select_target::both>>>;
    using ef_select9 = cds::ef<u64, pack_endian::lsb, pack_mode::dense, true,
                               cds::rank_backed_select<ef_hb, cds::rank9<ef_hb>,
                                                       cds::select9<ef_hb, select_target::both>>>;

    bool g_full = false;
    bool g_counters_seen = false;

    std::string g_detail;
    void pf(const char* fmt, ...) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(buf, sizeof buf, fmt, ap);
        va_end(ap);
        g_detail += buf;
    }

    struct Meas {
        double elem_per_s = 0.0;
        double ns_per_elem = 0.0;
        double ins_per_elem = 0.0;
        bool has_ins = false;
    };

    template <typename F> [[nodiscard]] Meas measure(std::size_t n, F&& fn) {
        ankerl::nanobench::Bench b;
        b.output(nullptr);
        b.warmup(1).epochs(11);
        b.minEpochTime(std::chrono::milliseconds(10));
        b.run("m", std::forward<F>(fn));

        const auto& r = b.results().front();
        using M = ankerl::nanobench::Result::Measure;
        const double nn = static_cast<double>(n > 0 ? n : 1);
        const double per_call = r.median(M::elapsed);

        Meas m;
        m.ns_per_elem = per_call / nn * 1e9;
        m.elem_per_s = (per_call > 0.0) ? nn / per_call : 0.0;
        const double ins = r.median(M::instructions);
        m.has_ins = !std::isnan(ins) && ins > 0.0;
        m.ins_per_elem = m.has_ins ? ins / nn : 0.0;
        if (m.has_ins)
            g_counters_seen = true;
        return m;
    }

    [[nodiscard]] double ratio(const Meas& a, const Meas& base) {
        return base.elem_per_s > 0 ? a.elem_per_s / base.elem_per_s : 0.0;
    }

    [[nodiscard]] std::string xstr(double v) {
        char b[16];
        std::snprintf(b, sizeof b, "%.2fx", v);
        return b;
    }

    [[nodiscard]] std::string rate(double v) {
        char buf[32];
        if (v >= 1e9)
            std::snprintf(buf, sizeof buf, "%.2f G/s", v / 1e9);
        else if (v >= 1e6)
            std::snprintf(buf, sizeof buf, "%.1f M/s", v / 1e6);
        else if (v >= 1e3)
            std::snprintf(buf, sizeof buf, "%.1f K/s", v / 1e3);
        else
            std::snprintf(buf, sizeof buf, "%.0f /s", v);
        return buf;
    }

    [[nodiscard]] std::string ins_str(const Meas& m) {
        if (!m.has_ins)
            return "-";
        char buf[24];
        std::snprintf(buf, sizeof buf, "%.1f", m.ins_per_elem);
        return buf;
    }

    [[nodiscard]] std::string ns_str(const Meas& m) {
        char buf[24];
        const double v = m.ns_per_elem;
        if (v >= 1000.0)
            std::snprintf(buf, sizeof buf, "%.0f", v);
        else if (v >= 10.0)
            std::snprintf(buf, sizeof buf, "%.1f", v);
        else
            std::snprintf(buf, sizeof buf, "%.2f", v);
        return buf;
    }

    [[nodiscard]] std::string bytes_str(double b) {
        char buf[32];
        if (b >= 1024.0 * 1024.0)
            std::snprintf(buf, sizeof buf, "%.2f MiB", b / (1024.0 * 1024.0));
        else if (b >= 1024.0)
            std::snprintf(buf, sizeof buf, "%.2f KiB", b / 1024.0);
        else
            std::snprintf(buf, sizeof buf, "%.0f B", b);
        return buf;
    }

    using OpRow = std::pair<std::string, Meas>;
    void op_table(const std::string& title, const std::vector<OpRow>& rows, double base_rate) {
        constexpr const char* F = "    %-18s %11s %8s %9s %9s\n";
        pf("  \033[2m%s\033[0m\n", title.c_str());
        pf(F, "config", "elem/s", "ns/op", "inst/ops", "vs base");
        for (const auto& [cfg, m] : rows) {
            char vs[16];
            if (base_rate > 0.0)
                std::snprintf(vs, sizeof vs, "%.2fx", m.elem_per_s / base_rate);
            else
                std::snprintf(vs, sizeof vs, "-");
            pf(F, cfg.c_str(), rate(m.elem_per_s).c_str(), ns_str(m).c_str(), ins_str(m).c_str(),
               vs);
        }
        pf("\n");
    }

    void mem_table(const std::string& title,
                   const std::vector<std::pair<std::string, double>>& rows, double base_bytes,
                   std::size_t n) {
        constexpr const char* F = "    %-18s %8s %12s %9s\n";
        pf("  \033[2m%s\033[0m\n", title.c_str());
        pf(F, "config", "bits/e", "memory", "vs base");
        for (const auto& [cfg, bytes] : rows) {
            char be[16];
            std::snprintf(be, sizeof be, "%.2f", bytes * 8.0 / static_cast<double>(n));
            char vs[16];
            std::snprintf(vs, sizeof vs, "%.2fx", base_bytes / bytes);
            pf(F, cfg.c_str(), be, bytes_str(bytes).c_str(), vs);
        }
        pf("\n");
    }

    void overhead_table(const std::string& title, double src_bytes,
                        const std::vector<std::pair<std::string, double>>& rows) {
        pf("  \033[2m%s\033[0m\n", title.c_str());
        pf("    %-18s %10s %12s\n", "impl", "overhead", "index");
        for (const auto& [nm, mem] : rows) {
            char o[24];
            std::snprintf(o, sizeof o, "%.1f%%", 100.0 * mem / src_bytes);
            pf("    %-18s %10s %12s\n", nm.c_str(), o, bytes_str(mem).c_str());
        }
        pf("\n");
    }

    [[nodiscard]] const char* endian_name(pack_endian e) {
        return e == pack_endian::lsb ? "lsb" : "msb";
    }
    [[nodiscard]] const char* mode_name(pack_mode m) {
        switch (m) {
            case pack_mode::none: return "none";
            case pack_mode::sparse: return "sparse";
            case pack_mode::dense: return "dense";
            default: return "rt";
        }
    }

    void section(const std::string& title) {
        pf("\n\033[1m== %s ==\033[0m\n\n", title.c_str());
    }

    struct PvRow {
        std::string cfg;
        Meas build, build_u, access, write, write_u, seq;
        double bytes;
    };

    template <std::uint8_t W, pack_endian E, pack_mode M>
    PvRow bench_pv(std::size_t n, const std::vector<u64>& vals,
                   const std::vector<std::size_t>& idx) {
        using vec_t = cds::packed_vector<u64, u64, W, E, M>;
        PvRow r;
        r.cfg = std::string(endian_name(E)) + "/" + mode_name(M);
        r.build = measure(n, [&] {
            vec_t v;
            v.reserve(n);
            for (std::size_t i = 0; i < n; ++i)
                v.push_back(vals[i]);
            ankerl::nanobench::doNotOptimizeAway(v.data());
        });
        if (g_full)
            r.build_u = measure(n, [&] {
                vec_t v;
                v.reserve(n);
                for (std::size_t i = 0; i < n; ++i)
                    v.push_back(cds::unsafe(vals[i]));
                ankerl::nanobench::doNotOptimizeAway(v.data());
            });
        vec_t v;
        v.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            v.push_back(vals[i]);
        r.access = measure(n, [&] {
            u64 s = 0;
            for (auto i : idx)
                s += v[i];
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        r.write = measure(n, [&] {
            for (std::size_t k = 0; k < n; ++k)
                v[idx[k]] = vals[k];
            ankerl::nanobench::doNotOptimizeAway(v.data());
        });
        if (g_full)
            r.write_u = measure(n, [&] {
                for (std::size_t k = 0; k < n; ++k)
                    v[idx[k]] = cds::unsafe(vals[k]);
                ankerl::nanobench::doNotOptimizeAway(v.data());
            });
        r.seq = measure(n, [&] {
            u64 s = 0;
            for (auto x : v)
                s += x;
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        r.bytes = static_cast<double>(v.nb_words() * sizeof(u64));
        return r;
    }

    template <std::uint8_t W> void packed_width_group(std::size_t n, const char* wlabel) {
        std::mt19937_64 rng(1);
        const u64 mask = (W >= 64) ? ~u64{0} : ((u64{1} << W) - 1);
        std::vector<u64> vals(n);
        for (auto& x : vals)
            x = rng() & mask;
        std::vector<std::size_t> idx(n);
        for (auto& i : idx)
            i = static_cast<std::size_t>(rng() % n);

        sdsl::int_vector<W> base(n);
        for (std::size_t i = 0; i < n; ++i)
            base[i] = vals[i];
        Meas b_build = measure(n, [&] {
            sdsl::int_vector<W> t(n);
            for (std::size_t i = 0; i < n; ++i)
                t[i] = vals[i];
            ankerl::nanobench::doNotOptimizeAway(std::as_const(t));
        });
        Meas b_acc = measure(n, [&] {
            u64 s = 0;
            for (auto i : idx)
                s += base[i];
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        Meas b_write = measure(n, [&] {
            for (std::size_t k = 0; k < n; ++k)
                base[idx[k]] = vals[k];
            ankerl::nanobench::doNotOptimizeAway(std::as_const(base));
        });
        Meas b_seq = measure(n, [&] {
            u64 s = 0;
            for (auto x : base)
                s += static_cast<u64>(x);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        const double base_bytes = static_cast<double>(sdsl::size_in_bytes(base));

        std::vector<PvRow> rows;
        rows.push_back(bench_pv<W, pack_endian::lsb, pack_mode::sparse>(n, vals, idx));
        rows.push_back(bench_pv<W, pack_endian::lsb, pack_mode::dense>(n, vals, idx));
        if (g_full) {
            rows.push_back(bench_pv<W, pack_endian::msb, pack_mode::sparse>(n, vals, idx));
            rows.push_back(bench_pv<W, pack_endian::msb, pack_mode::dense>(n, vals, idx));
        }

        const std::string pfx = std::string("packed_vector / ") + wlabel + " — ";
        auto emit = [&](const char* op, const Meas& base, const char* base_label, auto proj) {
            std::vector<OpRow> t;
            t.emplace_back(base_label, base);
            for (const auto& r : rows)
                t.emplace_back(r.cfg, proj(r));
            op_table(pfx + op, t, base.elem_per_s);
        };
        emit("build", b_build, "sdsl int_vec", [](const PvRow& r) { return r.build; });
        if (g_full)
            emit("build_u", b_build, "sdsl int_vec", [](const PvRow& r) { return r.build_u; });
        emit("rnd_read", b_acc, "sdsl int_vec", [](const PvRow& r) { return r.access; });
        emit("rnd_write", b_write, "sdsl int_vec", [](const PvRow& r) { return r.write; });
        if (g_full)
            emit("rnd_write_u", b_write, "sdsl int_vec", [](const PvRow& r) { return r.write_u; });
        emit("seq", b_seq, "sdsl int_vec", [](const PvRow& r) { return r.seq; });

        std::vector<std::pair<std::string, double>> mem;
        mem.emplace_back("sdsl int_vec", base_bytes);
        for (const auto& r : rows)
            mem.emplace_back(r.cfg, r.bytes);
        mem_table(pfx + "memory", mem, base_bytes, n);
    }

    void run_packed_vector(std::size_t n) {
        section(
            "packed_vector  vs  sdsl::int_vector<W>   (non-native widths so sdsl also bit-packs)");
        packed_width_group<4>(n, "W4");
        packed_width_group<12>(n, "W12");
        packed_width_group<17>(n, "W17");
        packed_width_group<20>(n, "W20");
    }

    struct BvRow {
        std::string cfg;
        Meas build, access, write, seq, pc;
        double bytes;
    };

    template <pack_endian E>
    BvRow bench_bv(std::size_t n, const std::vector<std::uint8_t>& bits,
                   const std::vector<std::size_t>& idx) {
        using vec_t = cds::bit_vector<u64, E>;
        BvRow r;
        r.cfg = std::string(endian_name(E));
        r.build = measure(n, [&] {
            vec_t v;
            v.reserve(n);
            for (std::size_t i = 0; i < n; ++i)
                v.push_back(bits[i]);
            ankerl::nanobench::doNotOptimizeAway(v.data());
        });
        vec_t v;
        v.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            v.push_back(bits[i]);
        r.access = measure(n, [&] {
            u64 s = 0;
            for (auto i : idx)
                s += static_cast<u64>(v[i]);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        r.write = measure(n, [&] {
            for (std::size_t k = 0; k < n; ++k)
                v[idx[k]] = bits[k];
            ankerl::nanobench::doNotOptimizeAway(v.data());
        });
        r.seq = measure(n, [&] {
            u64 s = 0;
            for (auto x : v)
                s += static_cast<u64>(x);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        r.pc = measure(n, [&] {
            auto c = v.popcount();
            ankerl::nanobench::doNotOptimizeAway(c);
        });
        r.bytes = static_cast<double>(v.nb_words() * sizeof(u64));
        return r;
    }

    void run_bit_vector(std::size_t n) {
        section("bit_vector  vs  sdsl::bit_vector");
        std::mt19937_64 rng(2);
        std::bernoulli_distribution dist(0.5);
        std::vector<std::uint8_t> bits(n);
        for (auto& b : bits)
            b = dist(rng) ? 1 : 0;
        std::vector<std::size_t> idx(n);
        for (auto& i : idx)
            i = static_cast<std::size_t>(rng() % n);

        Meas b_build = measure(n, [&] {
            sdsl::bit_vector t(n);
            for (std::size_t i = 0; i < n; ++i)
                t[i] = bits[i];
            ankerl::nanobench::doNotOptimizeAway(std::as_const(t));
        });
        sdsl::bit_vector base(n);
        for (std::size_t i = 0; i < n; ++i)
            base[i] = bits[i];
        Meas b_acc = measure(n, [&] {
            u64 s = 0;
            for (auto i : idx)
                s += base[i];
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        Meas b_write = measure(n, [&] {
            for (std::size_t k = 0; k < n; ++k)
                base[idx[k]] = bits[k];
            ankerl::nanobench::doNotOptimizeAway(std::as_const(base));
        });
        Meas b_seq = measure(n, [&] {
            u64 s = 0;
            for (auto x : base)
                s += static_cast<u64>(x);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        Meas b_pc = measure(n, [&] {
            const u64* d = base.data();
            const std::size_t words = (base.bit_size() + 63) / 64;
            std::size_t c = 0;
            for (std::size_t w = 0; w < words; ++w)
                c += static_cast<std::size_t>(std::popcount(d[w]));
            ankerl::nanobench::doNotOptimizeAway(c);
        });
        const double base_bytes = static_cast<double>(sdsl::size_in_bytes(base));

        std::vector<BvRow> rows;
        rows.push_back(bench_bv<pack_endian::lsb>(n, bits, idx));
        if (g_full)
            rows.push_back(bench_bv<pack_endian::msb>(n, bits, idx));

        const std::string pfx = "bit_vector — ";
        auto emit = [&](const char* op, const Meas& base, const char* base_label, auto proj) {
            std::vector<OpRow> t;
            t.emplace_back(base_label, base);
            for (const auto& r : rows)
                t.emplace_back(r.cfg, proj(r));
            op_table(pfx + op, t, base.elem_per_s);
        };
        emit("build", b_build, "sdsl bit_vec", [](const BvRow& r) { return r.build; });
        emit("rnd_read", b_acc, "sdsl bit_vec", [](const BvRow& r) { return r.access; });
        emit("rnd_write", b_write, "sdsl bit_vec", [](const BvRow& r) { return r.write; });
        emit("seq", b_seq, "sdsl bit_vec", [](const BvRow& r) { return r.seq; });
        emit("popcount", b_pc, "sdsl bit_vec", [](const BvRow& r) { return r.pc; });

        std::vector<std::pair<std::string, double>> mem;
        mem.emplace_back("sdsl bit_vec", base_bytes);
        for (const auto& r : rows)
            mem.emplace_back(r.cfg, r.bytes);
        mem_table(pfx + "memory", mem, base_bytes, n);
    }

    template <pack_endian E>
    void bench_rank_group(std::size_t n, std::size_t nq, double density, bool cached,
                          bool headline) {
        using src_t = cds::bit_vector<u64, E>;
        std::mt19937_64 rng(3);
        std::bernoulli_distribution dist(density);
        src_t v;
        v.reserve(n);
        sdsl::bit_vector sv(n);
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint8_t b = dist(rng) ? 1 : 0;
            v.push_back(b);
            sv[i] = b;
        }
        const double src_bytes = static_cast<double>(v.nb_words() * sizeof(u64));

        std::vector<std::size_t> q(nq);
        for (auto& x : q)
            x = static_cast<std::size_t>(rng() % (n + 1));

        sdsl::rank_support_v<1> sdr(&sv);
        cds::rank9<src_t> r9(v);
        cds::rank_poppy<src_t> rp(v);

        Meas sd_q = measure(nq, [&] {
            std::size_t s = 0;
            for (auto i : q)
                s += sdr(i);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        Meas r9_q = measure(nq, [&] {
            std::size_t s = 0;
            for (auto i : q)
                s += r9.rank1(i);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        Meas rp_q = measure(nq, [&] {
            std::size_t s = 0;
            for (auto i : q)
                s += rp.rank1(i);
            ankerl::nanobench::doNotOptimizeAway(s);
        });

        char grp[160];
        std::snprintf(grp, sizeof grp, "rank / %s %.0f%% dense / %s (src %s) — query (rank1)",
                      endian_name(E), density * 100.0, cached ? "cached" : "at-scale",
                      bytes_str(src_bytes).c_str());
        op_table(grp, {{"sdsl rank_v", sd_q}, {"rank9", r9_q}, {"rank_poppy", rp_q}},
                 sd_q.elem_per_s);

        overhead_table(std::string(grp).substr(0, std::string(grp).rfind(" — ")) + " — overhead",
                       src_bytes,
                       {{"sdsl rank_v", static_cast<double>(sdsl::size_in_bytes(sdr))},
                        {"rank9", static_cast<double>(r9.memory_size())},
                        {"rank_poppy", static_cast<double>(rp.memory_size())}});
    }

    void run_rank(std::size_t n) {
        section("rank  vs  sdsl::rank_support_v   (cached vs at-scale: throughput flips with "
                "working-set size)");
        const std::size_t cached = std::min<std::size_t>(n, 200'000);
        bench_rank_group<pack_endian::lsb>(cached, std::min<std::size_t>(cached, 2'000), 0.50, true,
                                           false);
        bench_rank_group<pack_endian::lsb>(n, 100'000, 0.50, false, true);
        if (g_full) {
            bench_rank_group<pack_endian::lsb>(n, 100'000, 0.05, false, false);
            bench_rank_group<pack_endian::msb>(n, 100'000, 0.50, false, false);
        }
    }

    template <pack_endian E, select_target T>
    void select_block(const std::string& group, const cds::bit_vector<u64, E>& v,
                      const sdsl::bit_vector& sv, const std::vector<std::size_t>& q1,
                      const std::vector<std::size_t>& q0, double src_bytes, std::size_t n,
                      std::size_t nq, bool headline) {
        using src_t = cds::bit_vector<u64, E>;
        constexpr bool HO = (T == select_target::ones || T == select_target::both);
        constexpr bool HZ = (T == select_target::zeros || T == select_target::both);

        cds::rank9<src_t> r9(v);
        cds::select9<src_t, T> s9(r9);
        cds::rank_poppy<src_t> rp(v);
        cds::select_poppy<src_t, T> sp(rp);
        cds::darray<src_t, T> da(v);
        sdsl::select_support_mcl<1> smcl1(&sv);
        sdsl::select_support_mcl<0> smcl0(&sv);

        Meas sd1{}, s91{}, sp1{}, da1{}, sd0{}, s90{}, sp0{}, da0{};
        if constexpr (HO) {
            sd1 = measure(nq, [&] {
                std::size_t s = 0;
                for (auto r : q1)
                    s += smcl1(r + 1);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            s91 = measure(nq, [&] {
                std::size_t s = 0;
                for (auto r : q1)
                    s += s9.select1(r);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            sp1 = measure(nq, [&] {
                std::size_t s = 0;
                for (auto r : q1)
                    s += sp.select1(r);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            da1 = measure(nq, [&] {
                std::size_t s = 0;
                for (auto r : q1)
                    s += da.select1(r);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
        }
        if constexpr (HZ) {
            sd0 = measure(nq, [&] {
                std::size_t s = 0;
                for (auto r : q0)
                    s += smcl0(r + 1);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            s90 = measure(nq, [&] {
                std::size_t s = 0;
                for (auto r : q0)
                    s += s9.select0(r);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            sp0 = measure(nq, [&] {
                std::size_t s = 0;
                for (auto r : q0)
                    s += sp.select0(r);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            da0 = measure(nq, [&] {
                std::size_t s = 0;
                for (auto r : q0)
                    s += da.select0(r);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
        }

        const char* tn = (T == select_target::ones)    ? "ones"
                         : (T == select_target::zeros) ? "zeros"
                                                       : "both";
        const double s9_mem = static_cast<double>(r9.memory_size() + s9.memory_size());
        const double sp_mem = static_cast<double>(rp.memory_size() + sp.memory_size());
        const double da_mem = static_cast<double>(da.memory_size());
        const std::string pfx = group + "target=" + tn + " — ";

        if constexpr (HO)
            op_table(pfx + "select1",
                     {{"sdsl mcl", sd1},
                      {"select9(+rank9)", s91},
                      {"select_poppy(+rp)", sp1},
                      {"darray", da1}},
                     sd1.elem_per_s);
        if constexpr (HZ)
            op_table(pfx + "select0",
                     {{"sdsl mcl", sd0},
                      {"select9(+rank9)", s90},
                      {"select_poppy(+rp)", sp0},
                      {"darray", da0}},
                     sd0.elem_per_s);

        const double sd_mem = (HO ? static_cast<double>(sdsl::size_in_bytes(smcl1)) : 0.0) +
                              (HZ ? static_cast<double>(sdsl::size_in_bytes(smcl0)) : 0.0);
        overhead_table(pfx + "overhead (incl. rank dependency for cds select9/poppy)", src_bytes,
                       {{"sdsl mcl", sd_mem},
                        {"select9(+rank9)", s9_mem},
                        {"select_poppy(+rp)", sp_mem},
                        {"darray", da_mem}});
    }

    template <pack_endian E>
    void bench_select_group(std::size_t n, std::size_t nq, double density, bool headline) {
        using src_t = cds::bit_vector<u64, E>;
        std::mt19937_64 rng(4);
        std::bernoulli_distribution dist(density);
        src_t v;
        v.reserve(n);
        sdsl::bit_vector sv(n);
        std::size_t ones = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint8_t b = dist(rng) ? 1 : 0;
            v.push_back(b);
            sv[i] = b;
            ones += b;
        }
        const std::size_t zeros = n - ones;
        if (ones == 0 || zeros == 0)
            return;
        const double src_bytes = static_cast<double>(v.nb_words() * sizeof(u64));

        std::vector<std::size_t> q1(nq), q0(nq);
        for (auto& x : q1)
            x = static_cast<std::size_t>(rng() % ones);
        for (auto& x : q0)
            x = static_cast<std::size_t>(rng() % zeros);

        char grp[128];
        std::snprintf(grp, sizeof grp, "select / %s %.0f%% (src %s) / ", endian_name(E),
                      density * 100.0, bytes_str(src_bytes).c_str());
        const std::string group = grp;
        select_block<E, select_target::both>(group, v, sv, q1, q0, src_bytes, n, nq, headline);
        if (g_full) {
            select_block<E, select_target::ones>(group, v, sv, q1, q0, src_bytes, n, nq, false);
            select_block<E, select_target::zeros>(group, v, sv, q1, q0, src_bytes, n, nq, false);
        }
    }

    void run_select(std::size_t n) {
        section("select  vs  sdsl::select_support_mcl   (target=both; index over " +
                std::to_string(n) + " bits, at-scale; overhead includes any rank dependency)");
        bench_select_group<pack_endian::lsb>(n, 100'000, 0.50, true);
        if (g_full) {
            bench_select_group<pack_endian::lsb>(n, 100'000, 0.05, false);
            bench_select_group<pack_endian::msb>(n, 100'000, 0.50, false);
        }
    }

    struct EfMeas {
        Meas build, acc, seq, nge, ple;
        double bytes = 0.0;
    };

    template <typename EfT>
    [[nodiscard]] EfMeas measure_ef(std::size_t n, std::span<const u64> vspan,
                                    const std::vector<std::size_t>& idx,
                                    const std::vector<u64>& qv) {
        EfT e(vspan);
        EfMeas m;
        m.build = measure(n, [&] {
            EfT t(vspan);
            ankerl::nanobench::doNotOptimizeAway(std::as_const(t));
        });
        m.acc = measure(n, [&] {
            u64 s = 0;
            for (auto i : idx)
                s += e[i];
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        m.seq = measure(n, [&] {
            u64 s = 0;
            for (auto x : e)
                s += x;
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        m.nge = measure(n, [&] {
            u64 s = 0;
            for (auto x : qv)
                s += e.nge(x).val;
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        m.ple = measure(n, [&] {
            u64 s = 0;
            for (auto x : qv)
                s += e.ple(x).val;
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        m.bytes = static_cast<double>(e.memory_size());
        return m;
    }

    void bench_ef_one(std::size_t n, u64 avg_gap, const char* dens, bool headline) {
        std::mt19937_64 rng(5);
        std::vector<u64> vals(n);
        {
            std::uniform_int_distribution<u64> gap(1, 2 * avg_gap);
            u64 running = 0;
            for (auto& x : vals) {
                running += gap(rng);
                x = running;
            }
        }
        const u64 universe = vals.back() + 1;

        std::vector<std::size_t> idx(n);
        for (auto& i : idx)
            i = static_cast<std::size_t>(rng() % n);
        std::vector<u64> qv(n);
        {
            std::uniform_int_distribution<u64> dist(0, universe - 1);
            for (auto& x : qv)
                x = dist(rng);
        }

        const std::span<const u64> vspan(vals);
        const EfMeas darr = measure_ef<cds::ef<>>(n, vspan, idx, qv);
        const EfMeas poppy = measure_ef<ef_poppy>(n, vspan, idx, qv);
        const EfMeas sel9 = measure_ef<ef_select9>(n, vspan, idx, qv);

        EfMeas sdslm;
        {
            auto build_sd = [&] {
                sdsl::sd_vector_builder b(universe, n);
                for (u64 vv : vals)
                    b.set(vv);
                return sdsl::sd_vector<>(b);
            };
            sdsl::sd_vector<> sd = build_sd();
            sdsl::sd_vector<>::rank_1_type rk(&sd);
            sdsl::sd_vector<>::select_1_type sl(&sd);
            sdslm.build = measure(n, [&] {
                auto s2 = build_sd();
                sdsl::sd_vector<>::rank_1_type r2(&s2);
                sdsl::sd_vector<>::select_1_type l2(&s2);
                ankerl::nanobench::doNotOptimizeAway(std::as_const(s2));
                ankerl::nanobench::doNotOptimizeAway(std::as_const(r2));
                ankerl::nanobench::doNotOptimizeAway(std::as_const(l2));
            });
            sdslm.acc = measure(n, [&] {
                u64 s = 0;
                for (auto i : idx)
                    s += sl(i + 1);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            sdslm.seq = measure(n, [&] {
                u64 s = 0;
                for (std::size_t i = 0; i < n; ++i)
                    s += sl(i + 1);
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            sdslm.nge = measure(n, [&] {
                u64 s = 0;
                for (u64 q : qv) {
                    const std::size_t r = rk(q);
                    s += (r < n) ? sl(r + 1) : vals.back();
                }
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            sdslm.ple = measure(n, [&] {
                u64 s = 0;
                for (u64 q : qv) {
                    const std::size_t r = rk(q + 1);
                    s += (r > 0) ? sl(r) : 0;
                }
                ankerl::nanobench::doNotOptimizeAway(s);
            });
            sdslm.bytes = static_cast<double>(sdsl::size_in_bytes(sd) + sdsl::size_in_bytes(rk) +
                                              sdsl::size_in_bytes(sl));
        }

        auto emit = [&](const char* op, auto proj) {
            std::vector<OpRow> rows;
            const double base = proj(sdslm).elem_per_s;
            rows.push_back({"sdsl sd_vector", proj(sdslm)});
            rows.push_back({"cds darray", proj(darr)});
            rows.push_back({"cds poppy", proj(poppy)});
            rows.push_back({"cds select9", proj(sel9)});
            char t[80];
            std::snprintf(t, sizeof t, "ef / %s (universe %.0fx n) — %s", dens,
                          static_cast<double>(universe) / static_cast<double>(n), op);
            op_table(t, rows, base);
        };
        emit("build", [](const EfMeas& m) { return m.build; });
        emit("rnd_read", [](const EfMeas& m) { return m.acc; });
        emit("nge", [](const EfMeas& m) { return m.nge; });
        emit("ple", [](const EfMeas& m) { return m.ple; });
        emit("seq", [](const EfMeas& m) { return m.seq; });

        char mt[64];
        std::snprintf(mt, sizeof mt, "ef / %s — memory", dens);
        std::vector<std::pair<std::string, double>> mrows;
        mrows.push_back({"sdsl sd_vector", sdslm.bytes});
        mrows.push_back({"cds darray", darr.bytes});
        mrows.push_back({"cds poppy", poppy.bytes});
        mrows.push_back({"cds select9", sel9.bytes});
        mem_table(mt, mrows, sdslm.bytes, n);
    }

    void run_ef(std::size_t n) {
        section(
            "ef (Elias-Fano)  vs  sdsl::sd_vector   (index variants: darray / poppy / select9)");
        bench_ef_one(n, 10, "sparse", true);
        bench_ef_one(n, 2, "dense", false);
    }

    template <std::uint16_t BS>
    void bench_rrr_one(std::size_t n, double density, const char* label, bool headline) {
        using cds_rrr = cds::rrr<BS>;
        using sdsl_rrr = sdsl::rrr_vector<BS>;
        std::mt19937_64 rng(6);
        std::bernoulli_distribution dist(density);
        cds::bit_vector<u64, pack_endian::lsb> cbv;
        cbv.reserve(n);
        sdsl::bit_vector sbv(n);
        std::size_t ones = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint8_t b = dist(rng) ? 1 : 0;
            cbv.push_back(b);
            sbv[i] = b;
            ones += b;
        }
        if (ones == 0)
            return;

        std::vector<std::size_t> idx(n), qr(n);
        for (auto& x : idx)
            x = static_cast<std::size_t>(rng() % n);
        for (auto& x : qr)
            x = static_cast<std::size_t>(rng() % ones);

        const cds_rrr cr(cbv);
        sdsl_rrr sr(sbv);
        typename sdsl_rrr::rank_1_type srank(&sr);
        typename sdsl_rrr::select_1_type sselect(&sr);

        Meas cds_build = measure(n, [&] {
            cds_rrr c(cbv);
            ankerl::nanobench::doNotOptimizeAway(c.rank1(n / 2));
        });
        Meas sd_build = measure(n, [&] {
            sdsl_rrr s(sbv);
            typename sdsl_rrr::rank_1_type rk(&s);
            typename sdsl_rrr::select_1_type sl(&s);
            ankerl::nanobench::doNotOptimizeAway(rk(n / 2));
            ankerl::nanobench::doNotOptimizeAway(sl(1));
        });
        Meas cds_acc = measure(n, [&] {
            u64 s = 0;
            for (auto i : idx)
                s += static_cast<u64>(cr[i]);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        Meas sd_acc = measure(n, [&] {
            u64 s = 0;
            for (auto i : idx)
                s += sr[i];
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        Meas cds_rk = measure(n, [&] {
            u64 s = 0;
            for (auto i : idx)
                s += cr.rank1(i);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        Meas sd_rk = measure(n, [&] {
            u64 s = 0;
            for (auto i : idx)
                s += srank(i);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        Meas cds_sel = measure(n, [&] {
            u64 s = 0;
            for (auto r : qr)
                s += cr.select1(r);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        Meas sd_sel = measure(n, [&] {
            u64 s = 0;
            for (auto r : qr)
                s += sselect(r + 1);
            ankerl::nanobench::doNotOptimizeAway(s);
        });

        const std::string pfx = std::string("rrr / ") + label + " — ";
        op_table(pfx + "build", {{"sdsl rrr_vector", sd_build}, {"cds rrr", cds_build}},
                 sd_build.elem_per_s);
        op_table(pfx + "access", {{"sdsl rrr_vector", sd_acc}, {"cds rrr", cds_acc}},
                 sd_acc.elem_per_s);
        op_table(pfx + "rank1", {{"sdsl rrr_vector", sd_rk}, {"cds rrr", cds_rk}},
                 sd_rk.elem_per_s);
        op_table(pfx + "select1", {{"sdsl rrr_vector", sd_sel}, {"cds rrr", cds_sel}},
                 sd_sel.elem_per_s);

        const double sd_bytes = static_cast<double>(
            sdsl::size_in_bytes(sr) + sdsl::size_in_bytes(srank) + sdsl::size_in_bytes(sselect));
        const double cds_bytes = static_cast<double>(cr.memory_size());
        mem_table(pfx + "memory", {{"sdsl rrr_vector", sd_bytes}, {"cds rrr", cds_bytes}}, sd_bytes,
                  n);
    }

    void run_rrr(std::size_t n) {
        section("rrr (RRR compressed bitmap)  vs  sdsl::rrr_vector   (sdsl memory includes its "
                "rank+select; raw = 1.0 bit/bit)");
        bench_rrr_one<15>(n, 0.50, "BS15 50%", false);
        bench_rrr_one<63>(n, 0.50, "BS63 50%", true);
        if (g_full) {
            bench_rrr_one<31>(n, 0.50, "BS31 50%", false);
            bench_rrr_one<15>(n, 0.05, "BS15 5%", false);
            bench_rrr_one<63>(n, 0.05, "BS63 5%", false);
        }
    }

    struct WMeas {
        Meas build, acc, rank, sel;
        double bytes = 0.0;
    };

    template <typename WM>
    [[nodiscard]] WMeas measure_wm(std::size_t n, std::size_t bits, std::size_t nq,
                                   std::span<const u64> span, const std::vector<std::size_t>& qi,
                                   const std::vector<u64>& qc, const std::vector<u64>& sc,
                                   const std::vector<std::size_t>& sr) {
        WM w(span, bits);
        WMeas m;
        m.build = measure(n, [&] {
            WM t(span, bits);
            ankerl::nanobench::doNotOptimizeAway(std::as_const(t));
        });
        m.acc = measure(nq, [&] {
            u64 s = 0;
            for (auto i : qi)
                s += w.access(i);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        m.rank = measure(nq, [&] {
            u64 s = 0;
            for (std::size_t k = 0; k < nq; ++k)
                s += w.rank(qc[k], qi[k]);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        m.sel = measure(nq, [&] {
            u64 s = 0;
            for (std::size_t k = 0; k < nq; ++k)
                s += w.select(sc[k], sr[k]);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        m.bytes = static_cast<double>(w.memory_size());
        return m;
    }

    void bench_wavelet_one(std::size_t n, std::size_t bits, const char* label, bool headline) {
        using bvv = cds::bit_vector<u64, pack_endian::lsb>;
        using lvl_darray =
            cds::bit_dict<bvv, cds::rank9<bvv>, cds::darray<bvv, select_target::both>>;
        using lvl_sel9 =
            cds::bit_dict<bvv, cds::rank9<bvv>, cds::select9<bvv, select_target::both>>;
        using lvl_poppy =
            cds::bit_dict<bvv, cds::rank_poppy<bvv>, cds::select_poppy<bvv, select_target::both>>;
        using wm_da = cds::wavelet_matrix<u64, 0, lvl_darray>;
        using wm_s9 = cds::wavelet_matrix<u64, 0, lvl_sel9>;
        using wm_pp = cds::wavelet_matrix<u64, 0, lvl_poppy>;

        std::mt19937_64 rng(7);
        const u64 sigma = u64{1} << bits;
        std::vector<u64> vals(n);
        {
            std::uniform_int_distribution<u64> d(0, sigma - 1);
            for (auto& x : vals)
                x = d(rng);
        }
        const std::span<const u64> span(vals);

        std::vector<std::size_t> cnt(static_cast<std::size_t>(sigma), 0);
        for (u64 x : vals)
            ++cnt[static_cast<std::size_t>(x)];
        const std::size_t nq = std::min<std::size_t>(n, 100'000);
        std::vector<std::size_t> qi(nq), sr(nq);
        std::vector<u64> qc(nq), sc(nq);
        for (std::size_t k = 0; k < nq; ++k) {
            qi[k] = rng() % n;
            qc[k] = rng() % sigma;
            u64 c;
            do {
                c = rng() % sigma;
            } while (cnt[static_cast<std::size_t>(c)] == 0);
            sc[k] = c;
            sr[k] = rng() % cnt[static_cast<std::size_t>(c)];
        }

        sdsl::int_vector<> iv(n, 0, static_cast<std::uint8_t>(bits));
        for (std::size_t i = 0; i < n; ++i)
            iv[i] = vals[i];
        sdsl::wm_int<> sw;
        sdsl::construct_im(sw, iv);

        Meas sd_b = measure(n, [&] {
            sdsl::wm_int<> w;
            sdsl::construct_im(w, iv);
            ankerl::nanobench::doNotOptimizeAway(sdsl::size_in_bytes(w));
        });
        Meas sd_a = measure(nq, [&] {
            u64 s = 0;
            for (auto i : qi)
                s += sw[i];
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        Meas sd_r = measure(nq, [&] {
            u64 s = 0;
            for (std::size_t k = 0; k < nq; ++k)
                s += sw.rank(qi[k], qc[k]);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        Meas sd_s = measure(nq, [&] {
            u64 s = 0;
            for (std::size_t k = 0; k < nq; ++k)
                s += sw.select(sr[k] + 1, sc[k]);
            ankerl::nanobench::doNotOptimizeAway(s);
        });
        const double sd_bytes = static_cast<double>(sdsl::size_in_bytes(sw));

        const WMeas da = measure_wm<wm_da>(n, bits, nq, span, qi, qc, sc, sr);
        const WMeas s9 = measure_wm<wm_s9>(n, bits, nq, span, qi, qc, sc, sr);
        const WMeas pp = measure_wm<wm_pp>(n, bits, nq, span, qi, qc, sc, sr);

        const std::string pfx = std::string("wavelet / ") + label + " \u2014 ";
        op_table(pfx + "build",
                 {{"sdsl wm_int", sd_b},
                  {"r9+darray", da.build},
                  {"r9+select9", s9.build},
                  {"poppy+selpop", pp.build}},
                 sd_b.elem_per_s);
        op_table(pfx + "access",
                 {{"sdsl wm_int", sd_a},
                  {"r9+darray", da.acc},
                  {"r9+select9", s9.acc},
                  {"poppy+selpop", pp.acc}},
                 sd_a.elem_per_s);
        op_table(pfx + "rank",
                 {{"sdsl wm_int", sd_r},
                  {"r9+darray", da.rank},
                  {"r9+select9", s9.rank},
                  {"poppy+selpop", pp.rank}},
                 sd_r.elem_per_s);
        op_table(pfx + "select",
                 {{"sdsl wm_int", sd_s},
                  {"r9+darray", da.sel},
                  {"r9+select9", s9.sel},
                  {"poppy+selpop", pp.sel}},
                 sd_s.elem_per_s);
        mem_table(pfx + "memory",
                  {{"sdsl wm_int", sd_bytes},
                   {"r9+darray", da.bytes},
                   {"r9+select9", s9.bytes},
                   {"poppy+selpop", pp.bytes}},
                  sd_bytes, n);
    }

    void run_wavelet(std::size_t n) {
        section("wavelet_matrix  vs  sdsl::wm_int   (level backends: r9+darray / r9+select9 / "
                "poppy+selpop; DNA 2-bit / 8 / 16)");
        bench_wavelet_one(n, 2, "2-bit (DNA)", false);
        bench_wavelet_one(n, 8, "8-bit", true);
        bench_wavelet_one(n, 16, "16-bit", false);
    }

    [[nodiscard]] std::string cpu_model() {
#if defined(__linux__)
        std::ifstream f("/proc/cpuinfo");
        std::string line;
        while (std::getline(f, line))
            if (line.rfind("model name", 0) == 0) {
                const auto p = line.find(':');
                if (p != std::string::npos)
                    return line.substr(p + 2);
            }
#endif
        return "unknown";
    }

    [[nodiscard]] std::string cache_line() {
#if defined(__linux__)
        auto kib = [](long v) { return v > 0 ? std::to_string(v / 1024) + "K" : std::string("?"); };
        return "L1d " + kib(sysconf(_SC_LEVEL1_DCACHE_SIZE)) + " / L2 " +
               kib(sysconf(_SC_LEVEL2_CACHE_SIZE)) + " / L3 " + kib(sysconf(_SC_LEVEL3_CACHE_SIZE));
#else
        return "n/a";
#endif
    }

    [[nodiscard]] std::string compiler() {
#if defined(__clang__)
        return std::string("clang ") + __clang_version__;
#elif defined(__GNUC__)
        return std::string("gcc ") + __VERSION__;
#else
        return "unknown";
#endif
    }

    void print_banner(std::size_t n) {
        std::printf("\033[1mcds-perf\033[0m — n = %zu elements%s\n", n, g_full ? " (--full)" : "");
        std::printf("  cpu     : %s\n", cpu_model().c_str());
        std::printf("  cache   : %s\n", cache_line().c_str());
        std::printf("  built   : %s, -O3 -march=native, C++23\n", compiler().c_str());
        std::printf("  vs      : sdsl-lite; each row is throughput (elem/ops per second); 'vs "
                    "base' >1.0 beats the sdsl baseline\n\n");
    }

    void usage() {
        std::printf("usage: cds-perf [SECTION ...] [--full] [n]\n\n"
                    "sections: packed  bit  rank  select  ef  rrr  wavelet  all  (default: all)\n"
                    "  --full   add extra axes (msb endian, unsafe writes, ones/zeros targets, "
                    "more densities)\n"
                    "  n        element count (default 1000000)\n"
                    "  --help   this message\n");
    }

} // namespace

int main(int argc, char** argv) {
    std::size_t n = 1'000'000;
    bool sel_packed = false, sel_bit = false, sel_rank = false, sel_select = false;
    bool sel_ef = false, sel_rrr = false, sel_wave = false, any = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "packed") {
            sel_packed = true;
            any = true;
        } else if (a == "bit") {
            sel_bit = true;
            any = true;
        } else if (a == "rank") {
            sel_rank = true;
            any = true;
        } else if (a == "select") {
            sel_select = true;
            any = true;
        } else if (a == "ef") {
            sel_ef = true;
            any = true;
        } else if (a == "rrr") {
            sel_rrr = true;
            any = true;
        } else if (a == "wavelet") {
            sel_wave = true;
            any = true;
        } else if (a == "all") {
            sel_packed = sel_bit = sel_rank = sel_select = sel_ef = sel_rrr = sel_wave = true;
            any = true;
        } else if (a == "--full") {
            g_full = true;
        } else if (a == "--help" || a == "-h") {
            usage();
            return 0;
        } else {
            const long long v = std::atoll(a.c_str());
            if (v > 0)
                n = static_cast<std::size_t>(v);
            else {
                usage();
                return 1;
            }
        }
    }
    if (!any)
        sel_packed = sel_bit = sel_rank = sel_select = sel_ef = sel_rrr = sel_wave = true;

    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    if (sel_packed)
        run_packed_vector(n);
    if (sel_bit)
        run_bit_vector(n);
    if (sel_rank)
        run_rank(n);
    if (sel_select)
        run_select(n);
    if (sel_ef)
        run_ef(n);
    if (sel_rrr)
        run_rrr(n);
    if (sel_wave)
        run_wavelet(n);

    print_banner(n);
    std::fputs(g_detail.c_str(), stdout);

    if (!g_counters_seen)
        std::printf("\nnote: hardware performance counters unavailable — ins/op columns show '-'.\n"
                    "      on Linux, allow perf access (sysctl kernel.perf_event_paranoid=1) for "
                    "instruction counts.\n");
    return 0;
}
