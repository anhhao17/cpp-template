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
| `net` | `Socket` interface + host POSIX TCP port + `TlsSocket` (TLS/mTLS) |
| `http` | request builder + bounded HTTP/1.1 response parser + `Client` |
| `events` | cooperative `EventLoop` with timers |
| `sm` | `UpdateFsm` update workflow on `etl::fsm` |

### TLS / mutual TLS

`net::TlsSocket` layers TLS over any `net::Socket` (so it composes with
`PosixTcpSocket`) and is configured like the Mender client:

```cpp
etlx::net::TlsConfig cfg;
cfg.ca_cert_pem     = ca_pem;       // verify the server   (Mender: server_cert)
cfg.client_cert_pem = client_pem;   // present for mutual TLS (client_cert)
cfg.client_key_pem  = client_key;   //                        (client_cert_key)
cfg.server_name     = "device.example.com";  // SNI + hostname check
// cfg.skip_verify  = true;         // testing only

etlx::ports::host::PosixTcpSocket tcp;
etlx::net::TlsSocket tls(tcp);
tls.Configure(cfg);
tls.Connect("device.example.com", 443);   // handshake (sends client cert)
// hand `tls` to http::Client just like a plain socket
```

Leaving `client_cert_pem`/`client_key_pem` empty gives server-only TLS; setting
them enables **mutual TLS**. The `example_mtls_client` target and the
`test_tls` suite demonstrate a full mutual-auth handshake against a loopback
server that requires a client certificate.

TLS is built only when `-DETLX_WITH_TLS=ON` (the default). **mbedTLS 3.6.6 LTS
is fetched and built from source** by CMake — it is *not* taken from the system
— so the cross-aarch64 build needs no TLS library in its sysroot. The HTTP
parser and the SHA-256/HMAC primitives remain dependency-free software
implementations; their interfaces are swappable (e.g. an llhttp port).
