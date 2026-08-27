#pragma once

#include <algorithm>
#include <limits>
#include <utility>
#include <cds/core/attributes.hpp>
#include <cds/bit/vector.hpp>
#include <cds/core/packed/ops.hpp>

namespace cds {

    template <typename Word, pack_endian Endian, typename Allocator>
    class bit_vector_builder<Word, Endian, true, Allocator> {
        static constexpr std::size_t digits = std::numeric_limits<Word>::digits;
        using alloc_traits = std::allocator_traits<Allocator>;

    public:
        explicit bit_vector_builder(std::size_t bit_size, const Allocator& alloc = Allocator())
            : m_alloc(alloc), m_capacity_words((bit_size + digits - 1) / digits), m_size(bit_size) {
            m_data = alloc_traits::allocate(m_alloc, m_capacity_words);
            std::fill_n(m_data, m_capacity_words, Word{0});
        }

        bit_vector_builder(const bit_vector_builder&) = delete;
        bit_vector_builder& operator=(const bit_vector_builder&) = delete;

        bit_vector_builder(bit_vector_builder&& other) noexcept
            : m_alloc(std::move(other.m_alloc)), m_data(std::exchange(other.m_data, nullptr)),
              m_capacity_words(std::exchange(other.m_capacity_words, 0)),
              m_size(std::exchange(other.m_size, 0)) {}

        bit_vector_builder& operator=(bit_vector_builder&& other) noexcept {
            if (this != &other) {
                release();
                m_alloc = std::move(other.m_alloc);
                m_data = std::exchange(other.m_data, nullptr);
                m_capacity_words = std::exchange(other.m_capacity_words, 0);
                m_size = std::exchange(other.m_size, 0);
            }
            return *this;
        }

        ~bit_vector_builder() {
            release();
        }

        // Precondition: index has not been written before.
        void set_bit(std::size_t index) noexcept {
            bit_packer<Word, std::uint8_t, 1, Endian, pack_mode::sparse>::template pack<false>(
                m_data, index, std::uint8_t{1});
        }

        // Precondition: index has not been written before.
        void set_range(std::size_t pos, std::size_t count) noexcept {
            if (count == 0)
                return;

            std::size_t offset = pos;
            std::size_t remaining = count;

            const std::size_t misalignment = offset % digits;
            if (misalignment != 0) {
                const std::size_t chunk = std::min<std::size_t>(remaining, digits - misalignment);
                const Word v = (chunk == digits) ? static_cast<Word>(~Word{0})
                                                 : static_cast<Word>((Word{1} << chunk) - 1);
                bit_ops<Word, Endian>::template insert<false>(m_data, offset,
                                                              static_cast<std::uint8_t>(chunk), v);
                offset += chunk;
                remaining -= chunk;
            }

            const std::size_t whole_words = remaining / digits;
            if (whole_words > 0) {
                std::fill_n(m_data + offset / digits, whole_words, ~Word{0});
                offset += whole_words * digits;
                remaining -= whole_words * digits;
            }

            if (remaining > 0) {
                const Word v = (remaining == digits)
                                   ? static_cast<Word>(~Word{0})
                                   : static_cast<Word>((Word{1} << remaining) - 1);
                bit_ops<Word, Endian>::template insert<false>(
                    m_data, offset, static_cast<std::uint8_t>(remaining), v);
            }
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }

        CDS_NO_UNIQUE_ADDRESS Allocator m_alloc;
        Word* m_data = nullptr;
        std::size_t m_capacity_words = 0;
        std::size_t m_size = 0;

    private:
        void release() noexcept {
            if (m_data)
                alloc_traits::deallocate(m_alloc, m_data, m_capacity_words);
            m_data = nullptr;
        }
    };

    template <typename Word, pack_endian Endian, typename Allocator>
    class bit_vector_builder<Word, Endian, false, Allocator> {
        static constexpr std::size_t digits = std::numeric_limits<Word>::digits;
        using alloc_traits = std::allocator_traits<Allocator>;

    public:
        bit_vector_builder() = default;
        explicit bit_vector_builder(const Allocator& alloc) : m_alloc(alloc) {}

        bit_vector_builder(const bit_vector_builder&) = delete;
        bit_vector_builder& operator=(const bit_vector_builder&) = delete;

        bit_vector_builder(bit_vector_builder&& other) noexcept
            : m_alloc(std::move(other.m_alloc)), m_data(std::exchange(other.m_data, nullptr)),
              m_capacity_words(std::exchange(other.m_capacity_words, 0)),
              m_size(std::exchange(other.m_size, 0)) {}

        bit_vector_builder& operator=(bit_vector_builder&& other) noexcept {
            if (this != &other) {
                release();
                m_alloc = std::move(other.m_alloc);
                m_data = std::exchange(other.m_data, nullptr);
                m_capacity_words = std::exchange(other.m_capacity_words, 0);
                m_size = std::exchange(other.m_size, 0);
            }
            return *this;
        }

        ~bit_vector_builder() {
            release();
        }

        void push_bit(bool value) noexcept {
            ensure_capacity_for(m_size + 1);
            if (value) {
                bit_packer<Word, std::uint8_t, 1, Endian, pack_mode::sparse>::template pack<false>(
                    m_data, m_size, std::uint8_t{1});
            }
            ++m_size;
        }

        void push_range(std::size_t count, bool value) noexcept {
            if (count == 0)
                return;
            ensure_capacity_for(m_size + count);
            if (value) {
                std::size_t offset = m_size;
                std::size_t remaining = count;

                const std::size_t misalignment = offset % digits;
                if (misalignment != 0) {
                    const std::size_t chunk =
                        std::min<std::size_t>(remaining, digits - misalignment);
                    const Word v = (chunk == digits) ? static_cast<Word>(~Word{0})
                                                     : static_cast<Word>((Word{1} << chunk) - 1);
                    bit_ops<Word, Endian>::template insert<false>(
                        m_data, offset, static_cast<std::uint8_t>(chunk), v);
                    offset += chunk;
                    remaining -= chunk;
                }

                const std::size_t whole_words = remaining / digits;
                if (whole_words > 0) {
                    std::fill_n(m_data + offset / digits, whole_words, ~Word{0});
                    offset += whole_words * digits;
                    remaining -= whole_words * digits;
                }

                if (remaining > 0) {
                    const Word v = (remaining == digits)
                                       ? static_cast<Word>(~Word{0})
                                       : static_cast<Word>((Word{1} << remaining) - 1);
                    bit_ops<Word, Endian>::template insert<false>(
                        m_data, offset, static_cast<std::uint8_t>(remaining), v);
                }
            }
            m_size += count;
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }

        CDS_NO_UNIQUE_ADDRESS Allocator m_alloc;
        Word* m_data = nullptr;
        std::size_t m_capacity_words = 0;
        std::size_t m_size = 0;

    private:
        void ensure_capacity_for(std::size_t new_size) noexcept {
            const std::size_t needed = (new_size + digits - 1) / digits;
            if (needed <= m_capacity_words)
                return;
            const std::size_t new_capacity = std::max(needed, m_capacity_words * 2);
            Word* new_data = alloc_traits::allocate(m_alloc, new_capacity);
            std::fill_n(new_data, new_capacity, Word{0});
            if (m_data) {
                std::copy_n(m_data, m_capacity_words, new_data);
                alloc_traits::deallocate(m_alloc, m_data, m_capacity_words);
            }
            m_data = new_data;
            m_capacity_words = new_capacity;
        }

        void release() noexcept {
            if (m_data)
                alloc_traits::deallocate(m_alloc, m_data, m_capacity_words);
            m_data = nullptr;
        }
    };

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian, pack_mode Mode,
              typename Allocator>
    template <bool Fixed>
        requires(Width == 1)
    packed_vector<Word, Value, Width, Endian, Mode, Allocator>::packed_vector(
        bit_vector_builder<Word, Endian, Fixed, Allocator>&& builder)
        : storage_type(Width, Endian, Mode), m_alloc(std::move(builder.m_alloc)),
          m_data(std::exchange(builder.m_data, nullptr)), m_size(builder.m_size),
          m_capacity_words(std::exchange(builder.m_capacity_words, 0)),
          m_capacity_elements(elements_for(m_capacity_words)) {
        builder.m_size = 0;
    }

} // namespace cds
