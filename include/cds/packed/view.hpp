#pragma once

#include <expected>
#include <span>
#include <type_traits>

#include <cds/core/debug.hpp>
#include <cds/core/packed/type.hpp>
#include <cds/core/packed/packer.hpp>
#include <cds/core/packed/iterator.hpp>

#include <cds/io/format.hpp>
#include <cds/io/byte.hpp>
#include <cds/version.hpp>

namespace cds {

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian, pack_mode Mode,
              bool Const>
    class packed_view_impl : public packing_storage<Width, Endian, Mode> {
        using storage_type = packing_storage<Width, Endian, Mode>;

        template <typename, typename, std::uint8_t, pack_endian, pack_mode, bool>
        friend class packed_view_impl;

    public:
        using word_type = Word;
        using value_type = Value;

        using bp = bit_packer<word_type, value_type, Width, Endian, Mode>;

        using pointer = std::conditional_t<Const, const word_type*, word_type*>;

        using reference =
            std::conditional_t<Const,
                               const_packed_reference<word_type, value_type, Width, Endian, Mode>,
                               packed_reference<word_type, value_type, Width, Endian, Mode>>;
        using const_reference = const_packed_reference<word_type, value_type, Width, Endian, Mode>;

        using iterator =
            std::conditional_t<Const,
                               const_packed_iterator_t<word_type, value_type, Width, Endian, Mode>,
                               packed_iterator_t<word_type, value_type, Width, Endian, Mode>>;
        using const_iterator = const_packed_iterator_t<word_type, value_type, Width, Endian, Mode>;

        static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    private:
        constexpr reference get_ref(std::size_t index) const noexcept {
            return this->visit([&](auto... args) {
                if constexpr (Const)
                    return bp::cref(m_data, m_offset + index, args...);
                else
                    return bp::ref(m_data, m_offset + index, args...);
            });
        }

        constexpr const_reference get_cref(std::size_t index) const noexcept {
            return this->visit(
                [&](auto... args) { return bp::cref(m_data, m_offset + index, args...); });
        }

        constexpr iterator get_it(std::size_t index) const noexcept {
            return this->visit([&](auto... args) {
                return iterator(m_data, sc<typename iterator::difference_type>(m_offset + index),
                                args...);
            });
        }

        constexpr const_iterator get_cit(std::size_t index) const noexcept {
            return this->visit([&](auto... args) {
                return const_iterator(
                    m_data, sc<typename iterator::difference_type>(m_offset + index), args...);
            });
        }

        constexpr packed_view_impl(pointer data, std::size_t offset, std::size_t size,
                                   std::uint8_t width, pack_endian endian, pack_mode mode) noexcept
            : storage_type(width, endian, mode), m_data(data), m_offset(offset), m_size(size) {}

    public:
        constexpr packed_view_impl() noexcept = default;

        constexpr packed_view_impl(pointer data, std::size_t size, std::uint8_t width = Width,
                                   pack_endian endian = Endian, pack_mode mode = Mode) noexcept
            : storage_type(width, endian, mode), m_data(data), m_offset(0), m_size(size) {}

        [[nodiscard]] static constexpr packed_view_impl
        from_words(pointer data, std::size_t nb_words, std::uint8_t width = Width,
                   pack_endian endian = Endian, pack_mode mode = Mode) noexcept {
            return packed_view_impl(data, to_capacity<word_type>(nb_words, width, mode), width,
                                    endian, mode);
        }

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wclass-conversion"
#endif
        template <bool C = Const, typename = std::enable_if_t<!C>>
        constexpr operator packed_view_impl<word_type, value_type, Width, Endian, Mode, true>()
            const noexcept {
            return packed_view_impl<word_type, value_type, Width, Endian, Mode, true>(
                m_data, m_offset, m_size, this->width(), this->endian(), this->mode());
        }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

        [[nodiscard]] constexpr reference operator[](std::size_t index) const noexcept {
            CDS_ASSERT(index < m_size, "out of range: index={}, size={}", index, m_size);
            return get_ref(index);
        }

        [[nodiscard]] constexpr reference at(std::size_t index) const {
            CDS_PANIC(index < m_size, "out of range: index={}, size={}", index, m_size);
            return get_ref(index);
        }

        [[nodiscard]] constexpr reference front() const noexcept {
            return get_ref(0);
        }

        [[nodiscard]] constexpr reference back() const noexcept {
            return get_ref(m_size - 1);
        }

        [[nodiscard]] constexpr iterator begin() const noexcept {
            return get_it(0);
        }

        [[nodiscard]] constexpr iterator end() const noexcept {
            return get_it(m_size);
        }

        [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
            return get_cit(0);
        }

        [[nodiscard]] constexpr const_iterator cend() const noexcept {
            return get_cit(m_size);
        }

        [[nodiscard]] constexpr std::size_t size() const noexcept {
            return m_size;
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return m_size == 0;
        }

        [[nodiscard]] constexpr pointer data() const noexcept {
            return m_data;
        }

        [[nodiscard]] constexpr std::size_t offset() const noexcept {
            return m_offset;
        }

        template <typename Source>
            requires Const && io::span_source<Source>
        [[nodiscard]] static std::expected<packed_view_impl, io::load_error>
        load(Source& source) noexcept {
            const auto version = io::read_cds_version_compatible(source);
            if (!version)
                return std::unexpected(version.error());

            detail::packed_save_header h{};
            if (!source.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::packed_save_magic)
                return std::unexpected(io::load_error::bad_magic);

            const auto params = detail::resolve_packing_params<Width, Endian, Mode>(h);
            if (!params)
                return std::unexpected(io::load_error::type_mismatch);
            const auto [width, endian, mode] = *params;

            const auto view_size = static_cast<std::size_t>(h.size);
            const std::size_t nw = to_nb_words<word_type>(view_size, width, mode);
            const std::size_t byte_len = nw * sizeof(word_type);

            const std::span<const std::byte> bytes = source.view(byte_len);
            if (bytes.size() != byte_len)
                return std::unexpected(io::load_error::io_failure);

            const auto* words_ptr = reinterpret_cast<const word_type*>(bytes.data());
            return packed_view_impl(words_ptr, view_size, width, endian, mode);
        }

        template <typename Source>
            requires(!Const) && io::mutable_span_source<Source>
        [[nodiscard]] static std::expected<packed_view_impl, io::load_error>
        load(Source& source) noexcept {
            const auto version = io::read_cds_version_compatible(source);
            if (!version)
                return std::unexpected(version.error());

            detail::packed_save_header h{};
            if (!source.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::packed_save_magic)
                return std::unexpected(io::load_error::bad_magic);

            const auto params = detail::resolve_packing_params<Width, Endian, Mode>(h);
            if (!params)
                return std::unexpected(io::load_error::type_mismatch);
            const auto [width, endian, mode] = *params;

            const auto view_size = static_cast<std::size_t>(h.size);
            const std::size_t nw = to_nb_words<word_type>(view_size, width, mode);
            const std::size_t byte_len = nw * sizeof(word_type);

            const std::span<std::byte> bytes = source.view_mut(byte_len);
            if (bytes.size() != byte_len)
                return std::unexpected(io::load_error::io_failure);

            auto* words_ptr = reinterpret_cast<word_type*>(bytes.data());
            return packed_view_impl(words_ptr, view_size, width, endian, mode);
        }

        [[nodiscard]] constexpr std::size_t nb_words() const noexcept {
            return to_nb_words<word_type>(m_offset + m_size, this->width(), this->mode());
        }

        [[nodiscard]] constexpr std::span<std::conditional_t<Const, const word_type, word_type>>
        span() const noexcept {
            return {m_data, nb_words()};
        }

        [[nodiscard]] constexpr packed_view_impl first(std::size_t count) const noexcept {
            return packed_view_impl(m_data, m_offset, count, this->width(), this->endian(),
                                    this->mode());
        }

        [[nodiscard]] constexpr packed_view_impl subview(std::size_t offset,
                                                         std::size_t count = npos) const noexcept {
            const std::size_t n = (count == npos) ? (m_size - offset) : count;
            return packed_view_impl(m_data, m_offset + offset, n, this->width(), this->endian(),
                                    this->mode());
        }

    private:
        pointer m_data{nullptr};
        std::size_t m_offset{0};
        std::size_t m_size{0};
    };

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian, pack_mode Mode>
    using packed_view = packed_view_impl<Word, Value, Width, Endian, Mode, false>;

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian, pack_mode Mode>
    using const_packed_view = packed_view_impl<Word, Value, Width, Endian, Mode, true>;

} // namespace cds
