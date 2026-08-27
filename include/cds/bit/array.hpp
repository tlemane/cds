#pragma once

#include <expected>
#include <utility>
#include <cds/io/byte.hpp>
#include <cds/bit/interface.hpp>
#include <cds/bit/view.hpp>

namespace cds {

    template <typename Word, std::size_t Capacity, pack_endian Endian>
    class bit_array : public bit_array_impl<Word, Capacity, Endian>,
                      public bit_mutation_ops<bit_array<Word, Capacity, Endian>> {
    public:
        using impl = bit_array_impl<Word, Capacity, Endian>;
        using impl::clear;
        using impl::impl;
        using bit_mutation_ops<bit_array<Word, Capacity, Endian>>::clear;

        using bit_view_type = bit_view<Word, Endian>;
        using const_bit_view_type = const_bit_view<Word, Endian>;

        bit_array() = default;
        bit_array(impl&& base) noexcept : impl(std::move(base)) {}

        template <typename Source>
        [[nodiscard]] static std::expected<bit_array, io::load_error>
        load(Source& source) noexcept {
            auto base_result = impl::load(source);
            if (!base_result)
                return std::unexpected(base_result.error());
            return bit_array(std::move(*base_result));
        }

        [[nodiscard]] bit_view_type as_view() noexcept {
            return bit_view_type{impl::as_view()};
        }

        [[nodiscard]] const_bit_view_type as_const_view() const noexcept {
            return const_bit_view_type{impl::as_view()};
        }
    };

    template <typename Word, std::size_t Capacity, pack_endian Endian>
    struct bit_source_traits<bit_array<Word, Capacity, Endian>> {
        using source_type = bit_array<Word, Capacity, Endian>;
        using word_type = Word;
        static constexpr pack_endian endian = Endian;
        static constexpr pack_mode mode = pack_mode::dense;
        static const Word* data(const source_type& s) noexcept {
            return s.data();
        }
        static std::size_t size(const source_type& s) noexcept {
            return s.size();
        }
        static std::size_t offset(const source_type&) noexcept {
            return 0;
        }
    };

} // namespace cds
