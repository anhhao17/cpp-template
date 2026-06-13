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

# Boost.Asio host transport. We fetch the official Boost source release (same
# tarball + hash pattern as mbedTLS) but use Asio HEADER-ONLY: SOURCE_SUBDIR
# points at a non-existent directory so FetchContent only downloads/extracts and
# never add_subdirectory()s Boost's own build. An INTERFACE target exposes the
# include path. This cross-compiles to aarch64 with no Boost build step.
if(ETLX_TRANSPORT STREQUAL "asio")
  FetchContent_Declare(
    boost
    URL      https://archives.boost.io/release/1.91.0/source/boost_1_91_0.tar.bz2
    URL_HASH SHA256=de5e6b0e4913395c6bdfa90537febd9028ea4c0735d2cdb0cd9b45d5f51264f5
    SOURCE_SUBDIR  __header_only_no_build__
  )
  FetchContent_MakeAvailable(boost)

  find_package(Threads REQUIRED)
  add_library(etlx_boost_asio INTERFACE)
  target_include_directories(etlx_boost_asio SYSTEM INTERFACE ${boost_SOURCE_DIR})
  target_link_libraries(etlx_boost_asio INTERFACE Threads::Threads)
  # Asio is header-only by default; we must NOT define
  # BOOST_ASIO_SEPARATE_COMPILATION (even to 0), or it expects a separately
  # compiled src.cpp.
  target_compile_definitions(etlx_boost_asio INTERFACE BOOST_ASIO_NO_DEPRECATED)
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
