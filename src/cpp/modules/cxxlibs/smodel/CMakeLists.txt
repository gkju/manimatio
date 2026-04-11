cmake_minimum_required(VERSION 3.28)
include(${CMAKE_SOURCE_DIR}/cmake/AddCxxModuleLibraryAuto.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/AddCxxSnippets.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/AddCxxTests.cmake)


add_cxx_module_library_auto(cxxlib_smodel ${CMAKE_CURRENT_LIST_DIR})
add_cxx_snippets(cxxlib_smodel ${CMAKE_CURRENT_LIST_DIR})
add_cxx_tests(cxxlib_smodel ${CMAKE_CURRENT_LIST_DIR})