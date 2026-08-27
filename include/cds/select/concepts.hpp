#pragma once

#include <concepts>

namespace cds {
    enum class select_target { ones, zeros, both };

    namespace detail {
        struct empty_storage {};
    } // namespace detail

    template <typename T>
    concept select1_structure = requires(const T& t, std::size_t r) {
        { t.select1(r) } -> std::convertible_to<std::size_t>;
    };

    template <typename T>
    concept select0_structure = requires(const T& t, std::size_t r) {
        { t.select0(r) } -> std::convertible_to<std::size_t>;
    };

    template <typename T>
    concept select_structure = requires(const T& t, std::size_t r) {
        { t.select0(r) } -> std::convertible_to<std::size_t>;
        { t.select1(r) } -> std::convertible_to<std::size_t>;
    };

} // namespace cds
