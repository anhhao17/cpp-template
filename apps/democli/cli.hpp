#ifndef DEMOCLI_CLI_HPP
#define DEMOCLI_CLI_HPP

#include "etlx/conf/cli.hpp"
#include "etlx/core/result.hpp"
#include "etlx/io/io.hpp"

// democli: a small OTA-style command-line app built on the etlx template,
// modelled on Mender's mender-update CLI. It demonstrates the declarative
// command table (conf::CliApp) plus the polymorphic Action pattern, with
// actions held in static storage rather than via make_shared.
namespace democli {

// Mender-style process exit codes.
enum ExitCode : int {
    ExitOk            = 0,
    ExitError         = 1,
    ExitNoUpdate      = 2,  // commit/rollback with nothing in progress
    ExitRebootNeeded  = 4,
};

// Shared services handed to every action. In a real build these would carry
// storage / net / crypto / event-loop references; here it is just the output
// stream so the demo stays self-contained and testable.
struct Context {
    etlx::io::Writer& out;
};

// Polymorphic command, mirroring Mender's Action. Execute returns NoError on
// success or an error whose code is used as the process exit code.
struct Action {
    virtual etlx::error::Error Execute(Context& ctx) = 0;
    virtual ~Action() = default;
};

// The declarative command table. Defined once; used for parsing and help.
const etlx::conf::CliApp& App();

// Parses argv (including argv[0]) into one of the statically-allocated actions.
// Returns a non-owning pointer into static storage, or an error. Handles the
// top-level "--help" / no-command / unknown-command cases by returning an
// action that prints help.
using ExpectedActionPtr = etlx::Result<Action*>;
ExpectedActionPtr ParseArguments(int argc, const char* const argv[], Context& ctx);

// Entry point mirroring Mender's cli Main(): parse, execute, map error to an
// exit code.
int Main(int argc, const char* const argv[]);

} // namespace democli

#endif // DEMOCLI_CLI_HPP
