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

#endif // ETLX_CONFIG_HPP
