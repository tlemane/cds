#pragma once

#include <utility>
#include <cds/core/attributes.hpp>
#include <cds/core/packed/type.hpp>

namespace cds {

    template <std::uint8_t Width, pack_endian Endian, pack_mode Mode> struct packing_storage {
    private:
        using width_storage =
            std::conditional_t<Width == 0, runtime_value<std::uint8_t>, static_value<Width>>;

        using type_storage = std::conditional_t<Endian == pack_endian::rt,
                                                runtime_value<pack_endian>, static_value<Endian>>;

        using mode_storage =
            std::conditional_t<Mode == pack_mode::rt, runtime_value<pack_mode>, static_value<Mode>>;

        CDS_NO_UNIQUE_ADDRESS width_storage m_width;
        CDS_NO_UNIQUE_ADDRESS type_storage m_endian;
        CDS_NO_UNIQUE_ADDRESS mode_storage m_mode;

    public:
        constexpr packing_storage(std::uint8_t width = Width, pack_endian endian = Endian,
                                  pack_mode mode = Mode) noexcept
            : m_width(width), m_endian(endian), m_mode(mode) {}

        constexpr auto width() const noexcept {
            return m_width.get();
        }

        constexpr auto endian() const noexcept {
            return m_endian.get();
        }

        constexpr auto mode() const noexcept {
            return m_mode.get();
        }

        template <typename F> constexpr decltype(auto) visit(F&& f) const noexcept {
            if constexpr (Width == 0) {
                if constexpr (Endian == pack_endian::rt) {
                    if constexpr (Mode == pack_mode::rt) {
                        return std::forward<F>(f)(width(), endian(), mode());
                    } else {
                        return std::forward<F>(f)(width(), endian());
                    }
                } else if constexpr (Mode == pack_mode::rt) {
                    return std::forward<F>(f)(width(), mode());
                } else {
                    return std::forward<F>(f)(width());
                }
            } else if constexpr (Endian == pack_endian::rt) {
                if constexpr (Mode == pack_mode::rt) {
                    return std::forward<F>(f)(endian(), mode());
                } else {
                    return std::forward<F>(f)(endian());
                }
            } else if constexpr (Mode == pack_mode::rt) {
                return std::forward<F>(f)(mode());
            } else {
                return std::forward<F>(f)();
            }
        }
    };

} // namespace cds
