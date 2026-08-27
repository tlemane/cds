#pragma once

#include <bit>
#include <cstddef>
#include <limits>

#include <cds/bit/interface.hpp>
#include <cds/core/broadword.hpp>
#include <cds/rank/concepts.hpp>

namespace cds {

    // rank_scan: rank with no index, scanning the source words at query time
    template <bit_source Source> class rank_scan {
    public:
        using source_type = Source;
        using word_type = typename bit_source_traits<Source>::word_type;
        static constexpr pack_endian endian = bit_source_traits<Source>::endian;
        static constexpr std::size_t digits = std::numeric_limits<word_type>::digits;

        rank_scan() noexcept = default;
        explicit rank_scan(const source_type& source) noexcept : m_source(&source) {}

        // Number of set bits in [0, index). Precondition: index <= size().
        [[nodiscard]] std::size_t rank1(std::size_t index) const noexcept {
            const word_type* data = bit_source_traits<Source>::data(*m_source);
            const std::size_t full_words = index / digits;

            std::size_t count = 0;
            for (std::size_t w = 0; w < full_words; ++w)
                count += static_cast<std::size_t>(std::popcount(data[w]));

            const std::size_t remainder_bits = index - full_words * digits;
            if (remainder_bits > 0)
                count += broadword::popcount_below<endian>(data[full_words], remainder_bits);

            return count;
        }

        [[nodiscard]] std::size_t rank0(std::size_t index) const noexcept {
            return index - rank1(index);
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return bit_source_traits<Source>::size(*m_source);
        }

        [[nodiscard]] constexpr std::size_t memory_size() const noexcept {
            return sizeof(rank_scan);
        }

    private:
        const source_type* m_source{nullptr};
    };

} // namespace cds
