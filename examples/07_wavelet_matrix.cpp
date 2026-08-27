// Wavelet matrix: access / rank / select over an arbitrary integer alphabet,
// built from cds bit vectors + rank/select. Compressed levels come for free by
// using rrr as the level type.

#include <cds/wavelet_matrix.hpp>
#include <cds/rrr.hpp>

#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

using u64 = std::uint64_t;

int main() {
    std::vector<u64> text = {5, 1, 7, 0, 3, 3, 6, 2, 5, 1, 3, 5};

    cds::wavelet_matrix<u64, 3> wm(std::span<const u64>{text}); // 3-bit alphabet [0,7]

    std::cout << "access(6)      = " << wm.access(6) << "  (expect 6)\n";
    std::cout << "rank(3, 12)    = " << wm.rank(3, 12) << "  (# of 3s, expect 3)\n";
    std::cout << "select(5, 1)   = " << wm.select(5, 1) << "  (2nd 5, expect 8)\n";

    // Same API, compressed levels:
    cds::wavelet_matrix<u64, 3, cds::rrr<>> comp(std::span<const u64>{text});
    std::cout << "comp rank(3,12) = " << comp.rank(3, 12) << '\n';

    return 0;
}
