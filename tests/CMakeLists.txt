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
etlx_add_test(test_http    http/test_http.cpp)
etlx_add_test(test_sm      sm/test_sm.cpp)
etlx_add_test(test_events  events/test_events.cpp)

# Transport-specific tests build only for the selected backend.
if(ETLX_TRANSPORT STREQUAL "posix")
  etlx_add_test(test_net   net/test_net.cpp LIBS etlx::transport)
elseif(ETLX_TRANSPORT STREQUAL "asio")
  if(ETLX_WITH_TLS)
    # Also exercise mutual TLS layered over the Asio transport.
    etlx_add_test(test_asio asio/test_asio.cpp LIBS etlx::transport etlx::tls)
    target_compile_definitions(test_asio PRIVATE ETLX_TEST_WITH_TLS=1)
  else()
    etlx_add_test(test_asio asio/test_asio.cpp LIBS etlx::transport)
    target_compile_definitions(test_asio PRIVATE ETLX_TEST_WITH_TLS=0)
  endif()
endif()

# TLS test runs over whichever transport is selected.
if(ETLX_WITH_TLS)
  etlx_add_test(test_tls   net/test_tls.cpp LIBS etlx::tls etlx::transport)
endif()
etlx_add_test(test_democli democli/test_democli.cpp LIBS democli_lib etlx::host)

# SE05x hardware/simulator integration tests.
# Tests skip automatically when EX_SSS_BOOT_SSS_PORT is not set, so they are
# safe to include in the normal ctest run.  Set the env var to run against
# the simulator (socket transport) or real I2C hardware.
if(ETLX_WITH_SE05X)
  etlx_add_test(test_se_connection
    se/test_se_connection.cpp
    LIBS etlx::se05x)
  etlx_add_test(test_se_rsa_key
    se/test_se_rsa_key.cpp
    LIBS etlx::se05x)
  etlx_add_test(test_se_object_store
    se/test_se_object_store.cpp
    LIBS etlx::se05x)
endif()
