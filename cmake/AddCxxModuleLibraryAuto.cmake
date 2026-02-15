function(add_cxx_module_library_auto target dir)
  if(CMAKE_VERSION VERSION_LESS 3.28)
    message(FATAL_ERROR "C++ modules require CMake >= 3.28")
  endif()

  add_library(${target} STATIC)

  file(GLOB_RECURSE MOD_IFACES CONFIGURE_DEPENDS
    "${dir}/modules/*.ixx"
    "${dir}/modules/*.cppm"
    "${dir}/modules/*.mpp"
  )

  if(MOD_IFACES)
    target_sources(${target}
      PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
          ${MOD_IFACES}
    )
  endif()

  file(GLOB_RECURSE NORMAL_SOURCES CONFIGURE_DEPENDS
    "${dir}/src/*.cpp"
    "${dir}/src/*.cc"
    "${dir}/src/*.cxx"
  )
  if(NORMAL_SOURCES)
    target_sources(${target} PRIVATE ${NORMAL_SOURCES})
  endif()

  if(IS_DIRECTORY "${dir}/include")
    target_include_directories(${target} PUBLIC "${dir}/include")
  endif()

  target_compile_features(${target} PUBLIC cxx_std_23)
endfunction()
