cmake_minimum_required(VERSION 3.28)
include(${CMAKE_SOURCE_DIR}/cmake/AddCxxModuleLibraryAuto.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/AddCxxSnippets.cmake)

add_cxx_module_library_auto(cxxlib_math ${CMAKE_CURRENT_LIST_DIR})
set_property(GLOBAL APPEND PROPERTY AUTO_CXXLIB_TARGETS cxxlib_math)

add_cxx_snippets(cxxlib_math ${CMAKE_CURRENT_LIST_DIR})