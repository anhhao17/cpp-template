# Host unit tests. These also run under qemu-aarch64 when cross-compiling, via
# the CMAKE_CROSSCOMPILING_EMULATOR set in the aarch64 toolchain file.
#
# Test code links GoogleTest, which uses exceptions/RTTI, so it deliberately
# does NOT link etlx_embedded_flags (the -fno-exceptions/-fno-rtti target).
include(GoogleTest)

# Defines a gtest executable and registers it with ctest. Extra args after the
# sources are additional link libraries.
function(etlx_add_test name)
  set(srcs "")
  set(extra_libs "")
  set(seen_lib FALSE)
  foreach(arg ${ARGN})
    if(arg STREQUAL "LIBS")
      set(seen_lib TRUE)
    elseif(seen_lib)
      list(APPEND extra_libs ${arg})
    else()
      list(APPEND srcs ${arg})
    endif()
  endforeach()

  add_executable(${name} ${srcs})
  target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR})
  target_link_libraries(${name} PRIVATE etlx::etlx GTest::gtest_main ${extra_libs})
  gtest_discover_tests(${name}
    DISCOVERY_MODE PRE_TEST   # don't run the binary at configure time (cross-build)
  )
endfunction()

etlx_add_test(test_core    core/test_core.cpp)
etlx_add_test(test_io      io/test_io.cpp)
etlx_add_test(test_cli     conf/test_cli.cpp)
etlx_add_test(test_kv      storage/test_kv.cpp LIBS etlx::host)
etlx_add_test(test_json    json/test_json.cpp)
etlx_add_test(test_crypto  crypto/test_crypto.cpp LIBS etlx::host)
etlx_add_test(test_democli democli/test_democli.cpp LIBS democli_lib etlx::host)
