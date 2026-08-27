#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include <doctest.h>

namespace {

    template <typename SelectFn>
    void check_select_sampled(SelectFn select_fn, const std::vector<std::size_t>& positions) {
        const std::size_t count = positions.size();
        if (count == 0)
            return;

        constexpr std::size_t max_middle = 128;
        const std::size_t stride = (count + max_middle - 1) / max_middle;

        auto check_one = [&](std::size_t r) {
            CAPTURE(r);
            CHECK(select_fn(r) == positions[r]);
        };

        const std::size_t edge = std::min<std::size_t>(count, 8);
        for (std::size_t r = 0; r < edge; ++r)
            check_one(r);
        for (std::size_t r = 0; r < count; r += stride)
            check_one(r);
        for (std::size_t r = count - edge; r < count; ++r)
            check_one(r);
    }

} // namespace
