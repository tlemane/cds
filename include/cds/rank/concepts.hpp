#pragma once

#include <concepts>
#include <cstddef>
#include <utility>

namespace cds {

    struct rank_bit {
        std::size_t rank;
        bool bit;
    };

    template <typename T>
    concept rank1_structure = requires(const T& t, std::size_t i) {
        { t.rank1(i) } -> std::convertible_to<std::size_t>;
        { t.size() } -> std::convertible_to<std::size_t>;
    };

    template <typename T>
    concept rank0_structure = requires(const T& t, std::size_t i) {
        { t.rank0(i) } -> std::convertible_to<std::size_t>;
        { t.size() } -> std::convertible_to<std::size_t>;
    };

    template <typename T>
    concept rank_structure = requires(const T& t, std::size_t i) {
        { t.rank1(i) } -> std::convertible_to<std::size_t>;
        { t.rank0(i) } -> std::convertible_to<std::size_t>;
        { t.size() } -> std::convertible_to<std::size_t>;
    };

} // namespace cds
