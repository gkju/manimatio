cmake_minimum_required(VERSION 3.28)

if (CMAKE_VERSION VERSION_LESS 3.18)
  set(DEV_MODULE Development)
else()
  set(DEV_MODULE Development.Module)
endif()

find_package(Python 3.9 COMPONENTS Interpreter ${DEV_MODULE} REQUIRED)
find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(cpp_rust_template NOMINSIZE module.cpp)
target_link_libraries(cpp_rust_template PRIVATE core)
target_compile_features(cpp_rust_template PRIVATE cxx_std_23)
