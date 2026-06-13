# Embedded C++ Template Library (ETL-based) — Implementation Plan

A reusable, heap-free C++ template/starter-kit for embedded systems, built on the
[Embedded Template Library (ETL)](https://www.etlcpp.com/). It provides a Mender-style
**declarative CLI** plus a set of **common service classes** (error/result model,
logging, I/O streams, config, JSON, crypto, network, HTTP, event loop, storage, and a
state machine), each shipped with usage examples.

Architecture and naming deliberately mirror the
[Mender client](https://github.com/mendersoftware/mender) `src/common` modules and its
[`cli.cpp`](https://github.com/mendersoftware/mender/blob/master/src/mender-update/cli/cli.cpp)
command model, but rewritten for constrained, no-`std`-heap targets.

---

## 1. Goals & non-goals

**Goals**
- A portable C++17 template that compiles for bare-metal / RTOS targets and for the host (for tests).
- Zero (or strictly bounded) dynamic allocation — all containers fixed-capacity via ETL.
- A clean **Result/Error** model (no exceptions, no RTTI required).
- A Mender-style declarative **CLI** with subcommands, options, arguments, and auto-generated help.
- **Common service interfaces** (network, crypto, http, io, log, storage) with swappable backends (HAL/ports).
- Every module shipped with a runnable **example** and **host unit tests**.

**Non-goals (initially)**
- Full OTA/update protocol implementation (we provide the *scaffolding* and an example, not a product).
- Multi-process/IPC/D-Bus (Mender uses these on Linux; embedded targets usually don't).
- Networking stack implementation — we abstract over lwIP / POSIX / mbedTLS, not reimplement them.

---

## 2. Design principles

| Principle | How it's applied |
|---|---|
| No heap | `etl::vector`, `etl::string`, `etl::flat_map`, object pools (`etl::pool`); no `new`/`make_shared`. |
| No exceptions | `ETL_NO_EXCEPTIONS`; errors returned as values via `Result<T>` (ETL `etl::expected`). |
| Portability via ports | Each subsystem defines an abstract interface; concrete backends live under `ports/`. |
| Compile-time sizing | Buffer/capacity limits are template params or config macros, not runtime. |
| Header-light templates | Interfaces header-only where practical; backends compiled per-platform. |
| Testable on host | Same code builds on host with a POSIX/mbedTLS port + GoogleTest. |
| Static dispatch where hot | Prefer `etl::delegate` / CRTP over virtual in performance-critical paths; virtual is fine for CLI actions and ports. |

---

## 3. Dependencies & tooling

| Concern | Choice | Notes |
|---|---|---|
| Core containers/types | **ETL** | `expected`, `optional`, `string`, `vector`, `flat_map`, `delegate`, `fsm`, `pool`, `crc`. |
| Crypto/TLS | **mbedTLS** (primary), wolfSSL (alt) | Hash, HMAC, AEAD, ECDSA/RSA verify, TLS. |
| HTTP parsing | **llhttp** (or picohttpparser) | Small, allocation-free request/response parser. |
| JSON | **ETL** + a small parser (e.g. a SAX-style hand-rolled or `jsmn`) | Bounded, no heap. |
| Build | **CMake** (+ presets) | One tree builds host tests and cross targets via toolchain files. |
| Host tests | **GoogleTest** (or Catch2) | Mender uses GoogleTest. |
| RTOS (optional) | FreeRTOS / Zephyr | Event loop & time ports can wrap these. |
| Static analysis | clang-tidy, cppcheck, optional MISRA | CI gates. |
| Formatting | clang-format | Match Mender's style for familiarity. |

---

## 4. Repository layout

Mirrors Mender's `src/common` + `cli` separation. Replace `mender-*` apps with your own.

```
embedded-etl-template/
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/
│   └── toolchains/                 # arm-none-eabi.cmake, host.cmake, ...
├── third_party/                    # etl, mbedtls, llhttp (submodules or fetched)
├── include/
│   └── etlx/                       # public headers (namespace etlx)
│       ├── core/                   # error, result(expected), optional, span, time
│       ├── conf/                   # CLI model + config parsing  (← mender conf)
│       ├── log/
│       ├── io/                     # Reader / Writer / Stream interfaces
│       ├── json/
│       ├── crypto/                 # Hasher, Verifier, Hmac, Random (interfaces)
│       ├── net/                    # Socket / DNS / TLS interfaces
│       ├── http/                   # Client, Request, Response
│       ├── events/                 # EventLoop / Timer / Scheduler
│       ├── storage/                # KeyValueStore (← mender key_value_database)
│       └── sm/                     # State machine helpers (etl::fsm wrappers)
├── src/                            # implementations (platform-independent)
├── ports/                         # backends per platform
│   ├── host/                       # POSIX sockets, std file storage, mbedTLS
│   ├── freertos/
│   └── bare-metal/
├── apps/
│   └── democli/                    # example application built on the template
│       ├── cli.hpp / cli.cpp       # ← models mender cli.cpp
│       └── actions/                # one .hpp/.cpp per command action
├── examples/                       # focused per-module usage examples
└── tests/                          # host unit tests (gtest), one dir per module
```

---

## 5. Core foundation (`etlx/core`)

These map Mender's `error`, `expected`, and `optional` modules onto ETL.

### 5.1 Error model (`core/error.hpp`)

Mender models errors as a *category + code + message*. We keep that, but the message
is a fixed-capacity `etl::string` and the category is a lightweight pointer/enum so it
costs nothing to copy around.

```cpp
namespace etlx::error {

using Code = int;

// A category groups related error codes (like std::error_category, but trivial).
struct Category {
    const char* name;
};

struct Error {
    const Category* category = nullptr;   // nullptr == NoError
    Code            code     = 0;
    etl::string<64> message;

    constexpr bool ok() const { return category == nullptr; }
    explicit operator bool() const { return !ok(); }   // truthy when there IS an error
};

inline const Error NoError {};

inline Error Make(const Category& cat, Code code, etl::string_view msg) {
    Error e; e.category = &cat; e.code = code; e.message.assign(msg.begin(), msg.end());
    return e;
}

} // namespace etlx::error
```

### 5.2 Result / Expected (`core/result.hpp`)

Mender returns `expected::Expected<T>` and `expected::unexpected(err)`. ETL ships
`etl::expected`, so we alias it:

```cpp
namespace etlx {
template <typename T>
using Result = etl::expected<T, error::Error>;          // value or Error

using Status = etl::expected<void, error::Error>;        // ok or Error

template <typename E = error::Error>
using Unexpected = etl::unexpected<E>;
}
```

Usage feels just like Mender's:

```cpp
etlx::Result<uint32_t> ParsePort(etl::string_view s) {
    uint32_t v = 0;
    if (!try_parse_u32(s, v))
        return etlx::Unexpected{etlx::error::Make(conf::Category, conf::InvalidOption, "bad port")};
    return v;   // implicit success
}
```

### 5.3 Other core bits
- `core/optional.hpp` → alias `etl::optional`.
- `core/span.hpp` → alias `etl::span` (views over fixed buffers, no copies).
- `core/time.hpp` → `Duration`/`TimePoint` abstraction implemented by ports.

---

## 6. Common service modules

Each module: **interface header** (portable) + **port backend(s)** + **example** + **tests**.

### 6.1 Logging (`etlx/log`)
- **Purpose:** leveled, allocation-free logging with compile-time level gating.
- **Key API:** `Log::Error/Warn/Info/Debug(fmt, ...)`; a `Sink` interface (`Write(level, msg)`) so output can go to UART, RTT, semihosting, or stdout.
- **ETL bits:** `etl::string` scratch buffer, `etl::to_string`.
- **Example:**
  ```cpp
  etlx::log::SetSink(uart_sink);
  ETLX_LOG_INFO("boot ok, hw=%s", board_name);
  ```

### 6.2 I/O streams (`etlx/io`)
- **Purpose:** Mender-style `Reader`/`Writer` abstraction so crypto/http/storage all speak the same byte-stream interface.
- **Key API:**
  ```cpp
  struct Reader  { virtual Result<size_t> Read (etl::span<uint8_t> dst) = 0; };
  struct Writer  { virtual Status         Write(etl::span<const uint8_t> src) = 0; };
  ```
- **Provided impls:** `SpanReader`, `SpanWriter` (over fixed buffers), `CountingWriter`, `TeeWriter`.
- **Example:** stream a file/flash region into a hasher without buffering the whole thing.

### 6.3 Config + CLI model (`etlx/conf`) — the heart of the CLI
This is the direct analogue of Mender's `conf` module (`CliApp`, `CliCommand`,
`CliOption`, `CliArgument`, `CmdlineOptionsIterator`, `PrintCliHelp`). See §7.

### 6.4 JSON (`etlx/json`)
- **Purpose:** parse/produce small JSON (config, manifests, HTTP bodies) with no heap.
- **Key API:** SAX-style `Parse(reader, handler)` + a bounded DOM `JsonValue` backed by `etl::flat_map`/`etl::vector` with fixed capacities.
- **ETL bits:** `etl::flat_map`, `etl::vector`, `etl::string`.
- **Example:** read `{"artifact_name": "...", "version": 3}` into a struct.

### 6.5 Crypto (`etlx/crypto`)
- **Purpose:** hashing, HMAC, AEAD, signature verification, RNG — interface + mbedTLS port.
- **Key API:**
  ```cpp
  struct Hasher { // SHA-256 etc.
      virtual Status Update(etl::span<const uint8_t>) = 0;
      virtual Result<Digest> Final() = 0;
  };
  struct Verifier { virtual Status Verify(etl::span<const uint8_t> data,
                                          etl::span<const uint8_t> sig) = 0; };
  struct Random  { virtual Status Fill(etl::span<uint8_t> out) = 0; };
  ```
- **Backends:** `ports/host/crypto_mbedtls.*`, optional wolfSSL.
- **Example:** verify an update image signature before applying it.

### 6.6 Network (`etlx/net`)
- **Purpose:** transport abstraction over sockets/lwIP/TLS.
- **Key API:** `Socket` (`Connect/Send/Recv/Close`), `Resolver` (DNS), `TlsSocket` (wraps a `Socket` + mbedTLS). All non-blocking-friendly, returning `Result`/would-block.
- **Backends:** `ports/host` (POSIX), `ports/freertos` (lwIP).
- **Example:** open a TCP/TLS connection and hand the `Reader`/`Writer` to HTTP.

### 6.7 HTTP (`etlx/http`)
- **Purpose:** minimal client modeled on Mender's `http` module: build a `Request`, send over a `net::Socket`/`TlsSocket`, parse a `Response` incrementally with **llhttp**.
- **Key API:** `Request{method, url, headers, body-writer}`, `Client::Do(request) -> Result<Response>`, streaming body via `io::Reader`.
- **ETL bits:** `etl::string` for URL/headers, fixed header table via `etl::flat_map`.
- **Example:** `GET https://.../manifest.json`, stream body into the JSON parser.

### 6.8 Storage / Key-Value (`etlx/storage`)
- **Purpose:** Mender's `key_value_database` analogue — persist small state (current artifact, update step) across reboots.
- **Key API:** `KeyValueStore::Get/Set/Remove/Commit` keyed by `etl::string`.
- **Backends:** NVS/EEPROM/flash sector, or a host file for tests.
- **Example:** persist "update in progress" so `resume` works after a reboot — exactly Mender's commit/rollback/resume model.

### 6.9 Events / Event loop (`etlx/events`)
- **Purpose:** single-threaded cooperative loop + timers (Mender uses an asio-style loop; we provide a tiny one or wrap RTOS).
- **Key API:** `EventLoop::Post(delegate)`, `Timer::After(duration, delegate)`, `EventLoop::Run()`.
- **ETL bits:** `etl::delegate`, `etl::priority_queue` (fixed) for timers, `etl::pool` for event nodes.
- **Example:** schedule a periodic "check-update" tick.

### 6.10 State machine (`etlx/sm`)
- **Purpose:** model the update workflow (Idle → Download → Verify → Install → Commit / Rollback) the way Mender's state machine does, but with `etl::fsm`/`etl::hfsm`.
- **Key API:** thin helpers around `etl::fsm` (states as classes, messages as events) with hooks for `stop-before` (matching Mender's `--stop-before` option).
- **Example:** the demo CLI's `install`/`commit`/`rollback`/`resume` drive this FSM.

---

## 7. CLI design (models `mender/.../cli.cpp`)

Reproduce Mender's **declarative command table + Action objects** approach, adapted to ETL.

### 7.1 Declarative model (`conf/cli.hpp`)

```cpp
namespace etlx::conf {

struct CliOption {
    etl::string_view long_option;      // e.g. "--stop-before"
    etl::string_view description;
    etl::string_view parameter;        // "" => boolean flag, else takes a value
};

struct CliArgument {
    etl::string_view name;
    bool             mandatory = false;
};

struct CliCommand {
    etl::string_view              name;            // "install"
    etl::string_view              description;
    etl::optional<CliArgument>    argument;
    etl::vector<CliOption, 8>     options;
};

struct CliApp {
    etl::string_view              name;
    etl::string_view              short_description;
    etl::string_view              long_description;
    etl::vector<CliCommand, 16>   commands;
};

// Iterates argv producing {option, value} tuples; same role as Mender's
// CmdlineOptionsIterator. Returns Result so callers can propagate errors.
class CmdlineOptionsIterator { /* Next() -> Result<OptionValue> */ };

Status PrintCliHelp(const CliApp&);
Status PrintCliCommandHelp(const CliApp&, etl::string_view command);

} // namespace etlx::conf
```

Compare directly with Mender, which declares e.g.:
```cpp
const conf::CliCommand cmd_install { .name = "install", .description = "...",
    .argument = conf::CliArgument{ .name = "artifact", .mandatory = true }, .options = {...} };
```

### 7.2 Action pattern (`apps/democli/actions/`)

Mender parses args into a polymorphic `Action` with `Execute(context)`. We keep the
interface but avoid `make_shared` — actions are stored in a **fixed object pool** /
static storage and referenced by pointer.

```cpp
namespace democli {

struct Context { /* config, storage, net, crypto, eventloop refs */ };

struct Action {
    virtual etlx::error::Error Execute(Context& ctx) = 0;
    virtual ~Action() = default;
};

class InstallAction : public Action {
public:
    explicit InstallAction(etl::string_view artifact) : artifact_(artifact) {}
    void SetStopBefore(etl::string_view s) { stop_before_ = s; }
    etlx::error::Error Execute(Context& ctx) override; // drives the sm::UpdateFsm
private:
    etl::string<128> artifact_;
    etl::string<32>  stop_before_;
};

// Returns a non-owning pointer into a static ActionPool, or an Error.
using ExpectedActionPtr = etlx::Result<Action*>;
ExpectedActionPtr ParseArguments(int argc, const char* const argv[]);

int Main(int argc, const char* const argv[]);  // mirrors mender cli Main()

} // namespace democli
```

### 7.3 Example command set (demo OTA-style CLI)

| Command | Argument / options | Action |
|---|---|---|
| `show-artifact` | — | print current artifact name from storage |
| `check-update` | — | HTTP GET manifest, compare versions |
| `install` | `<artifact>` (file/URL), `--stop-before STATE`, `--reboot-exit-code` | run download→verify→install FSM |
| `commit` | `--stop-before` | commit pending update (exit 2 if none) |
| `rollback` | `--stop-before` | rollback pending update (exit 2 if none) |
| `resume` | `--reboot-exit-code` | resume interrupted install after reboot |
| `daemon` | — | run event loop, periodic check-update |

Exit codes follow Mender's convention (e.g. `2` = no update in progress, `4` = reboot required).

---

## 8. Worked examples (`examples/`)

Each is a tiny `main()` you can build on host:

1. `result_error` — propagate `Result<T>` / `Error` across calls.
2. `cli_minimal` — define a 2-command `CliApp`, parse argv, print help.
3. `log_uart` — log through a custom sink.
4. `json_config` — parse a config blob into a struct.
5. `sha256_stream` — hash a buffer through `io::Reader` + `crypto::Hasher`.
6. `http_get` (host port) — fetch a URL, stream body to JSON.
7. `kv_persist` — set/get/commit across simulated reboot.
8. `update_fsm` — drive the install/commit/rollback state machine with `--stop-before`.

---

## 9. Build, toolchain & test

- **CMake presets:** `host-debug`, `host-asan`, `cortex-m4`, `cortex-m33`, etc.
- **Toolchain files** under `cmake/toolchains/` select compiler + flags (`-ffreestanding`, `-fno-exceptions`, `-fno-rtti`, `-Os`/`-Og`).
- **ETL config:** define `ETL_NO_EXCEPTIONS`, `ETL_CHECK_PUSH_POP`, capacity macros centrally in `etlx_config.hpp`.
- **Host tests:** GoogleTest, one target per module; ports/host provides POSIX socket + mbedTLS + file-KV backends so the *same* code is exercised.
- **CI:** matrix of {host-gcc, host-clang, arm-none-eabi}; run unit tests + clang-tidy + cppcheck + size report.

---

## 10. Suggested extras (beyond your request)

- **Watchdog port** (`etlx/wdt`) + a heartbeat helper — Mender has a watchdog concept; embedded targets need it.
- **Static memory budget report**: a CMake step that fails if `.bss/.data` or per-pool sizes regress.
- **`etl::profiler`/counters** for pool high-water marks (catch undersized capacities early).
- **Reproducible/secure boot hooks**: signature verify (`crypto::Verifier`) as a gate before `InstallAction`.
- **A/B (dual-bank) update example** wired to the FSM and `KeyValueStore` (rollback safety).
- **Fuzz targets** (host, libFuzzer) for the JSON and HTTP parsers — highest-risk parsing code.
- **MISRA/AUTOSAR C++ profile** as an optional clang-tidy config for safety-critical users.
- **Power/sleep port** so the event loop can idle the MCU between timers.
- **Telemetry/metrics sink** (counters over the log/HTTP transport).
- **Mock ports** under `ports/mock` for deterministic tests (fake socket, fake clock, fake storage).

---

## 11. Phased roadmap

### Phase 0 — Scaffolding (week 1)
- [ ] Repo layout, CMake + presets, ETL/mbedTLS/llhttp as submodules.
- [ ] `etlx_config.hpp`, host + arm toolchain files, CI skeleton.
- [ ] `core/error`, `core/result`, `core/optional`, `core/span` + tests.

### Phase 1 — CLI engine (weeks 2–3)
- [ ] `conf::CliApp/Command/Option/Argument` + `CmdlineOptionsIterator`.
- [ ] `PrintCliHelp` / `PrintCliCommandHelp`.
- [ ] `Action` interface + static `ActionPool`; `democli` with `show-artifact`, `check-update`.
- [ ] Example `cli_minimal` + tests for parsing/help/exit codes.

### Phase 2 — Foundational services (weeks 3–5)
- [ ] `log` (+ sinks), `io` (Reader/Writer + Span impls).
- [ ] `storage::KeyValueStore` (+ host file backend) — enables commit/resume.
- [ ] `json` parser + bounded DOM.
- [ ] Examples + tests for each.

### Phase 3 — Crypto & network (weeks 5–8)
- [ ] `crypto` interfaces + mbedTLS port (SHA-256, HMAC, ECDSA verify, RNG).
- [ ] `net` interfaces + host POSIX port; `TlsSocket` via mbedTLS.
- [ ] `http::Client` + llhttp response parsing; `http_get` example.

### Phase 4 — Update workflow (weeks 8–10)
- [ ] `events` loop + timers; `sm` wrappers over `etl::fsm`.
- [ ] `UpdateFsm` (download→verify→install→commit/rollback) with `--stop-before`.
- [ ] Full `democli`: `install/commit/rollback/resume/daemon` with Mender-style exit codes.
- [ ] A/B update example + fuzz targets for parsers.

### Phase 5 — Hardening & docs (weeks 10–12)
- [ ] Cross-build on a real MCU (e.g. STM32/nRF), size + RAM budget report in CI.
- [ ] clang-tidy/cppcheck gates, optional MISRA profile, sanitizer runs on host.
- [ ] Per-module README + a "porting guide" (how to add a new `ports/<platform>`).

---

## 12. Risks & considerations

| Risk | Mitigation |
|---|---|
| Fixed capacities too small/large at runtime | Centralize in `etlx_config.hpp`; CI size + pool high-water checks. |
| mbedTLS RAM footprint on small MCUs | Trim cipher suites; make crypto/TLS optional via CMake feature flags. |
| `std::function`/heap creeping in via deps | `-fno-exceptions -fno-rtti`, ban `<functional>`/`new` via clang-tidy. |
| Blocking sockets stalling the loop | Non-blocking `net` API returning would-block; integrate with `events`. |
| Action lifetime without `shared_ptr` | Static `ActionPool` + non-owning `Action*`; actions are stateless-ish/short-lived. |
| Divergence from Mender semantics | Keep the CLI struct shapes and exit-code conventions identical for familiarity. |

---

## 13. References

- Mender CLI command model — `src/mender-update/cli/cli.cpp`
- Mender common modules — `src/common` (error, expected, conf, io, json, http, crypto, key_value_database, events, log)
- ETL — containers, `expected`, `delegate`, `fsm`, `pool`: https://www.etlcpp.com/
- mbedTLS, llhttp, GoogleTest, FreeRTOS/Zephyr, CMake.

> Licensing note: Mender is Apache-2.0. You may reference its *design/structure* freely;
> if you copy any source verbatim, retain its Apache-2.0 headers and attribution.
