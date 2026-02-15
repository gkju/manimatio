include(CheckIPOSupported)

function(enable_cross_language_lto)
  set(options)
  set(oneValueArgs RUST_TARGET)
  set(multiValueArgs TARGETS)
  cmake_parse_arguments(ECLLTO "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT ECLLTO_TARGETS OR NOT ECLLTO_RUST_TARGET)
    message(FATAL_ERROR "enable_cross_language_lto(TARGETS ... RUST_TARGET ...)")
  endif()

  check_ipo_supported(RESULT ipo_ok OUTPUT ipo_err)
  if(NOT ipo_ok)
    message(WARNING "IPO/LTO not supported: ${ipo_err}")
    return()
  endif()

  foreach(tgt IN LISTS ECLLTO_TARGETS)
    set_property(TARGET ${tgt} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

    if(MSVC)
      target_compile_options(${tgt} PRIVATE -flto=thin)
      target_link_options(${tgt} PRIVATE -flto=thin)
    else()
      target_compile_options(${tgt} PRIVATE -flto=thin)
      target_link_options(${tgt} PRIVATE -flto=thin -fuse-ld=lld)
    endif()
  endforeach()

  corrosion_add_target_local_rustflags(${ECLLTO_RUST_TARGET}
    "-Clinker-plugin-lto"
    "-Clinker=clang"
    "-Clink-arg=-fuse-ld=lld"
  )
endfunction()
