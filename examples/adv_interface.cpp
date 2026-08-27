#include <cds/bit/vector.hpp>
#include <cds/rank/rank9.hpp>
#include <cds/rank/interface.hpp>
#include <cds/rrr.hpp>

#include <cstdint>
#include <iostream>
#include <memory>

using u64 = std::uint64_t;
using bv_t = cds::bit_vector<u64, cds::pack_endian::lsb>;

// Pick a rank implementation at runtime, returned behind a single pointer type.
std::unique_ptr<cds::rank_interface> make_rank(bool compressed, const bv_t& bv) {
    if (compressed)
        return std::make_unique<cds::rank_adapter<cds::rrr<>>>(bv);
    return std::make_unique<cds::rank_adapter<cds::rank9<bv_t>>>(bv);
}

int main() {
    bv_t bv;
    bv.reserve(1000);
    for (int i = 0; i < 1000; ++i)
        bv.push_back(i % 3 == 0);

    for (bool compressed : {false, true}) {
        std::unique_ptr<cds::rank_interface> idx = make_rank(compressed, bv);

        // One virtual call per query, dispatched to whichever impl was chosen.
        const std::size_t r = idx->rank1(300);
        std::cout << (compressed ? "rrr   " : "rank9 ") << "-> rank1(300) = " << r
                  << ", size() = " << idx->size() << '\n';
    }

    std::vector<std::unique_ptr<cds::rank_interface>> indexes;
    indexes.push_back(std::make_unique<cds::rank_adapter<cds::rank9<bv_t>>>(bv));
    indexes.push_back(std::make_unique<cds::rank_adapter<cds::rrr<>>>(bv));
    std::cout << "collection rank1(600): ";
    for (const auto& ix : indexes)
        std::cout << ix->rank1(600) << ' ';
    std::cout << '\n';

    return 0;
}
