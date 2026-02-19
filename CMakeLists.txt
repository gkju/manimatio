cmake_minimum_required(VERSION 3.28)

find_program(CLANG_SCAN_DEPS_PATH clang-scan-deps)
if(CLANG_SCAN_DEPS_PATH)
    set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS ${CLANG_SCAN_DEPS_PATH})
endif()

# -----------------------
# Project setup
# -----------------------
project(manimatio LANGUAGES C CXX)
find_package(msdfgen CONFIG REQUIRED)

option(ENABLE_LTO "Enable cross-language link-time optimization (LTO)" OFF)
option(ENABLE_CUDA_EXT "Enable CUDA extension" OFF)
option(ENABLE_PYBINDINGS "Enable nanobind bindings" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------
# Rust sources
# -----------------------
include(FetchContent)

FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.6
)
FetchContent_MakeAvailable(Corrosion)

corrosion_import_crate(MANIFEST_PATH ${CMAKE_SOURCE_DIR}/src/rust/Cargo.toml)

# CXX bridge
corrosion_add_cxxbridge(rust_bridge
  CRATE manimatio_rust_lib
  REGEN_TARGET regen_manimatio_rust_lib_cxxbridge
  FILES lib.rs
)

# -----------------------
# Catch2 setup
# -----------------------
enable_testing()
include(${CMAKE_SOURCE_DIR}/cmake/SetupCatch.cmake)
setup_catch()

# -----------------------
# C++ sources
# -----------------------
add_library(core STATIC
  src/cpp/core.cpp
)
target_link_libraries(core PUBLIC rust_bridge)

add_executable(app src/cpp/main.cpp)
target_link_libraries(app PRIVATE 
  core
  msdfgen::msdfgen
)

# -----------------------
# CXX modules
# -----------------------
include(${CMAKE_SOURCE_DIR}/cmake/LoadCxxLibs.cmake)
load_cxxlibs("${CMAKE_SOURCE_DIR}/src/cpp/modules/cxxlibs")

get_property(_cxxlibs GLOBAL PROPERTY AUTO_CXXLIB_TARGETS)
if(_cxxlibs)
  target_link_libraries(app PRIVATE ${_cxxlibs})
  target_compile_definitions(app PRIVATE HAS_CXXLIBS=1)
endif()

# -----------------------
# Optional CUDA nvcc library (separate sub-build)
# -----------------------
if(ENABLE_CUDA_EXT)
  if(APPLE)
    message(WARNING "CUDA requested but macOS is not supported here; disabling CUDA extension.")
  else()
    add_subdirectory(src/cpp/modules/cuda_ext/host_link)
    target_link_libraries(app PRIVATE cuda_ext_host)
    target_compile_definitions(app PRIVATE ENABLE_CUDA_EXT=1)
  endif()
endif()

# -----------------------
# Optional nanobind Python extension
# -----------------------
if(ENABLE_PYBINDINGS)
  add_subdirectory(src/cpp/bindings/python)
endif()

# -----------------------
# Cross-language LTO for release
# -----------------------
if(ENABLE_LTO)
  include(${CMAKE_SOURCE_DIR}/cmake/EnableCrossLangLTO.cmake)
  enable_cross_language_lto(TARGETS core app RUST_TARGET manimatio_rust_lib)
endif()

# -----------------------
# Snippet runner (build with: cmake --build <preset> --target snippets)
# -----------------------
get_property(_snippets GLOBAL PROPERTY ALL_SNIPPET_TARGETS)
if(_snippets)
  add_custom_target(snippets DEPENDS ${_snippets})
endif()

get_property(_tests GLOBAL PROPERTY ALL_TEST_TARGETS)
if(_tests)
  add_custom_target(tests DEPENDS ${_tests})
endif()