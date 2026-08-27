#pragma once

#include <expected>
#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include <cds/core/attributes.hpp>
#include <cds/core/debug.hpp>
#include <cds/core/packed/type.hpp>
#include <cds/core/packed/packer.hpp>
#include <cds/core/packed/iterator.hpp>

#include <cds/packed/view.hpp>

#include <cds/io/format.hpp>
#include <cds/io/byte.hpp>
#include <cds/version.hpp>

#ifndef CDS_PACKED_VECTOR_GROW_FACTOR
#define CDS_PACKED_VECTOR_GROW_FACTOR 2
#endif

namespace cds {

    template <typename Word, pack_endian Endian, bool Fixed,
              typename Allocator = std::allocator<Word>>
    class bit_vector_builder;

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian, pack_mode Mode,
              typename Allocator = std::allocator<Word>>
    class packed_vector : public packing_storage<Width, Endian, Mode> {
        using storage_type = packing_storage<Width, Endian, Mode>;
        using alloc_traits = std::allocator_traits<Allocator>;

    public:
        using word_type = Word;
        using value_type = Value;
        using allocator_type = Allocator;
        using size_type = std::size_t;

        using bp = bit_packer<word_type, value_type, Width, Endian, Mode>;
        using reference = packed_reference<word_type, value_type, Width, Endian, Mode>;
        using const_reference = const_packed_reference<word_type, value_type, Width, Endian, Mode>;
        using iterator = packed_iterator_t<word_type, value_type, Width, Endian, Mode>;
        using const_iterator = const_packed_iterator_t<word_type, value_type, Width, Endian, Mode>;

        using view_type = packed_view<Word, Value, Width, Endian, Mode>;
        using const_view_type = const_packed_view<Word, Value, Width, Endian, Mode>;

        static_assert(std::is_same_v<typename alloc_traits::value_type, word_type>,
                      "Allocator must allocate word_type");

    private:
        constexpr reference get_ref(std::size_t index) noexcept {
            return this->visit([&](auto... args) { return bp::ref(m_data, index, args...); });
        }

        constexpr const_reference get_cref(std::size_t index) const noexcept {
            return this->visit([&](auto... args) { return bp::cref(m_data, index, args...); });
        }

        constexpr iterator get_it(std::size_t index) noexcept {
            return this->visit([&](auto... args) {
                return iterator(m_data, sc<typename iterator::difference_type>(index), args...);
            });
        }

        constexpr const_iterator get_cit(std::size_t index) const noexcept {
            return this->visit([&](auto... args) {
                return const_iterator(m_data, sc<typename iterator::difference_type>(index),
                                      args...);
            });
        }

        std::size_t words_for(std::size_t n) const noexcept {
            return to_nb_words<word_type>(n, this->width(), this->mode());
        }

        std::size_t elements_for(std::size_t nb_words) const noexcept {
            return to_capacity<word_type>(nb_words, this->width(), this->mode());
        }

        void reallocate(std::size_t new_capacity_words) {
            word_type* new_data = alloc_traits::allocate(m_alloc, new_capacity_words);
            std::fill_n(new_data, new_capacity_words, word_type{0});

            if (m_data) {
                std::copy_n(m_data, std::min(m_capacity_words, new_capacity_words), new_data);
                alloc_traits::deallocate(m_alloc, m_data, m_capacity_words);
            }

            m_data = new_data;
            m_capacity_words = new_capacity_words;
            m_capacity_elements = elements_for(new_capacity_words);
        }

        void ensure_capacity_for(std::size_t new_size) {
            const std::size_t needed = words_for(new_size);
            if (needed <= m_capacity_words)
                return;

            std::size_t grown = m_capacity_words == 0 ? 1 : m_capacity_words;
            while (grown < needed) {
                const auto scaled = static_cast<std::size_t>(
                    std::ceil(static_cast<double>(grown) * CDS_PACKED_VECTOR_GROW_FACTOR));
                grown = scaled > grown ? scaled : grown + 1;
            }
            reallocate(grown);
        }

        void release() noexcept {
            if (m_data)
                alloc_traits::deallocate(m_alloc, m_data, m_capacity_words);
            m_data = nullptr;
            m_capacity_words = 0;
            m_capacity_elements = 0;
            m_size = 0;
        }

    public:
        constexpr explicit packed_vector(
            std::uint8_t width = Width, pack_endian endian = Endian, pack_mode mode = Mode,
            const allocator_type& alloc =
                allocator_type()) noexcept(std::is_nothrow_copy_constructible_v<allocator_type>)
            : storage_type(width, endian, mode), m_alloc(alloc) {}

        template <typename InputIt>
        constexpr packed_vector(InputIt first, InputIt last, std::uint8_t width = Width,
                                pack_endian endian = Endian, pack_mode mode = Mode,
                                const allocator_type& alloc = allocator_type())
            : packed_vector(width, endian, mode, alloc) {
            std::size_t r = sc<std::size_t>(last - first);
            reserve(r);
            for (; first != last; ++first)
                push_back(*first);
        }

        constexpr packed_vector(std::initializer_list<value_type> values,
                                std::uint8_t width = Width, pack_endian endian = Endian,
                                pack_mode mode = Mode,
                                const allocator_type& alloc = allocator_type())
            : packed_vector(values.begin(), values.end(), width, endian, mode, alloc) {}

        packed_vector(const packed_vector& other)
            : storage_type(static_cast<const storage_type&>(other)),
              m_alloc(alloc_traits::select_on_container_copy_construction(other.m_alloc)) {
            const std::size_t needed = words_for(other.m_size);
            if (needed > 0)
                reallocate(needed);
            std::copy_n(other.m_data, needed, m_data);
            m_size = other.m_size;
        }

        packed_vector(packed_vector&& other) noexcept
            : storage_type(static_cast<storage_type&&>(other)), m_alloc(std::move(other.m_alloc)),
              m_data(other.m_data), m_size(other.m_size), m_capacity_words(other.m_capacity_words),
              m_capacity_elements(other.m_capacity_elements) {
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity_words = 0;
            other.m_capacity_elements = 0;
        }

        template <bool Fixed>
            requires(Width == 1)
        explicit packed_vector(bit_vector_builder<Word, Endian, Fixed, Allocator>&& builder);

        packed_vector& operator=(const packed_vector& other) {
            if (this == &other)
                return *this;

            static_cast<storage_type&>(*this) = static_cast<const storage_type&>(other);

            if constexpr (alloc_traits::propagate_on_container_copy_assignment::value) {
                if (m_alloc != other.m_alloc)
                    release();
                m_alloc = other.m_alloc;
            }

            const std::size_t needed = words_for(other.m_size);
            if (needed > m_capacity_words)
                reallocate(needed);

            std::copy_n(other.m_data, needed, m_data);
            m_size = other.m_size;
            return *this;
        }

        packed_vector& operator=(packed_vector&& other) noexcept(
            alloc_traits::propagate_on_container_move_assignment::value ||
            alloc_traits::is_always_equal::value) {
            if (this == &other)
                return *this;

            static_cast<storage_type&>(*this) = static_cast<storage_type&&>(other);

            const bool can_steal = alloc_traits::propagate_on_container_move_assignment::value ||
                                   m_alloc == other.m_alloc;

            if (can_steal) {
                release();
                if constexpr (alloc_traits::propagate_on_container_move_assignment::value)
                    m_alloc = std::move(other.m_alloc);

                m_data = other.m_data;
                m_size = other.m_size;
                m_capacity_words = other.m_capacity_words;
                m_capacity_elements = other.m_capacity_elements;

                other.m_data = nullptr;
                other.m_size = 0;
                other.m_capacity_words = 0;
                other.m_capacity_elements = 0;

            } else {
                const std::size_t needed = words_for(other.m_size);
                if (needed > m_capacity_words)
                    reallocate(needed);
                std::copy_n(other.m_data, needed, m_data);
                m_size = other.m_size;
                other.clear();
            }
            return *this;
        }

        ~packed_vector() {
            release();
        }

        [[nodiscard]] view_type as_view() noexcept {
            return {data(), size(), this->width(), this->endian(), this->mode()};
        }

        [[nodiscard]] const_view_type as_const_view() const noexcept {
            return {data(), size(), this->width(), this->endian(), this->mode()};
        }

        void swap(packed_vector& other) noexcept(alloc_traits::propagate_on_container_swap::value ||
                                                 alloc_traits::is_always_equal::value) {
            using std::swap;
            swap(static_cast<storage_type&>(*this), static_cast<storage_type&>(other));
            if constexpr (alloc_traits::propagate_on_container_swap::value)
                swap(m_alloc, other.m_alloc);
            swap(m_data, other.m_data);
            swap(m_size, other.m_size);
            swap(m_capacity_words, other.m_capacity_words);
            swap(m_capacity_elements, other.m_capacity_elements);
        }

        [[nodiscard]] constexpr reference operator[](std::size_t index) noexcept {
            CDS_ASSERT(index < m_size, "out of range: index={}, size={}", index, m_size);
            return get_ref(index);
        }

        [[nodiscard]] constexpr const_reference operator[](std::size_t index) const noexcept {
            CDS_ASSERT(index < m_size, "out of range: index={}, size={}", index, m_size);
            return get_cref(index);
        }

        [[nodiscard]] constexpr reference at(std::size_t index) {
            CDS_PANIC(index < m_size, "out of range: index={}, size={}", index, m_size);
            return get_ref(index);
        }

        [[nodiscard]] constexpr const_reference at(std::size_t index) const {
            CDS_PANIC(index < m_size, "out of range: index={}, size={}", index, m_size);
            return get_cref(index);
        }

        [[nodiscard]] constexpr reference front() noexcept {
            return get_ref(0);
        }
        [[nodiscard]] constexpr const_reference front() const noexcept {
            return get_cref(0);
        }
        [[nodiscard]] constexpr reference back() noexcept {
            return get_ref(m_size - 1);
        }
        [[nodiscard]] constexpr const_reference back() const noexcept {
            return get_cref(m_size - 1);
        }

        [[nodiscard]] constexpr word_type* data() noexcept {
            return m_data;
        }
        [[nodiscard]] constexpr const word_type* data() const noexcept {
            return m_data;
        }

        [[nodiscard]] constexpr iterator begin() noexcept {
            return get_it(0);
        }
        [[nodiscard]] constexpr iterator end() noexcept {
            return get_it(m_size);
        }
        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            return get_cit(0);
        }
        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return get_cit(m_size);
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
        [[nodiscard]] constexpr std::size_t capacity() const noexcept {
            return elements_for(m_capacity_words);
        }
        [[nodiscard]] constexpr std::size_t nb_words() const noexcept {
            return m_capacity_words;
        }
        [[nodiscard]] constexpr std::size_t memory_size() const noexcept {
            return nb_words() * sizeof(Word) + sizeof(*this);
        }

        void reserve(std::size_t new_capacity) {
            const std::size_t needed = words_for(new_capacity);
            if (needed > m_capacity_words)
                reallocate(needed);
        }

        void shrink_to_fit() {
            const std::size_t needed = words_for(m_size);
            if (needed < m_capacity_words)
                reallocate(needed);
        }

        [[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
            return m_alloc;
        }

        auto& push_back(value_type value) {
            if (m_size >= m_capacity_elements) [[unlikely]]
                ensure_capacity_for(m_size + 1);
            get_ref(m_size) = value;
            ++m_size;
            return *this;
        }

        template <typename V>
            requires std::same_as<V, value_type> || std::same_as<V, unsafe<value_type>>
        auto& push_back(V value) {
            if (m_size >= m_capacity_elements) [[unlikely]]
                ensure_capacity_for(m_size + 1);
            get_ref(m_size) = value;
            ++m_size;
            return *this;
        }

        template <typename V>
            requires std::same_as<V, value_type> || std::same_as<V, unsafe<value_type>>
        auto& push_back(std::size_t c, V value) {
            if (c == 0)
                return *this;

            ensure_capacity_for(m_size + c);

            if constexpr (Width == 1) {
                const bool bit = static_cast<value_type>(value) != value_type{0};
                std::size_t remaining = c;

                const std::size_t misalignment = m_size % 64;
                if (misalignment != 0) {
                    const std::size_t to_align = std::min(remaining, 64 - misalignment);
                    for (std::size_t i = 0; i < to_align; ++i) {
                        get_ref(m_size) = value;
                        ++m_size;
                    }
                    remaining -= to_align;
                }

                const std::size_t whole_words = remaining / 64;
                if (whole_words > 0) {
                    const word_type fill_word = bit ? ~word_type{0} : word_type{0};
                    std::fill_n(m_data + (m_size / 64), whole_words, fill_word);
                    m_size += whole_words * 64;
                    remaining -= whole_words * 64;
                }

                for (std::size_t i = 0; i < remaining; ++i) {
                    get_ref(m_size) = value;
                    ++m_size;
                }
            } else {
                for (std::size_t i = 0; i < c; ++i) {
                    get_ref(m_size) = value;
                    ++m_size;
                }
            }

            return *this;
        }

        auto& emplace_back(value_type value) {
            return push_back(value);
        }

        void pop_back() noexcept {
            CDS_ASSERT(m_size > 0, "pop_back on empty packed_vector");
            --m_size;
        }

        void resize(std::size_t new_size, value_type value = value_type{}) {
            if (new_size > m_size) {
                ensure_capacity_for(new_size);
                for (std::size_t i = m_size; i < new_size; ++i)
                    get_ref(i) = value;
            }
            m_size = new_size;
        }

        void clear() noexcept {
            m_size = 0;
        }

        template <typename Sink> [[nodiscard]] bool save(Sink& sink) const noexcept {
            if (!io::write_cds_version(sink))
                return false;

            const detail::packed_save_header h{detail::packed_save_magic,
                                               this->width(),
                                               static_cast<std::uint8_t>(this->endian()),
                                               static_cast<std::uint8_t>(this->mode()),
                                               0,
                                               static_cast<std::uint64_t>(m_size)};

            if (!sink.write(&h, sizeof(h)))
                return false;

            const std::size_t nw = words_for(m_size);
            if (nw > 0 && !sink.write(m_data, nw * sizeof(word_type)))
                return false;

            return true;
        }

        template <typename Source>
        [[nodiscard]] static std::expected<packed_vector, io::load_error> load(Source& source) {
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

            packed_vector v(width, endian, mode);
            v.reserve(static_cast<std::size_t>(h.size));
            v.m_size = static_cast<std::size_t>(h.size);

            const std::size_t nw = v.words_for(v.m_size);
            if (nw > 0 && !source.read(v.m_data, nw * sizeof(word_type)))
                return std::unexpected(io::load_error::io_failure);

            return v;
        }

    private:
        CDS_NO_UNIQUE_ADDRESS allocator_type m_alloc{};
        word_type* m_data{nullptr};
        std::size_t m_size{0};
        std::size_t m_capacity_words{0};
        std::size_t m_capacity_elements{0};
    };

    template <typename Word, typename Value, std::uint8_t Width, pack_endian Endian, pack_mode Mode,
              typename Allocator>
    void swap(packed_vector<Word, Value, Width, Endian, Mode, Allocator>& lhs,
              packed_vector<Word, Value, Width, Endian, Mode, Allocator>&
                  rhs) noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }
} // namespace cds
