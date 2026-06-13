#ifndef ETLX_CONF_CLI_HPP
#define ETLX_CONF_CLI_HPP

#include "etlx/core/optional.hpp"
#include "etlx/core/result.hpp"
#include "etlx/etlx_config.hpp"
#include "etlx/io/io.hpp"

#include <etl/string_view.h>
#include <etl/vector.h>

// Declarative CLI model, the direct analogue of Mender's conf module. A program
// describes its commands as static data (CliApp / CliCommand / CliOption /
// CliArgument); CmdlineOptionsIterator walks argv against that description, and
// PrintCliHelp renders help from it. No heap, no exceptions.
namespace etlx::conf {

extern const error::Category Category;

enum Code : error::Code {
    None           = 0,
    UnknownCommand = 1,
    UnknownOption  = 2,
    MissingValue   = 3,  // option expects a value but none was given
    UnexpectedValue = 4, // boolean flag was given "--flag=x"
    TooManyArgs    = 5,
};

// A single long option. An empty `parameter` marks a boolean flag; otherwise
// the option consumes one value and `parameter` names it in help output.
struct CliOption {
    etl::string_view long_option;  // e.g. "--stop-before"
    etl::string_view description;
    etl::string_view parameter;    // "" => boolean flag, else value name
};

// A positional argument following the command name.
struct CliArgument {
    etl::string_view name;
    bool             mandatory = false;
};

struct CliCommand {
    etl::string_view                          name;         // "install"
    etl::string_view                          description;
    Optional<CliArgument>                     argument;
    etl::vector<CliOption, ETLX_CLI_MAX_OPTIONS> options;
};

struct CliApp {
    etl::string_view                            name;
    etl::string_view                            short_description;
    etl::string_view                            long_description;
    etl::vector<CliCommand, ETLX_CLI_MAX_COMMANDS> commands;
};

// One parsed token: either a recognised option (with its value, empty for a
// flag) or a positional argument (`is_argument` true, `option` empty).
struct OptionValue {
    etl::string_view option;   // e.g. "--stop-before", empty for an argument
    etl::string_view value;    // option value, or the argument text
    bool             is_argument = false;
};

// Walks the argv slice belonging to one command, validating each token against
// that command's declared options. Plays the same role as Mender's
// CmdlineOptionsIterator. Construct it with the args *after* the command name.
class CmdlineOptionsIterator {
public:
    CmdlineOptionsIterator(int argc, const char* const argv[],
                           const CliCommand& command)
        : argc_(argc), argv_(argv), command_(command) {}

    // Returns the next token, an empty Optional when exhausted, or an error for
    // an unknown option / missing or unexpected value.
    Result<Optional<OptionValue>> Next();

    bool done() const { return index_ >= argc_; }

private:
    const CliOption* FindOption(etl::string_view long_option) const;

    int                 argc_;
    const char* const*  argv_;
    const CliCommand&   command_;
    int                 index_         = 0;
    bool                args_only_     = false;  // set once "--" is seen
};

// Finds a command by name, or nullptr if the app has no such command.
const CliCommand* FindCommand(const CliApp& app, etl::string_view name);

// Renders top-level help (synopsis + command list) to the writer.
Status PrintCliHelp(const CliApp& app, io::Writer& out);

// Renders help for a single command (its argument + options) to the writer.
Status PrintCliCommandHelp(const CliApp& app, etl::string_view command,
                           io::Writer& out);

} // namespace etlx::conf

#endif // ETLX_CONF_CLI_HPP
