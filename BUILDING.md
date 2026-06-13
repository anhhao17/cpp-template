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
| `include/etlx/` | public headers: `core`, `io`, `log`, `conf`, `storage`, `json`, `crypto`, `net`, `http`, `events`, `sm` |
| `src/` | platform-independent implementations |
| `ports/host/` | host backends: stdout/stderr io+log, file KV store, `/dev/urandom`, POSIX TCP, monotonic clock |
| `apps/democli/` | demo OTA-style CLI built on the template (install drives the real update FSM) |
| `examples/` | one runnable `main()` per module |
| `tests/` | GoogleTest host/qemu unit tests (73 cases) |

## Modules

All modules are heap-free and exception-free, and build under both options:

| Module | What it provides |
|---|---|
| `core` | `Error`, `Result`/`Status` (over `etl::expected`), `Optional`, `Span`, time/clock |
| `io` | `Reader`/`Writer` + `Span`/`Counting`/`Tee` writers and `Copy` |
| `log` | leveled, compile-time-gated logging with a `Sink` seam |
| `conf` | declarative CLI model + `CmdlineOptionsIterator` + help rendering |
| `storage` | `KeyValueStore` (in-memory + host file backend) |
| `json` | bounded jsmn-style tokenizer + `Json` view accessor |
| `crypto` | `Hasher`/`Verifier`/`Random` + software SHA-256/HMAC + host RNG |
| `net` | `Socket` interface + host POSIX TCP port |
| `http` | request builder + bounded HTTP/1.1 response parser + `Client` |
| `events` | cooperative `EventLoop` with timers |
| `sm` | `UpdateFsm` update workflow on `etl::fsm` |

The third-party crypto/TLS and HTTP-parser dependencies named in `README.md`
(mbedTLS, llhttp) are intentionally replaced here with dependency-free software
implementations so the cross-aarch64 build needs no extra sysroot libraries;
the interfaces are swappable, so an mbedTLS / llhttp port can be dropped in.
