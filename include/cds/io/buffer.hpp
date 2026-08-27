#pragma once

#include <cds/io/byte.hpp>

#include <cstddef>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

namespace cds::io {

    class buffer_sink {
    public:
        [[nodiscard]] bool write(const void* data, std::size_t n) noexcept {
            const auto* bytes = static_cast<const std::byte*>(data);
            m_buffer.insert(m_buffer.end(), bytes, bytes + n);
            return true;
        }

        [[nodiscard]] std::span<const std::byte> data() const noexcept {
            return m_buffer;
        }
        [[nodiscard]] std::size_t size() const noexcept {
            return m_buffer.size();
        }

        [[nodiscard]] std::vector<std::byte> release() noexcept {
            return std::move(m_buffer);
        }

    private:
        std::vector<std::byte> m_buffer;
    };

    class buffer_source {
    public:
        explicit buffer_source(std::span<const std::byte> data) noexcept : m_data(data) {}

        [[nodiscard]] bool read(void* dest, std::size_t n) noexcept {
            if (m_pos + n > m_data.size())
                return false;
            std::memcpy(dest, m_data.data() + m_pos, n);
            m_pos += n;
            return true;
        }

        [[nodiscard]] bool skip(std::size_t n) noexcept {
            if (m_pos + n > m_data.size())
                return false;
            m_pos += n;
            return true;
        }

        [[nodiscard]] std::span<const std::byte> view(std::size_t n) noexcept {
            if (m_pos + n > m_data.size())
                return {};
            const std::span<const std::byte> result = m_data.subspan(m_pos, n);
            m_pos += n;
            return result;
        }

        [[nodiscard]] std::size_t remaining() const noexcept {
            return m_data.size() - m_pos;
        }

    private:
        std::span<const std::byte> m_data;
        std::size_t m_pos = 0;
    };

    class mutable_buffer_source {
    public:
        explicit mutable_buffer_source(std::span<std::byte> data) noexcept : m_data(data) {}

        [[nodiscard]] bool read(void* dest, std::size_t n) noexcept {
            if (m_pos + n > m_data.size())
                return false;
            std::memcpy(dest, m_data.data() + m_pos, n);
            m_pos += n;
            return true;
        }

        [[nodiscard]] bool skip(std::size_t n) noexcept {
            if (m_pos + n > m_data.size())
                return false;
            m_pos += n;
            return true;
        }

        [[nodiscard]] std::span<const std::byte> view(std::size_t n) noexcept {
            return view_mut(n);
        }

        [[nodiscard]] std::span<std::byte> view_mut(std::size_t n) noexcept {
            if (m_pos + n > m_data.size())
                return {};
            const std::span<std::byte> result = m_data.subspan(m_pos, n);
            m_pos += n;
            return result;
        }

        [[nodiscard]] std::size_t remaining() const noexcept {
            return m_data.size() - m_pos;
        }

    private:
        std::span<std::byte> m_data;
        std::size_t m_pos = 0;
    };

    static_assert(byte_sink<buffer_sink>);
    static_assert(byte_source<buffer_source>);
    static_assert(span_source<buffer_source>);
    static_assert(mutable_span_source<mutable_buffer_source>);

} // namespace cds::io
