#ifndef ETLX_CONFIG_HPP
#define ETLX_CONFIG_HPP

// Central configuration for the etlx template library.
//
// All compile-time capacities live here so a target can be sized in one place
// and CI can diff/budget them. These are deliberately conservative defaults;
// override them per-target with -DETLX_xxx or by editing this file in a fork.
//
// ETL_NO_EXCEPTIONS is defined by the build system (see CMakeLists.txt) so ETL
// reports container/contract violations through its assert handler rather than
// throwing. STL stays enabled because both supported targets (native host and
// aarch64-linux-gnu) ship a full libstdc++.

// Maximum number of characters held in an error::Error message.
#ifndef ETLX_ERROR_MESSAGE_CAPACITY
#define ETLX_ERROR_MESSAGE_CAPACITY 64
#endif

// Maximum number of characters in one formatted log line.
#ifndef ETLX_LOG_LINE_CAPACITY
#define ETLX_LOG_LINE_CAPACITY 128
#endif

// Maximum number of options accepted by a single CLI command.
#ifndef ETLX_CLI_MAX_OPTIONS
#define ETLX_CLI_MAX_OPTIONS 8
#endif

// Maximum number of commands registered on a CLI app.
#ifndef ETLX_CLI_MAX_COMMANDS
#define ETLX_CLI_MAX_COMMANDS 16
#endif

// Compile-time log level gate. Levels above this compile to nothing.
// 0=None 1=Error 2=Warn 3=Info 4=Debug
#ifndef ETLX_LOG_LEVEL
#define ETLX_LOG_LEVEL 4
#endif

// Key-value store sizing: number of entries, and the per-entry key/value caps.
#ifndef ETLX_KV_MAX_ENTRIES
#define ETLX_KV_MAX_ENTRIES 16
#endif

#ifndef ETLX_KV_KEY_CAPACITY
#define ETLX_KV_KEY_CAPACITY 32
#endif

#ifndef ETLX_KV_VALUE_CAPACITY
#define ETLX_KV_VALUE_CAPACITY 128
#endif

// Maximum number of headers in an HTTP request or parsed response.
#ifndef ETLX_HTTP_MAX_HEADERS
#define ETLX_HTTP_MAX_HEADERS 16
#endif

// Event loop sizing: immediate-callback queue depth and pending-timer count.
#ifndef ETLX_EVENTS_QUEUE_DEPTH
#define ETLX_EVENTS_QUEUE_DEPTH 16
#endif

#ifndef ETLX_EVENTS_MAX_TIMERS
#define ETLX_EVENTS_MAX_TIMERS 8
#endif

// SE05x port sizing. These bound the on-stack ASN.1 scratch buffers and the
// fixed-size etl containers used in the public SE API surface.
#ifndef ETLX_SE_SPKI_CAPACITY
#define ETLX_SE_SPKI_CAPACITY 512    // DER SPKI for RSA-2048 ≈ 294 bytes
#endif

#ifndef ETLX_SE_SIG_CAPACITY
#define ETLX_SE_SIG_CAPACITY 512     // RSA-2048 sig = 256 bytes; covers up to 4096
#endif

#ifndef ETLX_SE_CSR_PEM_CAPACITY
#define ETLX_SE_CSR_PEM_CAPACITY 2048 // PEM CSR for RSA-2048 ≈ 1.7 kB
#endif

#ifndef ETLX_SE_CERT_CAPACITY
#define ETLX_SE_CERT_CAPACITY 2048   // DER cert for RSA-2048 identity
#endif

#ifndef ETLX_SE_BLOB_CAPACITY
#define ETLX_SE_BLOB_CAPACITY 512    // binary objects (device-info, SCP03 keys)
#endif

#ifndef ETLX_SE_UID_LEN
#define ETLX_SE_UID_LEN 18           // SE05x chip UID is always 18 bytes
#endif

#endif // ETLX_CONFIG_HPP
