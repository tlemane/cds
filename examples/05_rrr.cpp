#include <cds/bit/vector.hpp>
#include <cds/rrr.hpp>

#include <cstdint>
#include <iostream>
#include <random>

using u64 = std::uint64_t;
using bv_t = cds::bit_vector<u64, cds::pack_endian::lsb>;

int main() {
    // A sparse bit vector (~5% ones)
    constexpr std::size_t n = 100'000;
    bv_t bv;
    bv.reserve(n);
    std::mt19937_64 rng(42);
    std::bernoulli_distribution coin(0.05);
    std::size_t ones = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const bool b = coin(rng);
        bv.push_back(b);
        ones += b;
    }

    // Default template args: BlockSize = 63, SampleRate = 32. rrr COPIES the
    // bits into its compressed form, so it does not depend on `bv` afterwards.
    cds::rrr<> r(bv);

    std::cout << "size()      = " << r.size() << ", ones = " << ones << '\n';
    std::cout << "r[7]        = " << r[7] << '\n';
    std::cout << "rank1(50000)= " << r.rank1(50'000) << '\n';
    std::cout << "rank0(50000)= " << r.rank0(50'000) << '\n';
    std::cout << "select1(10) = " << r.select1(10) << " (position of 11th one)\n";
    std::cout << "select0(10) = " << r.select0(10) << " (position of 11th zero)\n";

    // Compression: RRR's footprint vs a raw uncompressed bit array (n/8 bytes).
    std::cout << "rrr bytes   = " << r.memory_size() << " vs raw " << (n / 8) << " bytes\n";

    // BlockSize is a template parameter trading space against speed: larger
    // blocks compress a little better but decode a little slower.
    cds::rrr<31> r31(bv);
    cds::rrr<15> r15(bv);
    std::cout << "bytes by block size: <63>=" << r.memory_size() << " <31>=" << r31.memory_size()
              << " <15>=" << r15.memory_size() << '\n';

    return 0;
}
