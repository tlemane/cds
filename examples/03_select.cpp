#include <cds/bit/vector.hpp>
#include <cds/rank/rank9.hpp>
#include <cds/rank/poppy.hpp>
#include <cds/select/darray.hpp>
#include <cds/select/select9.hpp>
#include <cds/select/poppy.hpp>
#include <cds/select/scan.hpp>

#include <cstdint>
#include <iostream>

using u64 = std::uint64_t;
using bv_t = cds::bit_vector<u64, cds::pack_endian::lsb>;
using cds::select_target;

int main() {
    bv_t bv;
    bv.reserve(1000);
    for (int i = 0; i < 1000; ++i)
        bv.push_back(i % 3 == 0);

    // --- darray ---
    // `both` so we can query zeros and ones.
    cds::darray<bv_t, select_target::both> d(bv);
    std::cout << "darray select1(10) = " << d.select1(10)     // 10th one  -> 30
              << ", select0(10) = " << d.select0(10) << '\n'; // 10th zero

    // --- select9: backed by rank9 ---
    cds::rank9<bv_t> r9(bv);
    cds::select9<bv_t, select_target::ones> s9(r9);
    std::cout << "select9 select1(10) = " << s9.select1(10) << '\n';

    // --- select_poppy: backed by rank_poppy ---
    cds::rank_poppy<bv_t> rp(bv);
    cds::select_poppy<bv_t, select_target::ones> sp(rp);
    std::cout << "select_poppy select1(10) = " << sp.select1(10) << '\n';

    // --- select_scan: no index ---
    cds::select_scan<bv_t> ss(bv);
    std::cout << "select_scan select1(10) = " << ss.select1(10) << '\n';

    return 0;
}
