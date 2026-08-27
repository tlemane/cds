#pragma once

#include <atomic>
#include <filesystem>
#include <random>
#include <string>
#include <string_view>

namespace {

    [[nodiscard]] inline std::filesystem::path io_temp_path(std::string_view tag) {
        static std::atomic<unsigned> counter{0};
        std::random_device rd;
        std::string name = "cds_io_" + std::string(tag) + "_" + std::to_string(rd()) + "_" +
                           std::to_string(counter.fetch_add(1));
        return std::filesystem::temp_directory_path() / name;
    }

} // namespace
