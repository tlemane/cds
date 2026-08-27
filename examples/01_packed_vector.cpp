#include <cds/packed/vector.hpp>
#include <cds/packed/array.hpp>

#include <cstdint>
#include <iostream>

using u64 = std::uint64_t;
using cds::pack_endian;
using cds::pack_mode;

int main() {
    // --- compile-time width ---
    // First template params is the backing word type
    // Second is a type that hold the values to pack
    cds::packed_vector<u64, u64, 12, pack_endian::lsb, pack_mode::dense> v;
    v.push_back(4095);
    v.push_back(1);
    v.push_back(2730);

    std::cout << "v.size()      = " << v.size() << '\n';
    std::cout << "v[0]          = " << v[0] << '\n';
    std::cout << "v.nb_words()  = " << v.nb_words() << " words for " << v.size()
              << " x 12-bit values\n";

    // std::vector-like API: operator[] assignment, iterators
    v[1] = 100;
    u64 sum = 0;
    for (u64 x : v)
        sum += x;
    std::cout << "sum           = " << sum << '\n';

    // --- runtime width (Width == 0) ---
    // width = 20 is passed to ctor it is then fixed for the lifetime of the vector.
    cds::packed_vector<u64, u64, 0, pack_endian::lsb, pack_mode::dense> w(/*width=*/20);
    w.push_back(1'000'000);
    std::cout << "w[0] (20-bit) = " << w[0] << '\n';

    // --- packed_array: fixed compile-time capacity, no heap allocation ---
    cds::packed_array<u64, u64, /*Capacity=*/8, /*Width=*/10, pack_endian::lsb, pack_mode::dense> a;
    a.push_back(1023);
    a.push_back(512);
    std::cout << "a[0], a[1]    = " << a[0] << ", " << a[1] << '\n';

    // --- the `unsafe` fast write path ---
    // When writing into memory known to be zero (reserve() zero-fills),
    // cds::unsafe skips the location clear (faster)
    cds::packed_vector<u64, u64, 12, pack_endian::lsb, pack_mode::dense> fast;
    fast.reserve(4);
    for (u64 x : {10u, 20u, 30u, 40u})
        fast.push_back(cds::unsafe(x));
    std::cout << "fast[2]       = " << fast[2] << " (written via cds::unsafe)\n";

    // The same intent is available as user-defined literals (_c1/_c2/_c4/_c8)
    using namespace cds::literals;
    cds::packed_vector<u64, std::uint8_t, 8, pack_endian::lsb, pack_mode::dense> lit;
    lit.reserve(1);
    lit.push_back(std::uint8_t{0});
    lit[0] = 200_c1;
    std::cout << "lit[0]        = " << static_cast<int>(lit[0]) << '\n';

    return 0;
}
