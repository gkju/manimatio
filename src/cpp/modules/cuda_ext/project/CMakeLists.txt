cmake_minimum_required(VERSION 3.28)
project(cuda_ext_project LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)

add_library(cuda_ext STATIC cuda_ext.cu)
target_include_directories(cuda_ext PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
