#pragma once

#include <cds/core/packed/type.hpp>
#include <cds/io/byte.hpp>

#include <cstdint>
#include <optional>
#include <tuple>

namespace cds::detail {

    struct packed_save_header {
        std::uint32_t magic; // shared across packed_vector/packed_array
        std::uint8_t width;
        std::uint8_t endian;
        std::uint8_t mode;
        std::uint8_t reserved{0};
        std::uint64_t size; // element count
    };
    static_assert(io::mmap_aligned_header<packed_save_header>);

    inline constexpr std::uint32_t packed_save_magic = io::cds_magic(io::format_id::packed);

    template <std::uint8_t Width, pack_endian Endian, pack_mode Mode>
    [[nodiscard]] constexpr std::optional<std::tuple<std::uint8_t, pack_endian, pack_mode>>
    resolve_packing_params(const packed_save_header& h) noexcept {
        if constexpr (Width != 0) {
            if (h.width != Width)
                return std::nullopt;
        }
        if constexpr (Endian != pack_endian::rt) {
            if (static_cast<pack_endian>(h.endian) != Endian)
                return std::nullopt;
        }
        if constexpr (Mode != pack_mode::rt) {
            if (static_cast<pack_mode>(h.mode) != Mode)
                return std::nullopt;
        }

        const std::uint8_t width = (Width != 0) ? Width : h.width;
        const pack_endian endian =
            (Endian != pack_endian::rt) ? Endian : static_cast<pack_endian>(h.endian);
        const pack_mode mode = (Mode != pack_mode::rt) ? Mode : static_cast<pack_mode>(h.mode);

        return std::make_tuple(width, endian, mode);
    }

}
