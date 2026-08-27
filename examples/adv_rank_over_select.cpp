#include <cds/bit/vector.hpp>
#include <cds/rank/poppy.hpp>
#include <cds/select/poppy.hpp>
#include <cds/select/rank_backed_select.hpp>
#include <cds/ef.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

using u64 = std::uint64_t;
using bv_t = cds::bit_vector<u64, cds::pack_endian::lsb>;

int main() {
    // Compose rank_poppy + select_poppy (both-sided) into a single index built
    // straight from the bit source.
    bv_t bv;
    bv.reserve(1000);
    for (int i = 0; i < 1000; ++i)
        bv.push_back(i % 3 == 0);

    using succinct_select =
        cds::rank_backed_select<bv_t, cds::rank_poppy<bv_t>,
                                cds::select_poppy<bv_t, cds::select_target::both>>;

    succinct_select sel(bv); // builds the rank, then the select over it
    std::cout << "rank_backed_select: select1(10)=" << sel.select1(10)
              << " select0(10)=" << sel.select0(10) << " bytes=" << sel.memory_size() << '\n';

    // ef's last template parameter is the select index over its high bits. The
    // high-bits source type is exactly bit_vector<Word, Endian, Mode>, so the
    // rank_backed_select above is a drop-in replacement for the default darray.
    using succinct_ef = cds::ef<u64, cds::pack_endian::lsb, cds::pack_mode::dense,
                                /*IndexZeros=*/true, succinct_select>;

    std::vector<u64> values(200'000);
    {
        std::mt19937_64 rng(1);
        std::uniform_int_distribution<u64> dist(0, 50'000'000);
        for (auto& x : values)
            x = dist(rng);
        std::sort(values.begin(), values.end());
    }

    cds::ef<> ef_default(values); // darray<both> over the high bits
    succinct_ef ef_small(values); // poppy-backed select instead

    std::cout << "default ef : ef[4]=" << ef_default[4] << " nge(1000)={"
              << ef_default.nge(1000).pos << "," << ef_default.nge(1000).val << "}"
              << " bytes=" << ef_default.memory_size() << '\n';
    std::cout << "succinct ef: ef[4]=" << ef_small[4] << " nge(1000)={" << ef_small.nge(1000).pos
              << "," << ef_small.nge(1000).val << "}"
              << " bytes=" << ef_small.memory_size() << '\n';

    return 0;
}
