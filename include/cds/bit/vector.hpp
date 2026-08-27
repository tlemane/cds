#pragma once

#include <expected>
#include <memory>
#include <utility>
#include <cds/io/byte.hpp>
#include <cds/bit/interface.hpp>
#include <cds/bit/view.hpp>

namespace cds {
    template <typename Word, pack_endian Endian, typename Allocator = std::allocator<Word>>
    class bit_vector : public bit_vector_impl<Word, Endian, Allocator>,
                       public bit_mutation_ops<bit_vector<Word, Endian, Allocator>> {
    public:
        using impl = bit_vector_impl<Word, Endian, Allocator>;
        using impl::clear;
        using impl::impl;
        using bit_mutation_ops<bit_vector<Word, Endian, Allocator>>::clear;

        using bit_view_type = bit_view<Word, Endian>;
        using const_bit_view_type = const_bit_view<Word, Endian>;

        bit_vector() = default;

        bit_vector(impl&& base) noexcept : impl(std::move(base)) {}

        template <typename Source>
        [[nodiscard]] static std::expected<bit_vector, io::load_error> load(Source& source) {
            auto base_result = impl::load(source);
            if (!base_result)
                return std::unexpected(base_result.error());
            return bit_vector(std::move(*base_result));
        }

        [[nodiscard]] bit_view_type as_view() noexcept {
            return bit_view_type{impl::as_view()};
        }

        [[nodiscard]] const_bit_view_type as_const_view() const noexcept {
            return const_bit_view_type{impl::as_const_view()};
        }
    };

    template <typename Word, pack_endian Endian, typename Allocator>
    struct bit_source_traits<bit_vector<Word, Endian, Allocator>> {
        using source_type = bit_vector<Word, Endian, Allocator>;
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
