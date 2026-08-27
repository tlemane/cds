add_library(nanobench OBJECT nanobench.cpp)
target_include_directories(nanobench PUBLIC ${CMAKE_SOURCE_DIR}/ext)

function(cds_add_benchmark name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE cds nanobench cds_dev_flags)
endfunction()

cds_add_benchmark(bench_packed packed/packed.cpp)
cds_add_benchmark(bench_bv packed/bv.cpp)
cds_add_benchmark(bench_rank rank/rank.cpp)
cds_add_benchmark(bench_select select/select.cpp)

function(cds_add_sdsl_benchmark name source)
    cds_add_benchmark(${name} ${source})
    target_link_libraries(${name} PRIVATE sdsl-lite)
endfunction()

cds_add_sdsl_benchmark(bench_sdsl_packed sdsl/packed.cpp)
cds_add_sdsl_benchmark(bench_sdsl_bv     sdsl/bv.cpp)
cds_add_sdsl_benchmark(bench_sdsl_rank   sdsl/rank.cpp)
cds_add_sdsl_benchmark(bench_sdsl_select sdsl/select.cpp)
cds_add_sdsl_benchmark(bench_sdsl_ef     sdsl/ef.cpp)
cds_add_sdsl_benchmark(bench_sdsl_rrr    sdsl/rrr.cpp)
cds_add_sdsl_benchmark(bench_sdsl_wavelet sdsl/wavelet.cpp)

add_executable(cds-perf cds-perf.cpp)

target_include_directories(cds-perf PRIVATE ${CMAKE_SOURCE_DIR}/ext)
target_link_libraries(cds-perf PRIVATE cds::cds)

target_compile_features(cds-perf PRIVATE cxx_std_23)
target_compile_options(cds-perf PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native>
    $<$<CXX_COMPILER_ID:MSVC>:/O2>
)

target_link_libraries(cds-perf PRIVATE sdsl-lite)
