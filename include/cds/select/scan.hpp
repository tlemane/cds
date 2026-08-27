#pragma once

#include <bit>
#include <cstddef>
#include <limits>

#include <cds/bit/interface.hpp>
#include <cds/core/broadword.hpp>
#include <cds/core/debug.hpp>
#include <cds/rank/concepts.hpp>

namespace cds {

    // select_scan: select with no index, scanning the source words at query
    // time (zero extra space).
    template <bit_source Source> class select_scan {
    public:
        using source_type = Source;
        using word_type = typename bit_source_traits<Source>::word_type;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;
        static constexpr std::size_t digits = std::numeric_limits<word_type>::digits;

        select_scan() noexcept = default;
        explicit select_scan(const source_type& source) noexcept : m_source(&source) {}

        // Precondition: index < number of set bits.
        [[nodiscard]] std::size_t select1(std::size_t index) const noexcept {
            return scan<true>(index);
        }

        // Precondition: index < number of unset bits.
        [[nodiscard]] std::size_t select0(std::size_t index) const noexcept {
            return scan<false>(index);
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return bit_source_traits<Source>::size(*m_source);
        }

        [[nodiscard]] constexpr std::size_t memory_size() const noexcept {
            return sizeof(select_scan);
        }

    private:
        template <bool Ones> [[nodiscard]] std::size_t scan(std::size_t index) const noexcept {
            const word_type* data = bit_source_traits<Source>::data(*m_source);
            const std::size_t n = bit_source_traits<Source>::size(*m_source);
            const std::size_t num_words = (n + digits - 1) / digits;

            std::size_t remaining = index;
            for (std::size_t w = 0; w < num_words; ++w) {
                word_type word = data[w];
                if constexpr (!Ones) {
                    word = static_cast<word_type>(~word);
                }

                if (w == num_words - 1) {
                    const std::size_t valid_bits = n - w * digits;
                    if (valid_bits < digits) {
                        if constexpr (endian == pack_endian::lsb) {
                            const word_type valid_mask =
                                static_cast<word_type>((word_type{1} << valid_bits) - 1);
                            word &= valid_mask;
                        } else {
                            const word_type valid_mask = static_cast<word_type>(
                                ~((word_type{1} << (digits - valid_bits)) - 1));
                            word &= valid_mask;
                        }
                    }
                }

                const auto c = static_cast<std::size_t>(std::popcount(word));
                if (remaining < c) {
                    return w * digits + broadword::select_in_word<endian>(word, remaining);
                }

                remaining -= c;
            }

            CDS_ASSERT(false, "select_scan::select{}: index {} out of range", Ones ? 1 : 0, index);
            return n; // unreachable
        }

        const source_type* m_source{nullptr};
    };

} // namespace cds
