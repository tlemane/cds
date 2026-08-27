#include <doctest.h>

#include <cds/bit_dict.hpp>
#include <cds/rrr.hpp>
#include <cds/rank/poppy.hpp>
#include <cds/select/poppy.hpp>
#include <cds/select/select9.hpp>
#include <cds/io/buffer.hpp>

#include <cstddef>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

using u64 = std::uint64_t;
using bv = cds::bit_vector<u64, cds::pack_endian::lsb>;
using dict = cds::bit_dict_default<>;

using dict_poppy =
    cds::bit_dict<bv, cds::rank_poppy<bv>, cds::select_poppy<bv, cds::select_target::both>>;
using dict_sel9 = cds::bit_dict<bv, cds::rank9<bv>, cds::select9<bv, cds::select_target::both>>;

static_assert(cds::bit_dictionary<dict>);
static_assert(cds::bit_dictionary<dict_poppy>);
static_assert(cds::bit_dictionary<dict_sel9>);
static_assert(cds::bit_dictionary<cds::rrr<>>);

static bv make_bits(const std::vector<int>& pattern) {
    bv b;
    b.reserve(pattern.size());
    for (int x : pattern)
        b.push_back(x != 0);
    return b;
}

TEST_CASE("bit_dict access/rank/select vs manual") {
    const std::vector<int> p = {1, 0, 1, 1, 0, 0, 1, 0};
    dict d(make_bits(p));

    REQUIRE(d.size() == 8);
    for (std::size_t i = 0; i < p.size(); ++i)
        CHECK(d[i] == (p[i] != 0));

    CHECK(d.rank1(0) == 0);
    CHECK(d.rank1(3) == 2);
    CHECK(d.rank1(8) == 4);
    CHECK(d.rank0(8) == 4);
    CHECK(d.select1(0) == 0);
    CHECK(d.select1(3) == 6);
    CHECK(d.select0(0) == 1);
    CHECK(d.select0(3) == 7);
}

TEST_CASE("bit_dict copy rebuilds, move preserves") {
    dict a(make_bits({1, 0, 1, 1, 0, 0, 1, 0}));
    dict b = a;
    CHECK(b.select1(3) == 6);
    CHECK(b.rank1(8) == 4);
    dict c = std::move(a);
    CHECK(c.select1(3) == 6);
}

TEST_CASE("bit_dict save/load round-trip") {
    dict a(make_bits({1, 0, 1, 1, 0, 0, 1, 0}));
    cds::io::buffer_sink sink;
    REQUIRE(a.save(sink));
    auto bytes = sink.release();
    cds::io::buffer_source src(bytes);
    auto b = dict::load(src);
    REQUIRE(b.has_value());
    CHECK(b->size() == a.size());
    CHECK(b->select1(3) == 6);
    CHECK(b->rank1(8) == 4);
    CHECK(a.memory_size() > 0);
}

TEST_CASE("bit_dict as_view matches owning") {
    dict a(make_bits({1, 0, 1, 1, 0, 0, 1, 0}));
    auto v = a.as_view();
    static_assert(cds::bit_dictionary<decltype(v)>);
    CHECK(v[2] == true);
    CHECK(v.rank1(8) == 4);
    CHECK(v.select1(3) == 6);
    CHECK(v.select0(3) == 7);
}

template <typename Dict>
static void fuzz_dict(std::mt19937_64& rng, std::size_t n, unsigned one_in) {
    std::vector<int> pat(n);
    std::vector<std::size_t> ones, zeros;
    for (std::size_t i = 0; i < n; ++i) {
        const bool b = (rng() % one_in) == 0;
        pat[i] = b ? 1 : 0;
        (b ? ones : zeros).push_back(i);
    }

    Dict d(make_bits(pat));
    REQUIRE(d.size() == n);

    std::size_t acc = 0;
    for (std::size_t i = 0; i < n; ++i) {
        CHECK(d[i] == (pat[i] != 0));
        CHECK(d.rank1(i) == acc);
        CHECK(d.rank0(i) == i - acc);
        const auto rb = d.rank1_bit(i);
        CHECK(rb.rank == acc);
        CHECK(rb.bit == (pat[i] != 0));
        acc += static_cast<std::size_t>(pat[i]);
    }
    CHECK(d.rank1(n) == ones.size());

    for (std::size_t r = 0; r < ones.size(); ++r)
        CHECK(d.select1(r) == ones[r]);
    for (std::size_t r = 0; r < zeros.size(); ++r)
        CHECK(d.select0(r) == zeros[r]);
}

TEST_CASE("bit_dict shared-index backends match oracle (fuzz)") {
    std::mt19937_64 rng(0xC0FFEEULL);
    const std::vector<std::size_t> sizes = {1, 2, 63, 64, 65, 127, 1000, 4096};
    for (std::size_t n : sizes)
        for (unsigned one_in : {2u, 5u, 30u}) {
            fuzz_dict<dict_poppy>(rng, n, one_in);
            fuzz_dict<dict_sel9>(rng, n, one_in);
        }
}

TEST_CASE("bit_dict shared-index copy/move/view/save-load") {
    dict_poppy a(make_bits({1, 0, 1, 1, 0, 0, 1, 0}));

    dict_poppy b = a;
    CHECK(b.rank1(8) == 4);
    CHECK(b.select1(3) == 6);
    CHECK(b.select0(3) == 7);

    dict_poppy c = std::move(a);
    CHECK(c.select1(3) == 6);

    auto v = c.as_view();
    static_assert(cds::bit_dictionary<decltype(v)>);
    CHECK(v.rank1(8) == 4);
    CHECK(v.select0(3) == 7);

    cds::io::buffer_sink sink;
    REQUIRE(c.save(sink));
    auto bytes = sink.release();
    cds::io::buffer_source src(bytes);
    auto d = dict_poppy::load(src);
    REQUIRE(d.has_value());
    CHECK(d->size() == 8);
    CHECK(d->rank1(8) == 4);
    CHECK(d->select1(3) == 6);
}

TEST_CASE("bit_dict shared index is more compact than standalone darray") {
    std::mt19937_64 rng(1);
    std::vector<int> pat(std::size_t{1} << 16);
    for (auto& x : pat)
        x = static_cast<int>(rng() & 1u);

    dict standalone(make_bits(pat));
    dict_poppy shared(make_bits(pat));
    CHECK(shared.memory_size() < standalone.memory_size());
}
