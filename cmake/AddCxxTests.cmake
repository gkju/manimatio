function(add_cxx_tests lib_target dir)
  set(tests_dir "${dir}/tests")
  
  # Check if the tests/ directory exists
  if(NOT IS_DIRECTORY "${tests_dir}")
    return()
  endif()

  # Find all C++ files in the tests/ directory
  file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${tests_dir}/*.cpp"
  )

  # Only proceed if there are actually test files
  if(TEST_SOURCES)
    set(test_target "test_${lib_target}")

    add_executable(${test_target} ${TEST_SOURCES})
    
    # Link against the module itself and Catch2
    target_link_libraries(${test_target} PRIVATE 
        ${lib_target} 
        Catch2::Catch2WithMain
    )
    
    target_compile_features(${test_target} PRIVATE cxx_std_23)

    # Output tests to a common directory
    set_target_properties(${test_target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

    # Register with Catch for automatic discovery
    include(Catch)
    catch_discover_tests(${test_target})

    set_property(GLOBAL APPEND PROPERTY ALL_TEST_TARGETS ${test_target})
  endif()
endfunction()