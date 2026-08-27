#pragma once

#include <concepts>
#include <limits>
#include <utility>

#include <cds/core/debug.hpp>

#define CDS_LIKELY(x) __builtin_expect(!!(x), 1)
#define CDS_UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace cds {

    template <typename To, typename From> constexpr To sc(From&& from) noexcept {
        return static_cast<To>(std::forward<From>(from));
    }

    template <typename To, typename From> constexpr To cc(From&& from) noexcept {
        return const_cast<To>(std::forward<From>(from));
    }

    template <typename To, typename From> constexpr To rc(From&& from) noexcept {
        return reinterpret_cast<To>(std::forward<From>(from));
    }

    template <typename To, typename From> constexpr To dc(From&& from) {
        return dynamic_cast<To>(std::forward<From>(from));
    }

    template <typename To, typename From> constexpr To bc(const From& from) noexcept {
        return std::bit_cast<To>(from);
    }

    template <typename T> inline void unused(T&& v) {
        static_cast<void>(std::forward<T>(v));
    }

    template <std::integral Type> static constexpr auto digits = std::numeric_limits<Type>::digits;

    template <std::integral Type> static constexpr auto max_mask = digits<Type> - 1;

    template <std::unsigned_integral Type>
    [[nodiscard]] constexpr Type lsb_mask(std::size_t masked) noexcept {
        CDS_ASSERT(masked <= max_mask<Type>, "'masked' ({}) exceeds maximum allowed value ({})",
                   masked, max_mask<Type>);
        return Type((1ULL << masked) - 1);
    }

    template <std::unsigned_integral Type>
    [[nodiscard]] constexpr Type msb_mask(std::size_t masked) noexcept {
        CDS_ASSERT(masked <= max_mask<Type>, "'masked' ({}) exceeds maximum allowed value ({})",
                   masked, max_mask<Type>);
        return ~lsb_mask<Type>(digits<Type> - masked);
    }

}
