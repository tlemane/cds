#pragma once

// Portable memory-mapped file (POSIX mmap / Windows CreateFileMapping) behind
// one interface.

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <system_error>
#include <utility>

namespace cds::io {

    enum class mmap_flag : unsigned {
        shared = 1u,
        priv = 2u,
        anonymous = 4u,
        populate = 8u,
        noreserve = 16u,
    };

    [[nodiscard]] constexpr mmap_flag operator|(mmap_flag a, mmap_flag b) noexcept {
        return static_cast<mmap_flag>(std::to_underlying(a) | std::to_underlying(b));
    }

    [[nodiscard]] constexpr bool flag_set(mmap_flag set, mmap_flag bit) noexcept {
        return (std::to_underlying(set) & std::to_underlying(bit)) != 0u;
    }

    enum class mmap_advise { normal, sequential, random, willneed, dontneed };
    enum class mmap_sync { async, sync, invalidate };
    enum class mmap_mode { read_only, read_write };

} // namespace cds::io

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace cds::io::detail {

    using native_handle = ::HANDLE;
    inline const native_handle invalid_handle = INVALID_HANDLE_VALUE;

    [[nodiscard]] inline bool handle_valid(native_handle h) noexcept {
        return h != INVALID_HANDLE_VALUE && h != nullptr;
    }
    inline void close_handle(native_handle h) noexcept {
        if (handle_valid(h))
            ::CloseHandle(h);
    }

    [[nodiscard]] inline std::error_code last_error() noexcept {
        return std::error_code(static_cast<int>(::GetLastError()), std::system_category());
    }

    [[nodiscard]] inline std::size_t os_page_size() noexcept {
        SYSTEM_INFO si{};
        ::GetSystemInfo(&si);
        return static_cast<std::size_t>(si.dwPageSize);
    }
    [[nodiscard]] inline std::size_t os_mapping_granularity() noexcept {
        SYSTEM_INFO si{};
        ::GetSystemInfo(&si);
        return static_cast<std::size_t>(si.dwAllocationGranularity);
    }

    [[nodiscard]] inline std::expected<native_handle, std::error_code>
    os_open(const std::filesystem::path& path, bool writable) noexcept {
        const DWORD access = writable ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;
        const DWORD share = FILE_SHARE_READ | (writable ? FILE_SHARE_WRITE : 0u);
        HANDLE h = ::CreateFileW(path.c_str(), access, share, nullptr, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return std::unexpected(last_error());
        return h;
    }

    [[nodiscard]] inline std::expected<native_handle, std::error_code>
    os_create(const std::filesystem::path& path, std::size_t byte_size,
              unsigned /*permissions*/) noexcept {
        HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return std::unexpected(last_error());
        LARGE_INTEGER li{};
        li.QuadPart = static_cast<LONGLONG>(byte_size);
        if (!::SetFilePointerEx(h, li, nullptr, FILE_BEGIN) || !::SetEndOfFile(h)) {
            const auto ec = last_error();
            ::CloseHandle(h);
            return std::unexpected(ec);
        }
        return h;
    }

    [[nodiscard]] inline std::expected<std::size_t, std::error_code>
    os_file_size(native_handle h) noexcept {
        LARGE_INTEGER li{};
        if (!::GetFileSizeEx(h, &li))
            return std::unexpected(last_error());
        return static_cast<std::size_t>(li.QuadPart);
    }

    [[nodiscard]] inline std::expected<void*, std::error_code>
    os_map(native_handle fh, std::size_t aligned_offset, std::size_t mapped_bytes, bool writable,
           mmap_flag /*flag*/) noexcept {
        const DWORD protect = writable ? PAGE_READWRITE : PAGE_READONLY;
        HANDLE mapping =
            ::CreateFileMappingW(fh, nullptr, protect, 0, 0, nullptr); // 0/0 => whole file
        if (mapping == nullptr)
            return std::unexpected(last_error());

        const DWORD access = writable ? FILE_MAP_WRITE : FILE_MAP_READ;
        const auto off = static_cast<std::uint64_t>(aligned_offset);
        void* base =
            ::MapViewOfFile(mapping, access, static_cast<DWORD>((off >> 32) & 0xFFFFFFFFull),
                            static_cast<DWORD>(off & 0xFFFFFFFFull), mapped_bytes);
        // The view holds the file alive; the mapping handle is no longer needed.
        if (base == nullptr) {
            const auto ec = last_error();
            ::CloseHandle(mapping);
            return std::unexpected(ec);
        }
        ::CloseHandle(mapping);
        return base;
    }

    inline void os_unmap(void* base, std::size_t /*mapped_bytes*/) noexcept {
        if (base)
            ::UnmapViewOfFile(base);
    }

    [[nodiscard]] inline std::expected<void, std::error_code>
    os_flush(native_handle fh, void* base, std::size_t mapped_bytes, mmap_sync /*mode*/) noexcept {
        if (!::FlushViewOfFile(base, mapped_bytes))
            return std::unexpected(last_error());
        if (!::FlushFileBuffers(fh))
            return std::unexpected(last_error());
        return {};
    }

    [[nodiscard]] inline std::expected<void, std::error_code>
    os_advise(void* /*p*/, std::size_t /*len*/, mmap_advise /*advice*/) noexcept {
        return {}; // no madvise equivalent here; no-op success
    }

} // namespace cds::io::detail

#else // POSIX (Linux, macOS, ...)

#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace cds::io::detail {

    using native_handle = int;
    inline constexpr native_handle invalid_handle = -1;

    [[nodiscard]] inline bool handle_valid(native_handle h) noexcept {
        return h >= 0;
    }
    inline void close_handle(native_handle h) noexcept {
        if (h >= 0)
            ::close(h);
    }

    [[nodiscard]] inline std::error_code last_error() noexcept {
        return std::error_code(errno, std::generic_category());
    }

    [[nodiscard]] inline std::size_t os_page_size() noexcept {
        static const auto sz = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
        return sz;
    }
    [[nodiscard]] inline std::size_t os_mapping_granularity() noexcept {
        return os_page_size();
    }

    [[nodiscard]] inline int to_native_map_flags(mmap_flag f) noexcept {
        int r = 0;
        if (flag_set(f, mmap_flag::shared))
            r |= MAP_SHARED;
        if (flag_set(f, mmap_flag::priv))
            r |= MAP_PRIVATE;
        if (flag_set(f, mmap_flag::anonymous))
            r |= MAP_ANONYMOUS;
#ifdef MAP_POPULATE
        if (flag_set(f, mmap_flag::populate))
            r |= MAP_POPULATE;
#endif
#ifdef MAP_NORESERVE
        if (flag_set(f, mmap_flag::noreserve))
            r |= MAP_NORESERVE;
#endif
        if ((r & (MAP_SHARED | MAP_PRIVATE)) == 0)
            r |= MAP_SHARED;
        return r;
    }

    [[nodiscard]] inline int to_native_advice(mmap_advise a) noexcept {
        switch (a) {
            case mmap_advise::sequential: return POSIX_MADV_SEQUENTIAL;
            case mmap_advise::random: return POSIX_MADV_RANDOM;
            case mmap_advise::willneed: return POSIX_MADV_WILLNEED;
            case mmap_advise::dontneed: return POSIX_MADV_DONTNEED;
            case mmap_advise::normal:
            default: return POSIX_MADV_NORMAL;
        }
    }

    [[nodiscard]] inline int to_native_sync(mmap_sync s) noexcept {
        switch (s) {
            case mmap_sync::async: return MS_ASYNC;
            case mmap_sync::invalidate: return MS_INVALIDATE;
            case mmap_sync::sync:
            default: return MS_SYNC;
        }
    }

    [[nodiscard]] inline std::expected<native_handle, std::error_code>
    os_open(const std::filesystem::path& path, bool writable) noexcept {
        const int fd = ::open(path.c_str(), writable ? O_RDWR : O_RDONLY);
        if (fd < 0)
            return std::unexpected(last_error());
        return fd;
    }

    [[nodiscard]] inline std::expected<native_handle, std::error_code>
    os_create(const std::filesystem::path& path, std::size_t byte_size,
              unsigned permissions) noexcept {
        const int fd =
            ::open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, static_cast<mode_t>(permissions));
        if (fd < 0)
            return std::unexpected(last_error());
        if (::ftruncate(fd, static_cast<off_t>(byte_size)) != 0) {
            const auto ec = last_error();
            ::close(fd);
            return std::unexpected(ec);
        }
        return fd;
    }

    [[nodiscard]] inline std::expected<std::size_t, std::error_code>
    os_file_size(native_handle h) noexcept {
        struct stat st{};
        if (::fstat(h, &st) != 0)
            return std::unexpected(last_error());
        return static_cast<std::size_t>(st.st_size);
    }

    [[nodiscard]] inline std::expected<void*, std::error_code>
    os_map(native_handle fd, std::size_t aligned_offset, std::size_t mapped_bytes, bool writable,
           mmap_flag flag) noexcept {
        const int prot = writable ? (PROT_READ | PROT_WRITE) : PROT_READ;
        void* base = ::mmap(nullptr, mapped_bytes, prot, to_native_map_flags(flag), fd,
                            static_cast<off_t>(aligned_offset));
        if (base == MAP_FAILED)
            return std::unexpected(last_error());
        return base;
    }

    inline void os_unmap(void* base, std::size_t mapped_bytes) noexcept {
        if (base)
            ::munmap(base, mapped_bytes);
    }

    [[nodiscard]] inline std::expected<void, std::error_code>
    os_flush(native_handle /*fd*/, void* base, std::size_t mapped_bytes, mmap_sync mode) noexcept {
        if (::msync(base, mapped_bytes, to_native_sync(mode)) != 0)
            return std::unexpected(last_error());
        return {};
    }

    [[nodiscard]] inline std::expected<void, std::error_code>
    os_advise(void* p, std::size_t len, mmap_advise advice) noexcept {
        if (::posix_madvise(p, len, to_native_advice(advice)) != 0)
            return std::unexpected(last_error());
        return {};
    }

} // namespace cds::io::detail

#endif

namespace cds::io {

    using native_handle = detail::native_handle;
    inline const native_handle invalid_native_handle = detail::invalid_handle;

    [[nodiscard]] inline std::error_code last_os_error() noexcept {
        return detail::last_error();
    }
    [[nodiscard]] inline std::size_t page_size() noexcept {
        return detail::os_page_size();
    }
    [[nodiscard]] inline std::size_t mapping_granularity() noexcept {
        return detail::os_mapping_granularity();
    }

    [[nodiscard]] constexpr std::size_t align_down(std::size_t value,
                                                   std::size_t alignment) noexcept {
        return value - (value % alignment);
    }

    // Move-only RAII owner of a native handle (POSIX fd / Win32 HANDLE).
    class unique_handle {
    public:
        unique_handle() noexcept = default;
        explicit unique_handle(native_handle h) noexcept : m_h(h) {}

        unique_handle(const unique_handle&) = delete;
        unique_handle& operator=(const unique_handle&) = delete;

        unique_handle(unique_handle&& other) noexcept
            : m_h(std::exchange(other.m_h, detail::invalid_handle)) {}

        unique_handle& operator=(unique_handle&& other) noexcept {
            if (this != &other) {
                reset();
                m_h = std::exchange(other.m_h, detail::invalid_handle);
            }
            return *this;
        }

        ~unique_handle() {
            reset();
        }

        void reset(native_handle h = detail::invalid_handle) noexcept {
            detail::close_handle(m_h);
            m_h = h;
        }

        [[nodiscard]] native_handle get() const noexcept {
            return m_h;
        }
        [[nodiscard]] bool valid() const noexcept {
            return detail::handle_valid(m_h);
        }
        [[nodiscard]] native_handle release() noexcept {
            return std::exchange(m_h, detail::invalid_handle);
        }

    private:
        native_handle m_h = detail::invalid_handle;
    };

    // mmap_file<T, Mode>: maps `count` elements of T from a file at
    // `element_offset` (in units of T). view() gives a span<T> (or span<const T>).
    template <typename T, mmap_mode Mode = mmap_mode::read_only> class mmap_file {
    public:
        using value_type = T;
        using size_type = std::size_t;

        static constexpr bool is_writable = (Mode == mmap_mode::read_write);

        mmap_file() noexcept = default;

        mmap_file(const mmap_file&) = delete;
        mmap_file& operator=(const mmap_file&) = delete;

        mmap_file(mmap_file&& other) noexcept {
            move_from(std::move(other));
        }
        mmap_file& operator=(mmap_file&& other) noexcept {
            if (this != &other) {
                close();
                move_from(std::move(other));
            }
            return *this;
        }

        ~mmap_file() {
            close();
        }

        [[nodiscard]] static std::expected<mmap_file, std::error_code>
        open(const std::filesystem::path& path, std::size_t count, std::size_t element_offset = 0,
             mmap_flag flag = mmap_flag::shared) {
            auto h = detail::os_open(path, is_writable);
            if (!h)
                return std::unexpected(h.error());
            return map(unique_handle(*h), count, element_offset, flag);
        }

        [[nodiscard]] static std::expected<mmap_file, std::error_code>
        open_whole_file(const std::filesystem::path& path, mmap_flag flag = mmap_flag::shared) {
            auto h = detail::os_open(path, is_writable);
            if (!h)
                return std::unexpected(h.error());
            unique_handle fd(*h);

            auto byte_size = detail::os_file_size(fd.get());
            if (!byte_size)
                return std::unexpected(byte_size.error());
            if (*byte_size % sizeof(T) != 0)
                return std::unexpected(std::make_error_code(std::errc::invalid_argument));

            return map(std::move(fd), *byte_size / sizeof(T), 0, flag);
        }

        [[nodiscard]] static std::expected<mmap_file, std::error_code>
        create(const std::filesystem::path& path, std::size_t count, unsigned permissions = 0644)
            requires(Mode == mmap_mode::read_write)
        {
            auto h = detail::os_create(path, count * sizeof(T), permissions);
            if (!h)
                return std::unexpected(h.error());
            return map(unique_handle(*h), count, 0, mmap_flag::shared);
        }

        // Takes ownership of `owned` (a POSIX fd or a Win32 file HANDLE).
        [[nodiscard]] static std::expected<mmap_file, std::error_code>
        from_fd(native_handle owned, std::size_t count, std::size_t element_offset = 0,
                mmap_flag flag = mmap_flag::shared) {
            return map(unique_handle(owned), count, element_offset, flag);
        }

        void close() noexcept {
            if (m_base) {
                detail::os_unmap(m_base, m_mapped_bytes);
                m_base = nullptr;
                m_mapped_bytes = 0;
                m_data = nullptr;
                m_count = 0;
            }
            m_fd.reset();
        }

        [[nodiscard]] bool is_open() const noexcept {
            return m_fd.valid();
        }
        [[nodiscard]] bool is_mapped() const noexcept {
            return m_base != nullptr;
        }
        [[nodiscard]] std::size_t size() const noexcept {
            return m_count;
        }

        [[nodiscard]] std::span<const T> view() const noexcept {
            return std::span<const T>(m_data, m_count);
        }

        [[nodiscard]] std::span<T> view() noexcept
            requires(Mode == mmap_mode::read_write)
        {
            return std::span<T>(m_data, m_count);
        }

        [[nodiscard]] std::expected<void, std::error_code>
        advise(mmap_advise advice, std::size_t element_offset = 0, std::size_t count = 0) const {
            if (!m_base)
                return {};
            const std::size_t byte_off = element_offset * sizeof(T);
            const std::size_t byte_len =
                (count == 0 ? (m_count - element_offset) : count) * sizeof(T);
            auto* p = reinterpret_cast<unsigned char*>(m_data) + byte_off;
            return detail::os_advise(p, byte_len, advice);
        }

        [[nodiscard]] std::expected<void, std::error_code>
        sync(mmap_sync mode = mmap_sync::sync) const
            requires(Mode == mmap_mode::read_write)
        {
            if (!m_base)
                return {};
            return detail::os_flush(m_fd.get(), m_base, m_mapped_bytes, mode);
        }

    private:
        mmap_file(unique_handle fd, void* base, std::size_t mapped_bytes, T* data,
                  std::size_t count) noexcept
            : m_fd(std::move(fd)), m_base(base), m_mapped_bytes(mapped_bytes), m_data(data),
              m_count(count) {}

        void move_from(mmap_file&& other) noexcept {
            m_fd = std::move(other.m_fd);
            m_base = std::exchange(other.m_base, nullptr);
            m_mapped_bytes = std::exchange(other.m_mapped_bytes, 0);
            m_data = std::exchange(other.m_data, nullptr);
            m_count = std::exchange(other.m_count, 0);
        }

        [[nodiscard]] static std::expected<mmap_file, std::error_code>
        map(unique_handle fd, std::size_t count, std::size_t element_offset, mmap_flag flag) {
            const std::size_t byte_offset = element_offset * sizeof(T);
            const std::size_t byte_length = count * sizeof(T);

            const std::size_t gran = mapping_granularity();
            const std::size_t aligned_offset = align_down(byte_offset, gran);
            const std::size_t head_room = byte_offset - aligned_offset;
            const std::size_t mapped_bytes = byte_length + head_room;

            auto base = detail::os_map(fd.get(), aligned_offset, mapped_bytes, is_writable, flag);
            if (!base)
                return std::unexpected(base.error());

            T* data = reinterpret_cast<T*>(static_cast<unsigned char*>(*base) + head_room);
            return mmap_file(std::move(fd), *base, mapped_bytes, data, count);
        }

        unique_handle m_fd;
        void* m_base = nullptr; // granularity-aligned base (for unmap)
        std::size_t m_mapped_bytes = 0;
        T* m_data = nullptr; // caller-facing pointer, offset from m_base
        std::size_t m_count = 0;
    };

    template <typename T> using immap = mmap_file<T, mmap_mode::read_only>;

    template <typename T> using ommap = mmap_file<T, mmap_mode::read_write>;

} // namespace cds::io
