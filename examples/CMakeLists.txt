file(GLOB CDS_EXAMPLE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(example_src ${CDS_EXAMPLE_SOURCES})
    get_filename_component(example_name "${example_src}" NAME_WE)
    add_executable("example_${example_name}" "${example_src}")
    target_link_libraries("example_${example_name}" PRIVATE cds::cds)
    target_compile_features("example_${example_name}" PRIVATE cxx_std_23)
endforeach()
