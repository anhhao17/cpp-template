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
| `ports/asio/` | Boost.Asio TCP transport (`net::Socket`), optional (`ETLX_WITH_ASIO`) |
| `apps/democli/` | demo OTA-style CLI built on the template (install drives the real update FSM) |
| `examples/` | one runnable `main()` per module |
| `ports/se05x/` | NXP SE05x secure-element port (keygen, sign, SCP03), optional (`ETLX_WITH_SE05X`) |
| `apps/se_factory/` | factory provisioning CLI (`provision`, `write-cert`, `rotate-scp03`, …) |
| `tests/` | GoogleTest host/qemu unit tests (77 cases) |

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

### SE05x secure-element port (optional)

`ports/se05x` adds support for the NXP SE050/SE051/SE052 secure element family
via NXP's Plug & Trust SSS middleware (fetched from source at configure time).
Enable it with `-DETLX_WITH_SE05X=ON` (requires `-DETLX_WITH_TLS=ON`, the
default):

```sh
cmake -S . -B build/se05x -G Ninja \
  -DETLX_WITH_SE05X=ON \
  -DETLX_BUILD_TESTS=OFF   # unit tests do not need hardware
cmake --build build/se05x
```

The port exposes a C++ façade over the SSS API:

| Class | What it does |
|---|---|
| `se::Connection` | RAII session owner (`ex_sss_boot_ctx_t`); Platform SCP03 |
| `se::RsaKey` | RSA-2048/4096 keygen, sign (PKCS#1-v1.5-SHA256), CSR generation |
| `se::ObjectStore` | read/write binary blobs and DER certificates; chip UID |
| `se::Scp03Admin` | GP PUT KEY rotation; atomic key-file update |
| `se::SetupOpaquePk` | wire SE key into `mbedtls_pk_context` for `net::TlsSocket` |

**Factory provisioning** (`apps/se_factory`) provides seven commands:
`provision`, `write-cert`, `write-info`, `read-info`, `rotate-scp03`,
`uid`, and `verify`.  All commands require `EX_SSS_BOOT_SCP03_PATH`.

**SE-backed mTLS** — once a device is provisioned, the client's RSA private
key never leaves the chip during a TLS handshake:

```cpp
etlx::se::Connection conn = etlx::se::Connection::Open(nullptr).value();
etlx::se::RsaKey     key  = etlx::se::RsaKey::Open(conn, kRsaKeyId).value();

mbedtls_pk_context opaque;
mbedtls_pk_init(&opaque);
etlx::se::SetupOpaquePk(opaque, key);   // signing goes through the SE

etlx::net::TlsSocket tls(tcp);
tls.UseExternalKey(opaque);             // inject before Configure()

etlx::net::TlsConfig cfg;
cfg.ca_cert_pem     = ca_pem;
cfg.client_cert_pem = cert_pem;         // cert signed by your CA
// client_key_pem intentionally left empty — SE key is used
tls.Configure(cfg);
tls.Connect("device.example.com", 443);
```

See `examples/se_mtls_client` for the full demo (software fallback when
`EX_SSS_BOOT_SSS_PORT` is not set, so it runs in CI without hardware).

### Boost.Asio transport (optional)

For parity with the real Mender client (whose HTTP layer is built on
Boost.Asio/Beast), `ports/asio` provides `AsioTcpSocket`, an alternative
`net::Socket` backend for host/Linux targets. Because it implements the same
interface, it drops into `http::Client` unchanged and can be wrapped by
`net::TlsSocket` for **mutual TLS over an Asio transport** (the mTLS stack stays
on mbedTLS — no OpenSSL/`asio::ssl` needed):

```cpp
etlx::ports::asio::AsioTcpSocket transport;
etlx::net::TlsSocket            tls(transport);   // same TlsSocket as over POSIX
tls.Configure(cfg);                               // cfg carries client cert/key
tls.Connect("device.example.com", 443);
```

Built only when `-DETLX_TRANSPORT=asio` (the default). **Boost 1.91.0 is fetched
from source** (release tarball + SHA256, same pattern as mbedTLS) and used
**header-only** — CMake never runs Boost's own build (`SOURCE_SUBDIR` trick), so
it cross-compiles to aarch64 with no Boost build step and no Boost in the
sysroot. Boost headers are confined to the port's `.cpp` via a PIMPL, so
including `asio/asio_tcp.hpp` does not pull Boost into the rest of the build.
See `example_asio_http_get` (Asio + http) and `test_asio` (TCP round-trip +
mutual TLS over Asio). Turn it off with `-DETLX_TRANSPORT=posix` to avoid the
large Boost download.
