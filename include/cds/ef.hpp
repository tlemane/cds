#pragma once

// Elias 1974; Fano 1971; Vigna
// "Quasi-Succinct Indices", WSDM 2013.

#include <bit>
#include <concepts>
#include <cstdint>
#include <expected>
#include <iterator>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <cds/bit/builder.hpp>
#include <cds/bit/vector.hpp>
#include <cds/bit/view.hpp>
#include <cds/core/broadword.hpp>
#include <cds/core/debug.hpp>
#include <cds/core/packed/ops.hpp>
#include <cds/io/byte.hpp>
#include <cds/packed/vector.hpp>
#include <cds/select/darray.hpp>

namespace cds {

    template <typename T, typename Source>
    concept select_over_source = select1_structure<T> && std::constructible_from<T, const Source&>;

    template <typename T, typename Source>
    concept select01_over_source = select_over_source<T, Source> && select0_structure<T>;

}

namespace cds::detail {

    struct ef_return_value {
        std::size_t pos;
        std::uint64_t val;
    };

    inline constexpr std::size_t ef_npos = static_cast<std::size_t>(-1);
    inline constexpr std::uint64_t ef_novalue = static_cast<std::uint64_t>(-1);

    struct ef_header {
        std::uint32_t magic;
        std::uint32_t reserved{0};
        std::uint64_t size;
        std::uint64_t universe;
        std::uint64_t back;
        std::uint64_t low_width;
    };
    static_assert(io::mmap_aligned_header<ef_header>);

    inline constexpr std::uint32_t ef_magic = io::cds_magic(io::format_id::ef);
    //
    // Precondition: a set bit exists at or after from_bit within the valid
    // (non-padding) portion of data. Callers must never search past the last
    // real 1-bit
    template <pack_endian Endian>
    [[nodiscard]] inline std::size_t ef_find_next_set_bit(const std::uint64_t* data,
                                                          std::size_t from_bit) noexcept {
        std::size_t word_idx = from_bit / 64;
        const std::size_t bit_in_word = from_bit % 64;

        std::uint64_t word = data[word_idx];
        if constexpr (Endian == pack_endian::lsb)
            word &= (~std::uint64_t{0} << bit_in_word);
        else
            word &= (~std::uint64_t{0} >> bit_in_word);

        while (word == 0) {
            ++word_idx;
            word = data[word_idx];
        }
        if constexpr (Endian == pack_endian::lsb)
            return word_idx * broadword::digits + static_cast<std::size_t>(std::countr_zero(word));
        else
            return word_idx * broadword::digits + static_cast<std::size_t>(std::countl_zero(word));
    }

    // Backward companion to ef_find_next_set_bit. Highest set bit at or before from_bit.
    // Precondition: a set bit exists at or before from_bit.
    [[nodiscard]] inline std::size_t ef_find_prev_set_bit_lsb(const std::uint64_t* data,
                                                              std::size_t from_bit) noexcept {
        std::size_t word_idx = from_bit / 64;
        const std::size_t bit_in_word = from_bit % 64;
        std::uint64_t word =
            data[word_idx] & ((bit_in_word == 63) ? ~std::uint64_t{0}
                                                  : ((std::uint64_t{1} << (bit_in_word + 1)) - 1));
        while (word == 0)
            word = data[--word_idx];
        return word_idx * 64 + (63 - static_cast<std::size_t>(std::countl_zero(word)));
    }

    template <typename Derived> class ef_ops {
    public:
        using return_value = ef_return_value;
        static constexpr std::size_t npos = ef_npos;
        static constexpr std::uint64_t novalue = ef_novalue;

        // sequential decode without a fresh select1 per step. Only the
        // first dereference (or a jump to pos != 0) pays a select1 search. Every
        // ++ resumes the forward word-scan from where the last left off.
        //
        // The forward scan is unbounded in how many all-zero words it may
        // cross, so on pathologically sparse data a single ++ can cost more than
        // a from scratch select1.
        class iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = std::uint64_t;
            using reference = std::uint64_t;
            using pointer = void;

            iterator() noexcept = default;
            iterator(const Derived* e, std::size_t pos) noexcept : m_ef(e), m_pos(pos) {}

            [[nodiscard]] reference operator*() const noexcept {
                ensure_cursor();
                return m_ef->decode_at(m_bit_cursor, m_pos);
            }

            iterator& operator++() noexcept {
                ensure_cursor();
                ++m_pos;
                if (m_pos < m_ef->size()) {
                    if constexpr (Derived::endian == pack_endian::lsb) {
                        // Streaming scan: consume the current set bit and
                        // take the next via ctz, reloading only when the word
                        // empties.
                        // Breaks the per-step memory dependency ef_find_next_set_bit has
                        m_word &= (m_word - 1);
                        const std::uint64_t* d = m_ef->high_data();
                        while (m_word == 0)
                            m_word = d[++m_word_idx];
                        m_bit_cursor =
                            (m_word_idx << 6) + static_cast<std::size_t>(std::countr_zero(m_word));
                    } else {
                        m_bit_cursor = ef_find_next_set_bit<Derived::endian>(m_ef->high_data(),
                                                                             m_bit_cursor + 1);
                    }
                }
                return *this;
            }

            iterator operator++(int) noexcept {
                iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            [[nodiscard]] friend bool operator==(const iterator& a, const iterator& b) noexcept {
                return a.m_ef == b.m_ef && a.m_pos == b.m_pos;
            }

            [[nodiscard]] std::size_t position() const noexcept {
                return m_pos;
            }

        private:
            void ensure_cursor() const noexcept {
                if (!m_cursor_valid) {
                    m_bit_cursor = m_ef->select().select1(
                        m_pos); // real search, paid once (first deref or after a jump), not per ++
                    if constexpr (Derived::endian == pack_endian::lsb) {
                        // mask off bits below the current one so its lowest set bit is
                        // m_bit_cursor.
                        m_word_idx = m_bit_cursor >> 6;
                        m_word = m_ef->high_data()[m_word_idx] &
                                 (~std::uint64_t{0} << (m_bit_cursor & 63));
                    }
                    m_cursor_valid = true;
                }
            }

            const Derived* m_ef = nullptr;
            std::size_t m_pos = 0;
            mutable std::size_t m_bit_cursor = 0;
            mutable std::uint64_t m_word =
                0; // lsb streaming: current high word, consumed bits cleared
            mutable std::size_t m_word_idx = 0; // lsb streaming: index of m_word in high bits
            mutable bool m_cursor_valid = false;
        };

        static_assert(std::forward_iterator<iterator>,
                      "ef_ops::iterator must satisfy std::forward_iterator");

        // Precondition: i < size().
        [[nodiscard]] std::uint64_t operator[](std::size_t i) const noexcept {
            return decode_at(derived().select().select1(i), i);
        }

        [[nodiscard]] std::uint64_t at(std::size_t i) const noexcept {
            return (*this)[i];
        }

        [[nodiscard]] std::uint64_t select1(std::size_t i) const noexcept {
            return (*this)[i];
        }

        // Decode the value at logical index `pos` from its high-bit position
        // `bit_cursor`: the high part is (bit_cursor - pos), the low part is
        // low_width() bits packed at pos*low_width in low_data().
        //
        // Uses extract_fast (lsb, width <= 57), whose unaligned 8-byte read may
        // over-read the last element. Safe because ef low_data() has a spare
        // trailing word.
        [[nodiscard]] std::uint64_t decode_at(std::size_t bit_cursor,
                                              std::size_t pos) const noexcept {
            const auto high = static_cast<std::uint64_t>(bit_cursor - pos);
            const std::size_t w = derived().low_width();
            if (w == 0)
                return high; // high << 0

            using word_type = typename Derived::word_type;
            const word_type* low = derived().low_data();
            const std::size_t bit_pos = pos * w;
            std::uint64_t lo;
            if constexpr (Derived::endian == pack_endian::lsb) {
                if (w <= 57) [[likely]]
                    lo = bit_ops<word_type, Derived::endian>::extract_fast(
                        low, bit_pos, static_cast<std::uint8_t>(w));
                else
                    lo = bit_ops<word_type, Derived::endian>::extract(low, bit_pos,
                                                                      static_cast<std::uint8_t>(w));
            } else {
                lo = bit_ops<word_type, Derived::endian>::extract(low, bit_pos,
                                                                  static_cast<std::uint8_t>(w));
            }
            return (high << w) | lo;
        }

        [[nodiscard]] std::uint64_t back() const noexcept {
            return (*this)[derived().size() - 1];
        }

        // Difference between consecutive elements (recovers the original delta
        // when built via ef::from_deltas).
        // Precondition: i < size() - 1.
        [[nodiscard]] std::uint64_t diff(std::size_t i) const noexcept {
            return derived()[i + 1] - derived()[i];
        }

        // Number of stored values strictly less than k.
        [[nodiscard]] std::size_t rank(std::uint64_t k) const noexcept {
            std::size_t lo = 0;
            std::size_t hi = derived().size();
            while (lo < hi) {
                const std::size_t mid = lo + (hi - lo) / 2;
                if (derived()[mid] < k)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            return lo;
        }

        // Leftmost [position, value] with value >= x. Saturates to
        // [size()-1, back()] if x > back(). Sentinel if empty.
        [[nodiscard]] return_value nge(std::uint64_t x) const noexcept {
            const std::size_t n = derived().size();
            if (n == 0)
                return {npos, novalue};
            const std::uint64_t b = derived().back();
            if (x > b)
                return {n - 1, b};

            if constexpr (Derived::index_zeros) {
                const std::uint64_t h_x = x >> derived().low_width();
                std::uint64_t p = 0;
                std::uint64_t begin = 0;
                if (h_x > 0) {
                    p = derived().select().select0(h_x - 1);
                    begin = p - h_x + 1;
                }

                std::size_t bit_cursor = ef_find_next_set_bit<Derived::endian>(
                    derived().high_data(), static_cast<std::size_t>(p));
                std::size_t pos = static_cast<std::size_t>(begin);
                std::uint64_t val = derived().decode_at(bit_cursor, pos);

                while (val < x) {
                    ++pos;
                    bit_cursor = ef_find_next_set_bit<Derived::endian>(derived().high_data(),
                                                                       bit_cursor + 1);
                    val = derived().decode_at(bit_cursor, pos);
                }
                return {pos, val};
            } else {
                const std::size_t pos = rank(x);
                return {pos, derived()[pos]};
            }
        }

        // Rightmost [position, value] with value <= x. Saturates to
        // [size()-1, back()] if x >= back(). Sentinel if x < front() or
        // empty.
        [[nodiscard]] return_value ple(std::uint64_t x) const noexcept {
            const std::size_t n = derived().size();
            if (n == 0)
                return {npos, novalue};
            const std::uint64_t b = derived().back();
            if (x >= b)
                return {n - 1, b};

            if constexpr (Derived::index_zeros) {
                if (x == novalue)
                    return {n - 1, b};

                const auto lf = leapfrog(x + 1);

                if (lf.has_prev)
                    return {lf.prev_pos, lf.prev_val};

                if (lf.pos == 0)
                    return {npos, novalue};

                const std::size_t pos = lf.pos - 1;
                if constexpr (Derived::endian == pack_endian::lsb) {
                    // scan back one set bit from lf cursor instead of a fresh select1
                    const std::size_t prev_bit =
                        ef_find_prev_set_bit_lsb(derived().high_data(), lf.bit_cursor - 1);
                    return {pos, derived().decode_at(prev_bit, pos)};
                } else {
                    return {pos, derived()[pos]};
                }
            } else {
                const std::uint64_t count_leq = (x == novalue) ? n : rank(x + 1);
                if (count_leq == 0)
                    return {npos, novalue};
                const std::size_t pos = static_cast<std::size_t>(count_leq) - 1;
                return {pos, derived()[pos]};
            }
        }

        // [lo, hi] such that lo.val <= x < hi.val (the pair bracketing x).
        [[nodiscard]] std::pair<return_value, return_value> locate(std::uint64_t x) const noexcept {
            const return_value lo = ple(x);
            return_value hi{npos, novalue};
            const std::size_t n = derived().size();
            if (lo.pos != npos && lo.pos + 1 < n)
                hi = {lo.pos + 1, derived()[lo.pos + 1]};
            else if (lo.pos == npos && n > 0)
                hi = {0, derived()[0]};
            return {lo, hi};
        }

        [[nodiscard]] iterator get_iterator_at(std::size_t pos) const noexcept {
            return iterator(&derived(), pos);
        }
        [[nodiscard]] iterator begin() const noexcept {
            return get_iterator_at(0);
        }
        [[nodiscard]] iterator end() const noexcept {
            return get_iterator_at(derived().size());
        }

    private:
        struct leapfrog_result {
            std::size_t pos;
            std::uint64_t val;
            bool has_prev;
            std::size_t prev_pos;
            std::uint64_t prev_val;
            std::size_t bit_cursor; // high-bit position of pos, lets ple scan back one bit
        };

        // Shared by nge/ple (index_zeros path): leftmost [pos, val] with val >= x,
        // also recording the preceding [prev_pos, prev_val] the scan stepped over.
        [[nodiscard]] leapfrog_result leapfrog(std::uint64_t x) const noexcept {
            const std::uint64_t h_x = x >> derived().low_width();
            std::uint64_t p = 0;
            std::uint64_t begin = 0;
            if (h_x > 0) {
                p = derived().select().select0(h_x - 1);
                begin = p - h_x + 1;
            }

            std::size_t bit_cursor = ef_find_next_set_bit<Derived::endian>(
                derived().high_data(), static_cast<std::size_t>(p));
            std::size_t pos = static_cast<std::size_t>(begin);
            std::uint64_t val = derived().decode_at(bit_cursor, pos);

            bool has_prev = false;
            std::size_t prev_pos = 0;
            std::uint64_t prev_val = 0;

            while (val < x) {
                has_prev = true;
                prev_pos = pos;
                prev_val = val;

                ++pos;
                bit_cursor =
                    ef_find_next_set_bit<Derived::endian>(derived().high_data(), bit_cursor + 1);
                val = derived().decode_at(bit_cursor, pos);
            }

            return {pos, val, has_prev, prev_pos, prev_val, bit_cursor};
        }

        [[nodiscard]] const Derived& derived() const noexcept {
            return static_cast<const Derived&>(*this);
        }
    };

}

namespace cds {

    template <typename Word, pack_endian Endian, pack_mode Mode, typename SelectViewImpl>
    class ef_view;

    // ef: owning, self-contained quasi-succinct representation of a
    // non-decreasing integer sequence, with random access, rank, successor /
    // predecessor (nge/ple), locate, and fast sequential decode. SelectImpl is a
    // swappable template parameter (default `darray` over the high-bits vector).
    template <typename Word = std::uint64_t, pack_endian Endian = pack_endian::lsb,
              pack_mode Mode = pack_mode::dense, bool IndexZeros = true,
              typename SelectImpl = darray<bit_vector<Word, Endian>,
                                           IndexZeros ? select_target::both : select_target::ones>>
    class ef : public detail::ef_ops<ef<Word, Endian, Mode, IndexZeros, SelectImpl>> {
    public:
        using high_bits_type = bit_vector<Word, Endian>;
        using high_select_type = SelectImpl;
        using low_bits_type = packed_vector<Word, std::uint64_t, 0, Endian, Mode>;
        using return_value = typename detail::ef_ops<ef>::return_value;
        using iterator = typename detail::ef_ops<ef>::iterator;
        using word_type = Word;

        static constexpr pack_endian endian = Endian;
        static constexpr bool index_zeros = IndexZeros;

        static_assert(select_over_source<high_select_type, high_bits_type>,
                      "ef: SelectImpl must satisfy select1_structure and be constructible directly "
                      "from `const high_bits_type&`.");
        static_assert(!IndexZeros || select01_over_source<high_select_type, high_bits_type>,
                      "ef: IndexZeros=true requires SelectImpl to also satisfy select0_structure "
                      "(e.g. darray<..., select_target::both>).");

        // A constructed ef always keeps an (possibly empty) select index
        ef() {
            m_select.emplace(m_high);
        }

        // `universe`: explicit upper bound (all values must be < universe).
        // Pass 0 (default) to infer it as values.back() + 1.
        explicit ef(std::span<const std::uint64_t> values, std::uint64_t universe = 0)
            : m_size(values.size()) {
            if (m_size == 0) {
                m_universe = universe;
                m_select.emplace(m_high); // empty select over empty high bits
                return;
            }

            m_universe = (universe != 0) ? universe : (values.back() + 1);
            CDS_ASSERT(m_universe > values.back(),
                       "ef: universe ({}) must exceed the largest value ({})", m_universe,
                       values.back());

            build(values);
        }

        // Builds from an unsorted sequence of non-negative deltas,
        // internally prefix-summing them (with a leading zero). Result
        // has size() == deltas.size() + 1
        // element 0 is always the implicit leading zero.
        // diff(i) for i in [0, deltas.size()) recovers the i-th original delta.
        [[nodiscard]] static ef from_deltas(std::span<const std::uint64_t> deltas,
                                            std::uint64_t universe = 0) {
            std::vector<std::uint64_t> prefixed;
            prefixed.reserve(deltas.size() + 1);
            prefixed.push_back(0);
            std::uint64_t running = 0;
            for (const std::uint64_t d : deltas) {
                running += d;
                prefixed.push_back(running);
            }

            ef e;
            e.m_size = prefixed.size();
            e.m_universe = (universe != 0) ? universe : (running + 1);
            CDS_ASSERT(e.m_universe > running,
                       "ef::from_deltas: universe ({}) must exceed the largest prefix sum ({})",
                       e.m_universe, running);
            e.build(prefixed);
            return e;
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }

        [[nodiscard]] std::uint64_t universe() const noexcept {
            return m_universe;
        }

        [[nodiscard]] std::size_t low_width() const noexcept {
            return m_low_width;
        }

        [[nodiscard]] std::uint64_t back() const noexcept {
            return m_back;
        }

        [[nodiscard]] auto as_view() const noexcept;

        [[nodiscard]] const high_select_type& select() const noexcept {
            return *m_select;
        }

        [[nodiscard]] const Word* low_data() const noexcept {
            return m_low.data();
        }

        [[nodiscard]] const std::uint64_t* high_data() const noexcept {
            return m_high.data();
        }

        [[nodiscard]] std::size_t overhead_bits() const noexcept {
            std::size_t bits = m_low.nb_words() * 64 + m_high.size();
            if constexpr (requires(const high_select_type& s) {
                              { s.overhead_bits() } -> std::convertible_to<std::size_t>;
                          }) {
                if (m_select)
                    bits += m_select->overhead_bits();
            }
            return bits;
        }

        [[nodiscard]] std::size_t memory_size() const noexcept {
            std::size_t bytes =
                sizeof(ef) + m_low.nb_words() * sizeof(Word) + m_high.nb_words() * sizeof(Word);
            if constexpr (requires(const high_select_type& s) {
                              { s.memory_size() } -> std::convertible_to<std::size_t>;
                          }) {
                if (m_select)
                    bytes += m_select->memory_size() - sizeof(high_select_type);
            }
            return bytes;
        }

        template <typename Sink>
            requires io::byte_sink<Sink>
        [[nodiscard]] bool save(Sink& sink) const noexcept {
            if (!io::write_cds_version(sink))
                return false;

            const detail::ef_header h{
                detail::ef_magic, 0,      static_cast<std::uint64_t>(m_size),
                m_universe,       m_back, static_cast<std::uint64_t>(m_low_width)};
            if (!sink.write(&h, sizeof(h)))
                return false;

            return m_low.save(sink) && m_high.save(sink) && m_select->save(sink);
        }

        template <typename Source>
            requires io::byte_source<Source>
        [[nodiscard]] static std::expected<ef, io::load_error> load(Source& source) {
            const auto version = io::read_cds_version_compatible(source);
            if (!version)
                return std::unexpected(version.error());

            detail::ef_header h{};
            if (!source.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::ef_magic)
                return std::unexpected(io::load_error::bad_magic);

            ef e;
            e.m_size = static_cast<std::size_t>(h.size);
            e.m_universe = h.universe;
            e.m_back = h.back;
            e.m_low_width = static_cast<std::size_t>(h.low_width);

            auto low = low_bits_type::load(source);
            if (!low)
                return std::unexpected(low.error());
            e.m_low = std::move(*low);
            if (e.m_low_width > 0)
                e.m_low.reserve(e.m_size +
                                64); // restore the spare trailing word extract_fast relies on

            auto high = high_bits_type::load(source);
            if (!high)
                return std::unexpected(high.error());
            e.m_high = std::move(*high);

            auto sel = high_select_type::load(source, e.m_high);
            if (!sel)
                return std::unexpected(sel.error());
            e.m_select.emplace(std::move(*sel));
            return e;
        }

    private:
        void build(std::span<const std::uint64_t> values) {
            m_low_width = compute_low_width(m_size, m_universe);
            m_low =
                low_bits_type(sc<std::uint8_t>(m_low_width > 0 ? m_low_width : 1), Endian, Mode);

            const std::uint64_t last_high = (m_size > 0) ? (values[m_size - 1] >> m_low_width) : 0;
            const std::size_t total_high_bits = static_cast<std::size_t>(last_high) + m_size;

            if (m_low_width > 0) {
                // +1 spare word so decode_at unaligned 64-bit read never
                // runs off the end (see decode_at)
                // 64 extra elements is at least one Word regardless of width.
                m_low.reserve(m_size + 64);
                const std::uint64_t low_mask = (m_low_width == 64)
                                                   ? ~std::uint64_t{0}
                                                   : ((std::uint64_t{1} << m_low_width) - 1);
                for (std::size_t i = 0; i < m_size; ++i)
                    m_low.push_back(values[i] & low_mask);
            }

            bit_vector_builder<Word, Endian, true> hb(total_high_bits);
            for (std::size_t i = 0; i < m_size; ++i) {
                const std::size_t pos = static_cast<std::size_t>(values[i] >> m_low_width) + i;
                hb.set_bit(pos);
            }
            m_high =
                high_bits_type(std::move(hb)); // steal the builder buffer

            m_select.emplace(m_high);
            m_back = values[m_size - 1];
        }

        [[nodiscard]] static std::size_t compute_low_width(std::size_t n,
                                                           std::uint64_t u) noexcept {
            if (n == 0)
                return 0;
            const std::uint64_t ratio = u / static_cast<std::uint64_t>(n);
            if (ratio == 0)
                return 0;
            return static_cast<std::size_t>(std::bit_width(ratio)) - 1;
        }

        std::size_t m_size = 0;
        std::uint64_t m_universe = 0;
        std::uint64_t m_back = 0;
        std::size_t m_low_width = 0;
        low_bits_type m_low{std::uint8_t{1}, Endian, Mode};
        high_bits_type m_high;
        std::optional<high_select_type> m_select;
    };

    // ef_view: non-owning
    template <typename Word = std::uint64_t, pack_endian Endian = pack_endian::lsb,
              pack_mode Mode = pack_mode::dense,
              typename SelectViewImpl =
                  darray_view<const_bit_view_impl<Word, Endian>, select_target::both>>
    class ef_view : public detail::ef_ops<ef_view<Word, Endian, Mode, SelectViewImpl>> {
    public:
        using return_value = typename detail::ef_ops<ef_view>::return_value;
        using iterator = typename detail::ef_ops<ef_view>::iterator;
        using low_view_type = const_packed_view<Word, std::uint64_t, 0, Endian, Mode>;
        using high_view_type = const_bit_view_impl<Word, Endian>;
        using word_type = Word;

        static constexpr pack_endian endian = Endian;
        static constexpr bool index_zeros = select0_structure<SelectViewImpl>;

        static_assert(select1_structure<SelectViewImpl>,
                      "ef_view: SelectViewImpl must satisfy select1_structure");

        ef_view(std::span<const Word> low_words, SelectViewImpl select_view, std::size_t size,
                std::uint64_t universe, std::uint8_t low_width, std::uint64_t back) noexcept
            : m_low_words(low_words), m_select(select_view), m_size(size), m_universe(universe),
              m_back(back), m_low_width(low_width) {}

        [[nodiscard]] std::size_t size() const noexcept {
            return m_size;
        }
        [[nodiscard]] std::uint64_t universe() const noexcept {
            return m_universe;
        }
        [[nodiscard]] std::size_t low_width() const noexcept {
            return m_low_width;
        }

        [[nodiscard]] std::uint64_t back() const noexcept {
            return m_back;
        }

        [[nodiscard]] const SelectViewImpl& select() const noexcept {
            return m_select;
        }
        [[nodiscard]] const Word* low_data() const noexcept {
            return m_low_words.data();
        }
        [[nodiscard]] const std::uint64_t* high_data() const noexcept {
            return m_select.data();
        }

        template <typename ByteSource>
            requires io::span_source<ByteSource>
        [[nodiscard]] static std::expected<ef_view, io::load_error>
        load(ByteSource& reader) noexcept {
            const auto version = io::read_cds_version_compatible(reader);
            if (!version)
                return std::unexpected(version.error());

            detail::ef_header h{};
            if (!reader.read(&h, sizeof(h)))
                return std::unexpected(io::load_error::io_failure);
            if (h.magic != detail::ef_magic)
                return std::unexpected(io::load_error::bad_magic);

            auto low = low_view_type::load(reader);
            if (!low)
                return std::unexpected(low.error());
            const std::span<const Word> low_words(low->data(), low->nb_words());

            auto high = high_view_type::load(reader);
            if (!high)
                return std::unexpected(high.error());

            auto select = SelectViewImpl::load(reader, *high);
            if (!select)
                return std::unexpected(select.error());

            return ef_view(low_words, std::move(*select), static_cast<std::size_t>(h.size),
                           h.universe, static_cast<std::uint8_t>(h.low_width), h.back);
        }

    private:
        std::span<const Word> m_low_words;
        SelectViewImpl m_select;
        std::size_t m_size;
        std::uint64_t m_universe;
        std::uint64_t m_back{0};
        std::uint8_t m_low_width;
    };

    template <typename Word, pack_endian Endian, pack_mode Mode, bool IndexZeros,
              typename SelectImpl>
    auto ef<Word, Endian, Mode, IndexZeros, SelectImpl>::as_view() const noexcept {
        return ef_view<Word, Endian, Mode, decltype(m_select->as_view())>(
            std::span<const Word>(m_low.data(), m_low.nb_words()), m_select->as_view(), m_size,
            m_universe, static_cast<std::uint8_t>(m_low_width), m_back);
    }

}
