# Native (host) toolchain.
#
# Build option 1 of 2: compiles for the machine running CMake using the system
# compiler. Used for fast iteration, host unit tests, and sanitizer runs.
#
# Usage:
#   cmake --preset native
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/host.cmake -S . -B build/native

set(ETLX_TARGET_NATIVE ON CACHE BOOL "Building for the native host" FORCE)

# Let CMake auto-detect the system gcc/g++ / clang. Honour an override if the
# caller exported CC/CXX before configuring.
if(DEFINED ENV{CC})
  set(CMAKE_C_COMPILER "$ENV{CC}" CACHE FILEPATH "")
endif()
if(DEFINED ENV{CXX})
  set(CMAKE_CXX_COMPILER "$ENV{CXX}" CACHE FILEPATH "")
endif()
