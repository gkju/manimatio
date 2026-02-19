cmake_minimum_required(VERSION 3.28)
include(${CMAKE_SOURCE_DIR}/cmake/AddCxxModuleLibraryAuto.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/AddCxxTests.cmake)

add_cxx_module_library_auto(cxxlib_gfx ${CMAKE_CURRENT_LIST_DIR})
set_property(GLOBAL APPEND PROPERTY AUTO_CXXLIB_TARGETS cxxlib_gfx)

target_link_libraries(cxxlib_gfx PUBLIC msdfgen::msdfgen)

add_cxx_tests(cxxlib_gfx ${CMAKE_CURRENT_LIST_DIR})