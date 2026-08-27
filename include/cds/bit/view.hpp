#pragma once

#include <expected>
#include <utility>
#include <cds/bit/interface.hpp>

namespace cds {

    template <typename Word, pack_endian Endian>
    class bit_view : public bit_view_impl<Word, Endian>,
                     public bit_mutation_ops<bit_view<Word, Endian>> {
    public:
        using impl = bit_view_impl<Word, Endian>;
        using impl::impl;
        using bit_mutation_ops<bit_view<Word, Endian>>::clear;

        bit_view(impl&& base) noexcept : impl(std::move(base)) {}

        template <typename Source>
            requires io::mutable_span_source<Source>
        [[nodiscard]] static std::expected<bit_view, io::load_error> load(Source& source) noexcept {
            auto base_result = impl::load(source);
            if (!base_result)
                return std::unexpected(base_result.error());
            return bit_view(std::move(*base_result));
        }
    };

    template <typename Word, pack_endian Endian>
    class const_bit_view : public const_bit_view_impl<Word, Endian> {
    public:
        using impl = const_bit_view_impl<Word, Endian>;
        using impl::impl;

        const_bit_view(impl&& base) noexcept : impl(std::move(base)) {}

        template <typename Source>
            requires io::span_source<Source>
        [[nodiscard]] static std::expected<const_bit_view, io::load_error>
        load(Source& source) noexcept {
            auto base_result = impl::load(source);
            if (!base_result)
                return std::unexpected(base_result.error());
            return const_bit_view(std::move(*base_result));
        }
    };

    template <typename Word, pack_endian Endian> struct bit_source_traits<bit_view<Word, Endian>> {
        using source_type = bit_view<Word, Endian>;
        using word_type = Word;
        static constexpr pack_endian endian = Endian;
        static constexpr pack_mode mode = pack_mode::dense;
        static const Word* data(const source_type& s) noexcept {
            return s.data();
        }
        static std::size_t size(const source_type& s) noexcept {
            return s.size();
        }
        static std::size_t offset(const source_type& s) noexcept {
            return s.offset();
        }
    };

    template <typename Word, pack_endian Endian>
    struct bit_source_traits<const_bit_view<Word, Endian>> {
        using source_type = const_bit_view<Word, Endian>;
        using word_type = Word;
        static constexpr pack_endian endian = Endian;
        static constexpr pack_mode mode = pack_mode::dense;
        static const Word* data(const source_type& s) noexcept {
            return s.data();
        }
        static std::size_t size(const source_type& s) noexcept {
            return s.size();
        }
        static std::size_t offset(const source_type& s) noexcept {
            return s.offset();
        }
    };

}
