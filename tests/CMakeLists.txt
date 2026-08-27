set(CDS_DOCTEST_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ext CACHE PATH "Path to vendored doctest.h")

if(NOT EXISTS "${CDS_DOCTEST_INCLUDE_DIR}/doctest.h")
    message(FATAL_ERROR
        "doctest.h not found at ${CDS_DOCTEST_INCLUDE_DIR}. "
        "Set -DCDS_DOCTEST_INCLUDE_DIR=<path> to the directory containing it."
    )
endif()

add_library(cds_doctest_main OBJECT doctest_main.cpp)
target_include_directories(cds_doctest_main PRIVATE ${CDS_DOCTEST_INCLUDE_DIR})
target_link_libraries(cds_doctest_main PRIVATE cds_dev_flags)

function(cds_add_test name)
    add_executable(${name} ${ARGN} $<TARGET_OBJECTS:cds_doctest_main>)

    target_include_directories(${name} PRIVATE
        ${CDS_DOCTEST_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(${name} PRIVATE cds::cds cds_dev_flags)

    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS "cds")
endfunction()

cds_add_test(cds_core           core/cds_core.cpp)
cds_add_test(cds_packed_array   packed/cds_packed_array.cpp)
cds_add_test(cds_packed_vector  packed/cds_packed_vector.cpp)
cds_add_test(cds_packed_view    packed/cds_packed_view.cpp)
cds_add_test(cds_bit            bit/cds_bit.cpp)
cds_add_test(cds_io             io/cds_io.cpp)
cds_add_test(cds_rank           rank/cds_rank.cpp)
cds_add_test(cds_select         select/cds_select.cpp)
cds_add_test(cds_rrr            cds_rrr.cpp)
cds_add_test(cds_ef             cds_ef.cpp)
cds_add_test(cds_bit_dict       cds_bit_dict.cpp)
cds_add_test(cds_wavelet_matrix cds_wavelet_matrix.cpp)


