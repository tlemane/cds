#pragma once

#include <limits>
#include <cstdint>
#include <concepts>

namespace cds {

    enum class pack_endian : std::uint8_t { msb, lsb, rt };

    enum class pack_mode : std::uint8_t { none, sparse, dense, rt };

    template <std::unsigned_integral Word>
    inline constexpr std::size_t to_nb_words(std::size_t n, std::size_t width,
                                             pack_mode m) noexcept {
        constexpr auto d = std::numeric_limits<Word>::digits;
        switch (m) {
            case pack_mode::none: return n;
            case pack_mode::dense: return ((n * width) + d - 1) / d;
            case pack_mode::sparse: {
                auto e = d / width;
                return e ? (n + (e - 1)) / e : n;
            }
            default: return 0;
        }
    }

    template <std::unsigned_integral Word>
    inline constexpr std::size_t to_capacity(std::size_t n, std::size_t width,
                                             pack_mode m) noexcept {
        constexpr auto d = std::numeric_limits<Word>::digits;
        switch (m) {
            case pack_mode::none: return n;
            case pack_mode::sparse: return n * (d / width);
            case pack_mode::dense: return (n * d) / width;
            default: return 0;
        }
    }

    template <auto Value> struct static_value {
        constexpr static_value(auto) noexcept {}
        constexpr auto get() const noexcept {
            return Value;
        }
    };

    template <typename T> struct runtime_value {
        T value;
        constexpr runtime_value(T v) noexcept : value(v) {}

        constexpr T get() const noexcept {
            return value;
        }
    };

    template <typename T> struct unsafe {
        T value;

        constexpr explicit unsafe(T v) noexcept : value(v) {}
        [[nodiscard]] constexpr explicit operator T() const noexcept {
            return value;
        }
    };

    template <typename T> unsafe(T) -> unsafe<T>;

    template <typename T>
        requires std::unsigned_integral<T>
    struct packed_traits {
        using value_type = T;
        using packed_type = T;

        static constexpr packed_type to(value_type v) noexcept {
            return v;
        }

        static constexpr value_type from(packed_type v) noexcept {
            return v;
        }
    };

} // namespace cds

namespace cds::literals {

    [[nodiscard]] constexpr unsafe<std::uint8_t> operator""_c1(unsigned long long value) noexcept {
        return unsafe<std::uint8_t>(static_cast<std::uint8_t>(value));
    }

    [[nodiscard]] constexpr unsafe<std::uint16_t> operator""_c2(unsigned long long value) noexcept {
        return unsafe<std::uint16_t>(static_cast<std::uint16_t>(value));
    }

    [[nodiscard]] constexpr unsafe<std::uint32_t> operator""_c4(unsigned long long value) noexcept {
        return unsafe<std::uint32_t>(static_cast<std::uint32_t>(value));
    }

    [[nodiscard]] constexpr unsafe<std::uint64_t> operator""_c8(unsigned long long value) noexcept {
        return unsafe<std::uint64_t>(static_cast<std::uint64_t>(value));
    }

} // namespace cds::literals
