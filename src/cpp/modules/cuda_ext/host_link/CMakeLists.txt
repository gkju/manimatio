include(ExternalProject)

set(CUDA_EXT_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../project)
set(CUDA_EXT_BIN_DIR ${CMAKE_BINARY_DIR}/cuda_ext-build)

ExternalProject_Add(cuda_ext_build
  SOURCE_DIR ${CUDA_EXT_SRC_DIR}
  BINARY_DIR ${CUDA_EXT_BIN_DIR}
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  INSTALL_COMMAND ""
)

if(WIN32)
  set(CUDA_EXT_LIB ${CUDA_EXT_BIN_DIR}/cuda_ext.lib)
else()
  set(CUDA_EXT_LIB ${CUDA_EXT_BIN_DIR}/libcuda_ext.a)
endif()

add_library(cuda_ext_host STATIC IMPORTED GLOBAL)
add_dependencies(cuda_ext_host cuda_ext_build)
set_target_properties(cuda_ext_host PROPERTIES IMPORTED_LOCATION ${CUDA_EXT_LIB})
target_include_directories(cuda_ext_host INTERFACE ${CUDA_EXT_SRC_DIR}/include)
