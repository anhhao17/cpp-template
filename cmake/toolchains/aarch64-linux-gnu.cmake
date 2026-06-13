# Cross-compilation toolchain for 64-bit ARM Linux (aarch64).
#
# Build option 2 of 2: produces aarch64 ELF binaries that run on ARM64 Linux
# devices (e.g. Raspberry Pi 64-bit, i.MX8, Jetson) or under qemu-aarch64 on the
# host. Expects the Debian/Ubuntu "g++-aarch64-linux-gnu" cross toolchain.
#
# Usage:
#   cmake --preset aarch64
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake -S . -B build/aarch64

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(ETLX_TARGET_AARCH64 ON CACHE BOOL "Cross-building for aarch64-linux-gnu" FORCE)

# Allow overriding the cross-compiler prefix (e.g. a vendor SDK).
set(ETLX_CROSS_PREFIX "aarch64-linux-gnu-" CACHE STRING "Cross toolchain prefix")

set(CMAKE_C_COMPILER   "${ETLX_CROSS_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${ETLX_CROSS_PREFIX}g++")
set(CMAKE_AR           "${ETLX_CROSS_PREFIX}ar")
set(CMAKE_RANLIB       "${ETLX_CROSS_PREFIX}ranlib")
set(CMAKE_STRIP        "${ETLX_CROSS_PREFIX}strip")

# Search for programs only in the host paths, but headers/libraries only in the
# target sysroot — the standard CMake cross-compilation layout.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Run aarch64 test binaries transparently through qemu user-mode emulation if it
# is available. This lets `ctest` execute the cross-built unit tests on the host.
find_program(ETLX_QEMU_AARCH64 NAMES qemu-aarch64 qemu-aarch64-static)
if(ETLX_QEMU_AARCH64)
  set(CMAKE_CROSSCOMPILING_EMULATOR "${ETLX_QEMU_AARCH64};-L;/usr/aarch64-linux-gnu")
endif()
