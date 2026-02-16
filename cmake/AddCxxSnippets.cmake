function(add_cxx_snippets lib_target dir)
  set(snippets_dir "${dir}/snippets")
  if(NOT IS_DIRECTORY "${snippets_dir}")
    return()
  endif()

  file(GLOB_RECURSE SNIPPET_SOURCES CONFIGURE_DEPENDS
    "${snippets_dir}/*.cpp"
  )

  foreach(src ${SNIPPET_SOURCES})
    cmake_path(GET src STEM snippet_name)
    set(snippet_target "snippet_${lib_target}_${snippet_name}")

    add_executable(${snippet_target} ${src})
    target_link_libraries(${snippet_target} PRIVATE ${lib_target})
    target_compile_features(${snippet_target} PRIVATE cxx_std_23)

    set_target_properties(${snippet_target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/snippets"
      EXCLUDE_FROM_ALL TRUE          # don't build with the default target
    )

    set_property(GLOBAL APPEND PROPERTY ALL_SNIPPET_TARGETS ${snippet_target})
  endforeach()
endfunction()