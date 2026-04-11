function(load_cxxlibs base_dir)
  if(NOT IS_DIRECTORY "${base_dir}")
    return()
  endif()

  # Use a common target for CXX modules
  add_library(cxxlibs STATIC)
  target_compile_features(cxxlibs PUBLIC cxx_std_23)
 
  # Pre-create the file set with BASE_DIRS covering all subdirectories
  target_sources(cxxlibs PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES BASE_DIRS "${base_dir}"
  )
 
  set_property(GLOBAL PROPERTY CXXLIBS_UNIFIED_TARGET cxxlibs)
  set_property(GLOBAL APPEND PROPERTY AUTO_CXXLIB_TARGETS cxxlibs)

  file(GLOB children RELATIVE "${base_dir}" "${base_dir}/*")
  foreach(child ${children})
    if(IS_DIRECTORY "${base_dir}/${child}")
      add_subdirectory("${base_dir}/${child}" "${CMAKE_BINARY_DIR}/cxxlib_${child}")
    endif()
  endforeach()
endfunction()
