cmake_minimum_required(VERSION 3.28)

project(cds
    VERSION 0.1.0
    DESCRIPTION "cds - compact data structures"
    HOMEPAGE_URL "https://github.com/tlemane/cds"
    LANGUAGES CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            "Debug" "Release" "RelWithDebInfo" "MinSizeRel"
        )
    endif()
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(CDS_BUILD_TESTS        "Build tests"                                   ${PROJECT_IS_TOP_LEVEL})
option(CDS_BUILD_BENCHMARKS   "Build benchmarks"                              ${PROJECT_IS_TOP_LEVEL})
option(CDS_BUILD_EXAMPLES     "Build the example programs"                    ${PROJECT_IS_TOP_LEVEL})
option(CDS_INSTALL            "Generate install target and package config"    ${PROJECT_IS_TOP_LEVEL})
option(CDS_ENABLE_SANITIZERS  "Build tests with ASan + UBSan"                 OFF)
option(CDS_WARNINGS_AS_ERRORS "Treat warnings as errors in dev builds"        OFF)

add_library(cds INTERFACE)
add_library(cds::cds ALIAS cds)

target_include_directories(cds INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(cds INTERFACE cxx_std_23)

target_compile_options(cds INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
)

add_library(cds_dev_flags INTERFACE)

target_compile_options(cds_dev_flags INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-O3>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU,Clang>>:-march=native>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/O2>
    $<$<AND:$<CONFIG:RelWithDebInfo>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-O2 -g>
    $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-O0 -g>
    $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:/Od /Zi>
)

if(CDS_WARNINGS_AS_ERRORS)
    target_compile_options(cds_dev_flags INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Werror>
        $<$<CXX_COMPILER_ID:MSVC>:/WX>
    )
endif()

if(CDS_ENABLE_SANITIZERS)
    target_compile_options(cds_dev_flags INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all>
    )
    target_link_options(cds_dev_flags INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fsanitize=address,undefined>
    )
endif()

if(CDS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (CDS_BUILD_BENCHMARKS)
    add_subdirectory(ext/sdsl)
    add_subdirectory(benchmarks)
endif()

if (CDS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(CDS_INSTALL)
    set(CDS_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/cds")

    install(TARGETS cds
        EXPORT cdsTargets
    )

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp"
    )

    install(EXPORT cdsTargets
        FILE cdsTargets.cmake
        NAMESPACE cds::
        DESTINATION ${CDS_INSTALL_CMAKEDIR}
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cdsConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/cdsConfig.cmake
        INSTALL_DESTINATION ${CDS_INSTALL_CMAKEDIR}
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/cdsConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/cdsConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cdsConfigVersion.cmake
        DESTINATION ${CDS_INSTALL_CMAKEDIR}
    )

    export(EXPORT cdsTargets
        FILE ${CMAKE_CURRENT_BINARY_DIR}/cdsTargets.cmake
        NAMESPACE cds::
    )
endif()

if(CDS_INSTALL AND PROJECT_IS_TOP_LEVEL)
    set(CPACK_PACKAGE_NAME "cds")
    set(CPACK_PACKAGE_VENDOR "tlemane")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
    set(CPACK_PACKAGE_FILE_NAME "cds-${PROJECT_VERSION}")
    set(CPACK_PACKAGE_CHECKSUM SHA256)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
        set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
        set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    endif()

    set(CPACK_GENERATOR "TGZ")
    if(WIN32)
        list(APPEND CPACK_GENERATOR "ZIP")
    endif()

    set(CPACK_SOURCE_GENERATOR "TGZ")
    set(CPACK_SOURCE_PACKAGE_FILE_NAME "cds-${PROJECT_VERSION}-src")
    set(CPACK_SOURCE_IGNORE_FILES
        "/\\\\.git/"
        "/\\\\.github/"
        "/build/"
        "/\\\\.direnv/"
        "\\\\.gitignore$"
    )

    include(CPack)
endif()

if(PROJECT_IS_TOP_LEVEL)
    find_program(CLANG_FORMAT_EXE
        NAMES clang-format clang-format-19 clang-format-18 clang-format-17 clang-format-16
    )

    if(CLANG_FORMAT_EXE)
        file(GLOB_RECURSE CDS_FORMAT_SOURCES
            CONFIGURE_DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/include/cds/*.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/include/cds/*/*.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*/*.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/*/*.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/*/*.hpp
        )

        add_custom_target(format
            COMMAND ${CLANG_FORMAT_EXE} -i --style=file ${CDS_FORMAT_SOURCES}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Formatting include/cds with clang-format"
            VERBATIM
        )

        add_custom_target(format-check
            COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror --style=file ${CDS_FORMAT_SOURCES}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Checking include/cds formatting"
            VERBATIM
        )
    else()
        message(STATUS "clang-format not found — `format` / `format-check` targets unavailable")
    endif()
endif()



