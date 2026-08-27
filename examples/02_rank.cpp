#include <cds/bit/vector.hpp>
#include <cds/rank/rank9.hpp>
#include <cds/rank/poppy.hpp>
#include <cds/rank/scan.hpp>

#include <cstdint>
#include <iostream>

using u64 = std::uint64_t;
using bv_t = cds::bit_vector<u64, cds::pack_endian::lsb>;

int main() {
    // A bit vector with a 1 at every 3rd position over [0, 1000).
    bv_t bv;
    bv.reserve(1000);
    for (int i = 0; i < 1000; ++i)
        bv.push_back(i % 3 == 0);

    // Each index binds to `bv`
    // `bv` must stay alive while the index is used.
    cds::rank9<bv_t> r9(bv);
    cds::rank_poppy<bv_t> rp(bv);
    cds::rank_scan<bv_t> rs(bv);

    std::cout << "rank1(300): "
              << "rank9=" << r9.rank1(300) << " poppy=" << rp.rank1(300)
              << " scan=" << rs.rank1(300) << '\n'; // 100 ones in [0,300)

    std::cout << "rank0(300) = " << r9.rank0(300) << '\n'; // 300 - 100 = 200

    // rank1(n) is the total popcount.
    std::cout << "rank1(size) = " << r9.rank1(bv.size()) << " (== popcount " << bv.popcount()
              << ")\n";

    // The size/speed trade-off shows up in memory_size().
    std::cout << "index bytes: rank9=" << r9.memory_size() << " poppy=" << rp.memory_size()
              << " (raw bits = " << bv.size() << ")\n";

    return 0;
}
