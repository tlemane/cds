#pragma once

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <format>
#include <print>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>

namespace cds {

    inline std::string get_source_filename(const std::source_location& loc) {
        std::string p = loc.file_name();

        constexpr std::string_view root = "cds";
        const auto pos = p.rfind(root);

        return pos != std::string::npos ? p.substr(pos) : p;
    }

    inline void cds_assert(bool condition, const std::source_location& loc, const char* expression,
                           const std::string& message) noexcept {
        if (!condition) {
            constexpr const char* format = "Assertion [ {0} ] failed at {2}:L{3}\n    -> {1}\n";
            std::print(stderr, format, expression, message, get_source_filename(loc), loc.line());
            std::abort();
        };
    }

    inline void panic(const std::source_location& loc, const char* expression,
                      const std::string& message) noexcept {
        constexpr const char* format = "Panicked with [ {0} ] at {2}:L{3}\n    -> {1}\n";
        std::print(stderr, format, expression, message, get_source_filename(loc), loc.line());
        std::terminate();
    }

    inline void panic(bool condition, const std::source_location& loc, const char* expression,
                      const std::string& message) noexcept {
        if (!condition) {
            panic(loc, expression, message);
        };
    }

}

#if defined(NDEBUG) && !defined(CDS_FORCE_ASSERT)
#define CDS_ASSERT(expression, message, ...) ((void)0)
#else
// is_constant_evaluated: during constant evaluation the failure branch is never evaluated, so a
// constexpr function using CDS_ASSERT stays usable
#define CDS_ASSERT(expression, message, ...)                                                       \
    (((expression) || std::is_constant_evaluated())                                                \
         ? (void)0                                                                                 \
         : cds::cds_assert(false, std::source_location::current(), #expression,                    \
                           std::format(message __VA_OPT__(, ) __VA_ARGS__)))
#endif

#define CDS_PANIC(expression, message, ...)                                                        \
    cds::panic((expression), std::source_location::current(), #expression,                         \
               std::format(message __VA_OPT__(, ) __VA_ARGS__))
