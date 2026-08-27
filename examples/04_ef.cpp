#include <cds/ef.hpp>

#include <cstdint>
#include <iostream>
#include <vector>

using u64 = std::uint64_t;

int main() {
    // The input must be sorted (non-decreasing)
    // duplicates are allowed.
    std::vector<u64> values = {5, 8, 8, 20, 50, 51, 90, 200, 512};

    // universe is inferred as back() + 1 unless you pass it explicitly.
    cds::ef<> ef(values);

    std::cout << "size()     = " << ef.size() << '\n';     // 9
    std::cout << "universe() = " << ef.universe() << '\n'; // 513
    std::cout << "back()     = " << ef.back() << '\n';     // 512

    // Random access: the i-th stored value.
    std::cout << "ef[4]      = " << ef[4] << " (expect 50)\n";

    // rank(k): how many values are < k.
    std::cout << "rank(30)   = " << ef.rank(30) << " (values < 30, expect 4)\n";

    // Successor / predecessor. Each returns {pos, val}.
    auto nge = ef.nge(30); // leftmost value >= 30
    auto ple = ef.ple(30); // rightmost value <= 30
    std::cout << "nge(30)    = {pos=" << nge.pos << ", val=" << nge.val << "} (expect 50)\n";
    std::cout << "ple(30)    = {pos=" << ple.pos << ", val=" << ple.val << "} (expect 20)\n";

    // locate(x): the (predecessor, successor) pair bracketing x.
    auto [lo, hi] = ef.locate(30);
    std::cout << "locate(30) = ({" << lo.pos << "," << lo.val << "}, {" << hi.pos << "," << hi.val
              << "})\n";

    // diff(i): values[i+1] - values[i] (the i-th gap).
    std::cout << "diff(3)    = " << ef.diff(3) << " (values[4]-values[3] = 50-20 = 30)\n";

    // Fast sequential decode
    std::cout << "decoded:  ";
    for (u64 x : ef)
        std::cout << x << ' ';
    std::cout << '\n';

    // from_deltas: build the prefix-sum sequence directly from a list of gaps.
    // Result has size deltas.size()+1 with a leading implicit 0.
    std::vector<u64> deltas = {5, 3, 0, 12, 30};
    auto e2 = cds::ef<>::from_deltas(deltas); // -> 0, 5, 8, 8, 20, 50
    std::cout << "from_deltas: ";
    for (u64 x : e2)
        std::cout << x << ' ';
    std::cout << '\n';

    return 0;
}
