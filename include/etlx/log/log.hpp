#ifndef ETLX_LOG_LOG_HPP
#define ETLX_LOG_LOG_HPP

#include "etlx/etlx_config.hpp"

#include <etl/string_view.h>

// Leveled, allocation-free logging with compile-time level gating. Output is
// routed through a Sink so it can land on a UART, RTT, semihosting channel or
// stdout depending on the platform port.
namespace etlx::log {

enum class Level : int {
    None  = 0,
    Error = 1,
    Warn  = 2,
    Info  = 3,
    Debug = 4,
};

// Destination for formatted log lines. A port implements Write().
struct Sink {
    virtual void Write(Level level, etl::string_view line) = 0;
    virtual ~Sink() = default;
};

// Installs the sink that subsequent log calls write to. Passing nullptr
// silences logging. Not thread-safe; set it once during start-up.
void SetSink(Sink* sink);

// Sets the runtime level threshold. Messages above this level are dropped even
// if they passed the compile-time ETLX_LOG_LEVEL gate. Defaults to Debug.
void SetLevel(Level level);

Level GetLevel();

// Formats with printf semantics into a fixed line buffer and forwards to the
// active sink. Prefer the ETLX_LOG_* macros, which also apply compile-time
// gating so disabled levels generate no code.
void Logf(Level level, const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

} // namespace etlx::log

// Compile-time gated entry points. A level whose numeric value exceeds
// ETLX_LOG_LEVEL expands to nothing, so its arguments are not even evaluated.
#define ETLX_LOG_AT(level_value, level_enum, ...)                         \
    do {                                                                  \
        if constexpr ((level_value) <= ETLX_LOG_LEVEL) {                  \
            ::etlx::log::Logf((level_enum), __VA_ARGS__);                 \
        }                                                                 \
    } while (0)

#define ETLX_LOG_ERROR(...) ETLX_LOG_AT(1, ::etlx::log::Level::Error, __VA_ARGS__)
#define ETLX_LOG_WARN(...)  ETLX_LOG_AT(2, ::etlx::log::Level::Warn,  __VA_ARGS__)
#define ETLX_LOG_INFO(...)  ETLX_LOG_AT(3, ::etlx::log::Level::Info,  __VA_ARGS__)
#define ETLX_LOG_DEBUG(...) ETLX_LOG_AT(4, ::etlx::log::Level::Debug, __VA_ARGS__)

#endif // ETLX_LOG_LOG_HPP
