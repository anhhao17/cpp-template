# Third-party dependencies, pulled via FetchContent so the tree builds without
# manual submodule juggling. Pinned to specific tags for reproducibility.
include(FetchContent)

# ETL: the Embedded Template Library (heap-free containers/types).
FetchContent_Declare(
  etl
  GIT_REPOSITORY https://github.com/ETLCPP/etl.git
  GIT_TAG        20.47.1
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(etl)

# mbedTLS: fetched and built from source (not the system copy) so the cross
# aarch64 build needs no TLS library in its sysroot. Provides TLS + the crypto
# primitives behind net::TlsSocket. Only fetched when TLS is enabled.
if(ETLX_WITH_TLS)
  FetchContent_Declare(
    mbedtls
    URL      https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.6/mbedtls-3.6.6.tar.bz2
    URL_HASH SHA256=8fb65fae8dcae5840f793c0a334860a411f884cc537ea290ce1c52bb64ca007a
  )
  # Build just the libraries: no sample programs, fuzzers, or test suite.
  set(ENABLE_PROGRAMS  OFF CACHE BOOL "" FORCE)
  set(ENABLE_TESTING   OFF CACHE BOOL "" FORCE)
  set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(mbedtls)
endif()

# GoogleTest, used by the host unit tests only.
if(ETLX_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.16.0
    GIT_SHALLOW    TRUE
  )
  # Don't install gtest with our project; keep it a build-only dependency.
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()
