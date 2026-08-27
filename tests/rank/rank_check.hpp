#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include <doctest.h>

namespace {

    template <typename Rank>
    void check_rank_sampled(const Rank& r, const std::vector<std::size_t>& oracle_rank1,
                            std::size_t n) {
        const std::size_t count = n + 1;

        constexpr std::size_t max_middle = 128;
        const std::size_t stride = (count + max_middle - 1) / max_middle;

        auto check_one = [&](std::size_t i) {
            CAPTURE(i);
            const std::size_t expected1 = oracle_rank1[i];
            CHECK(r.rank1(i) == expected1);
            CHECK(r.rank0(i) == i - expected1);
        };

        const std::size_t edge = std::min<std::size_t>(count, 8);
        for (std::size_t i = 0; i < edge; ++i)
            check_one(i);
        for (std::size_t i = 0; i < count; i += stride)
            check_one(i);
        for (std::size_t i = count - edge; i < count; ++i)
            check_one(i);
    }

}
