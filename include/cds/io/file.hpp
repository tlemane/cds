#pragma once

#include <cds/io/byte.hpp>

#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>

namespace cds::io {

    // Buffered file I/O over std::FILE, so it is portable (POSIX and Windows)
    // with no platform headers. Binary mode ("wb"/"rb") keeps byte counts exact.

    class file_sink {
    public:
        [[nodiscard]] static std::optional<file_sink> open(const std::string& path) noexcept {
            std::FILE* fp = std::fopen(path.c_str(), "wb");
            if (fp == nullptr)
                return std::nullopt;
            return file_sink(fp);
        }

        file_sink(const file_sink&) = delete;
        file_sink& operator=(const file_sink&) = delete;

        file_sink(file_sink&& other) noexcept : m_fp(std::exchange(other.m_fp, nullptr)) {}
        file_sink& operator=(file_sink&& other) noexcept {
            if (this != &other) {
                release();
                m_fp = std::exchange(other.m_fp, nullptr);
            }
            return *this;
        }

        ~file_sink() {
            release();
        }

        [[nodiscard]] bool write(const void* data, std::size_t n) noexcept {
            if (m_fp == nullptr)
                return false;
            return std::fwrite(data, 1, n, m_fp) == n;
        }

    private:
        explicit file_sink(std::FILE* fp) noexcept : m_fp(fp) {}
        void release() noexcept {
            if (m_fp != nullptr)
                std::fclose(m_fp);
            m_fp = nullptr;
        }

        std::FILE* m_fp = nullptr;
    };

    class file_source {
    public:
        [[nodiscard]] static std::optional<file_source> open(const std::string& path) noexcept {
            std::FILE* fp = std::fopen(path.c_str(), "rb");
            if (fp == nullptr)
                return std::nullopt;
            return file_source(fp);
        }

        file_source(const file_source&) = delete;
        file_source& operator=(const file_source&) = delete;

        file_source(file_source&& other) noexcept : m_fp(std::exchange(other.m_fp, nullptr)) {}
        file_source& operator=(file_source&& other) noexcept {
            if (this != &other) {
                release();
                m_fp = std::exchange(other.m_fp, nullptr);
            }
            return *this;
        }

        ~file_source() {
            release();
        }

        [[nodiscard]] bool read(void* data, std::size_t n) noexcept {
            if (m_fp == nullptr)
                return false;
            return std::fread(data, 1, n, m_fp) == n;
        }

        [[nodiscard]] bool skip(std::size_t n) noexcept {
            if (m_fp == nullptr)
                return false;
            return std::fseek(m_fp, static_cast<long>(n), SEEK_CUR) == 0;
        }

    private:
        explicit file_source(std::FILE* fp) noexcept : m_fp(fp) {}
        void release() noexcept {
            if (m_fp != nullptr)
                std::fclose(m_fp);
            m_fp = nullptr;
        }

        std::FILE* m_fp = nullptr;
    };

    static_assert(byte_sink<file_sink>);
    static_assert(byte_source<file_source>);

} // namespace cds::io
