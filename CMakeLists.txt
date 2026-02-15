cmake_minimum_required(VERSION 3.28)
project(manimatio LANGUAGES C CXX)

option(ENABLE_LTO "Enable cross-language link-time optimization (LTO)" OFF)
option(ENABLE_CUDA_EXT "Enable CUDA extension" OFF)
option(ENABLE_PYBINDINGS "Enable nanobind bindings" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.6
)
FetchContent_MakeAvailable(Corrosion)