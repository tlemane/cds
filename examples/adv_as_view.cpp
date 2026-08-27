#include <cds/bit/vector.hpp>
#include <cds/rrr.hpp>
#include <cds/rank/interface.hpp>
#include <cds/select/interface.hpp>

#include <cstdint>
#include <iostream>

using u64 = std::uint64_t;

void call_rank(cds::rank_interface* ri) {
    std::cout << "rank1(5000)   = " << ri->rank1(5000) << '\n';
}

void call_select(cds::select_interface* si) {
    std::cout << "select1(10)   = " << si->select1(10) << '\n';
    std::cout << "select0(10)   = " << si->select0(10) << '\n';
}
int main() {
    cds::bit_vector<u64, cds::pack_endian::lsb> bv;
    bv.reserve(10000);
    for (int i = 0; i < 10000; ++i)
        bv.push_back(i % 5 == 0);
    cds::rrr<> backend(bv);

    auto rnk = cds::rank_adapter(backend.as_view());
    auto sel = cds::select_adapter(backend.as_view());

    call_rank(&rnk);
    call_select(&sel);

    return 0;
}
