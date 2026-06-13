# Building

The tree supports **two build options**, both driven by CMake presets and both
exercised by the same source and tests.

| Option | Preset | Toolchain file | Output |
|---|---|---|---|
| Native (host) | `native` | `cmake/toolchains/host.cmake` | binaries for this machine |
| Cross aarch64 | `aarch64` | `cmake/toolchains/aarch64-linux-gnu.cmake` | ARM64 Linux binaries |

ETL (20.47.1) and GoogleTest (1.16.0) are fetched automatically by CMake
`FetchContent` on first configure, so no submodules are needed.

## Prerequisites

- CMake ≥ 3.21 and Ninja
- A native C++17 compiler (`g++` or `clang++`)
- For the cross build: `g++-aarch64-linux-gnu` (Debian/Ubuntu package)
- Optional, to *run* the cross build: `qemu-user` (`qemu-aarch64`)

## Native build

```sh
cmake --preset native
cmake --build build/native
ctest --preset native            # 28 tests
./build/native/apps/democli/democli --help
```

An AddressSanitizer variant is also available: `--preset native-asan`.

## Cross build for aarch64

```sh
cmake --preset aarch64
cmake --build build/aarch64
```

The toolchain file wires `qemu-aarch64` in as `CMAKE_CROSSCOMPILING_EMULATOR`,
so the cross-built unit tests can be run on the host transparently:

```sh
ctest --preset aarch64           # runs the aarch64 binaries under qemu
qemu-aarch64 -L /usr/aarch64-linux-gnu ./build/aarch64/apps/democli/democli install img
```

Override the cross prefix for a vendor SDK with
`-DETLX_CROSS_PREFIX=aarch64-poky-linux-`.

## Layout

| Path | Contents |
|---|---|
| `include/etlx/` | public headers (`core`, `io`, `log`, `conf`) |
| `src/` | platform-independent implementations |
| `ports/host/` | host backends for the `io`/`log` interfaces |
| `apps/democli/` | demo OTA-style CLI built on the template |
| `examples/` | one runnable `main()` per module |
| `tests/` | GoogleTest host/qemu unit tests |

What is implemented today maps to phases 0–1 (plus `log` and `io` from phase 2)
of the roadmap in `README.md`: the `core` foundation, the declarative CLI
engine, logging, and the I/O stream model, all with both build options and
tests green on each.
