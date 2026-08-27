#include <cds/bit/vector.hpp>

#include <cstdint>
#include <iostream>

using u64 = std::uint64_t;

using bv_t = cds::bit_vector<u64, cds::pack_endian::lsb>;

int main() {
    bv_t bv;

    // reserve() pre-allocates (and zero-fills) storage
    bv.reserve(1000);

    // set every third bit.
    for (int i = 0; i < 1000; ++i)
        bv.push_back(i % 3 == 0);

    // Random access. operator[] returns a proxy that reads a single bit
    std::cout << "bv.size()      = " << bv.size() << '\n';
    std::cout << "bv[0], bv[1]   = " << static_cast<int>(bv[0]) << ", " << static_cast<int>(bv[1])
              << '\n';
    std::cout << "bv[42]         = " << static_cast<int>(bv[42]) << '\n';

    // Whole-vector popcount
    std::cout << "bv.popcount()  = " << bv.popcount() << '\n';

    // Mutation through the proxy reference. Bit 1 is currently 0 (1 % 3 != 0)
    std::cout << "bv[1] (before) = " << static_cast<int>(bv[1]) << '\n';
    bv[1] = true;
    std::cout << "bv[1] (after)  = " << static_cast<int>(bv[1]) << ", popcount now "
              << bv.popcount() << '\n';

    // Iterate and count the ones
    std::size_t ones = 0;
    for (std::size_t i = 0; i < bv.size(); ++i)
        ones += bv[i] ? 1u : 0u;
    std::cout << "counted ones   = " << ones << '\n';

    return 0;
}
