#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <type_traits>
#include <vector>

#include <cds/version.hpp>

namespace cds::io {

    // Error returned by load() paths.
    enum class load_error {
        io_failure,        // a read()/skip() failed (truncated stream, etc.)
        bad_magic,         // header magic did not match
        bad_version,       // saved major version unsupported by this build
        type_mismatch,     // saved packing parameters differ from the target type
        capacity_exceeded, // saved element count exceeds a fixed-capacity target
        size_mismatch,     // saved data was built for a source of a different size
    };

    template <typename S>
    concept byte_sink = requires(S& s, const void* data, std::size_t n) {
        { s.write(data, n) } -> std::same_as<bool>;
    };

    template <typename S>
    concept byte_source = requires(S& s, void* data, std::size_t n) {
        { s.read(data, n) } -> std::same_as<bool>;
        { s.skip(n) } -> std::same_as<bool>;
    };

    template <typename S>
    concept span_source = byte_source<S> && requires(S& s, std::size_t n) {
        { s.view(n) } -> std::same_as<std::span<const std::byte>>;
    };

    template <typename S>
    concept mutable_span_source = span_source<S> && requires(S& s, std::size_t n) {
        { s.view_mut(n) } -> std::same_as<std::span<std::byte>>;
    };

    template <typename Header>
    concept mmap_aligned_header = (sizeof(Header) % 8 == 0);

    // Serialized-format magic
    // the ASCII bytes 'C', 'D', 'S' followed by a one-byte format id
    [[nodiscard]] constexpr std::uint32_t cds_magic(std::uint8_t format) noexcept {
        return static_cast<std::uint32_t>('C') | (static_cast<std::uint32_t>('D') << 8) |
               (static_cast<std::uint32_t>('S') << 16) | (static_cast<std::uint32_t>(format) << 24);
    }

    // Registry of serialized formats
    enum class format_id : std::uint8_t {
        packed = 0,
        rank9 = 1,
        rank_poppy = 2,
        select9 = 3,
        select_poppy = 4,
        darray = 5,
        ef = 6,
        rrr = 7,
        wavelet = 8,
    };

    [[nodiscard]] constexpr std::uint32_t cds_magic(format_id id) noexcept {
        return cds_magic(static_cast<std::uint8_t>(id));
    }

    struct cds_version_header {
        std::uint16_t major;
        std::uint16_t minor;
        std::uint16_t patch;
        std::uint16_t reserved{0};
    };
    static_assert(mmap_aligned_header<cds_version_header>);

    template <byte_sink Sink> [[nodiscard]] bool write_cds_version(Sink& sink) noexcept {
        const cds_version_header v{CDS_VERSION_MAJOR, CDS_VERSION_MINOR, CDS_VERSION_PATCH};
        return sink.write(&v, sizeof(v));
    }

    template <byte_source Source>
    [[nodiscard]] std::expected<cds_version_header, load_error>
    read_cds_version_compatible(Source& source) noexcept {
        cds_version_header v{};
        if (!source.read(&v, sizeof(v)))
            return std::unexpected(load_error::io_failure);
        if (v.major != CDS_VERSION_MAJOR)
            return std::unexpected(load_error::bad_version);
        return v;
    }

    struct format_header {
        std::uint32_t magic;
        std::uint32_t version;
    };

    template <byte_sink Sink>
    [[nodiscard]] bool write_header(Sink& sink, std::uint32_t magic,
                                    std::uint32_t version) noexcept {
        const format_header h{magic, version};
        return sink.write(&h, sizeof(h));
    }

    template <byte_source Source>
    [[nodiscard]] bool read_header(Source& source, std::uint32_t expected_magic,
                                   std::uint32_t expected_version) noexcept {
        format_header h{};
        if (!source.read(&h, sizeof(h)))
            return false;
        return h.magic == expected_magic && h.version == expected_version;
    }

    template <byte_sink Sink, typename T>
        requires std::is_trivially_copyable_v<T>
    [[nodiscard]] bool write_vector(Sink& sink, const std::vector<T>& v) noexcept {
        const auto n = static_cast<std::uint64_t>(v.size());
        if (!sink.write(&n, sizeof(n)))
            return false;
        if (n > 0 && !sink.write(v.data(), static_cast<std::size_t>(n) * sizeof(T)))
            return false;
        return true;
    }

    template <typename T, byte_source Source>
        requires std::is_trivially_copyable_v<T>
    [[nodiscard]] std::expected<std::vector<T>, load_error> read_vector(Source& source) noexcept {
        std::uint64_t n = 0;
        if (!source.read(&n, sizeof(n)))
            return std::unexpected(load_error::io_failure);

        std::vector<T> v(static_cast<std::size_t>(n));
        if (n > 0 && !source.read(v.data(), static_cast<std::size_t>(n) * sizeof(T)))
            return std::unexpected(load_error::io_failure);

        return v;
    }

    template <typename T, byte_source Source>
        requires std::is_trivially_copyable_v<T>
    [[nodiscard]] bool skip_vector(Source& source) noexcept {
        std::uint64_t n = 0;
        if (!source.read(&n, sizeof(n)))
            return false;
        return source.skip(static_cast<std::size_t>(n) * sizeof(T));
    }

    template <typename T, span_source Source>
        requires std::is_trivially_copyable_v<T>
    [[nodiscard]] std::expected<std::span<const T>, load_error>
    view_vector(Source& source) noexcept {
        std::uint64_t n = 0;
        if (!source.read(&n, sizeof(n)))
            return std::unexpected(load_error::io_failure);

        if (n == 0)
            return std::span<const T>{};

        const std::size_t byte_len = static_cast<std::size_t>(n) * sizeof(T);
        const std::span<const std::byte> bytes = source.view(byte_len);
        if (bytes.size() != byte_len)
            return std::unexpected(load_error::io_failure);

        return std::span<const T>(reinterpret_cast<const T*>(bytes.data()),
                                  static_cast<std::size_t>(n));
    }

}
