#include "apps/democli/cli.hpp"

#include "apps/democli/actions/actions.hpp"
#include "etlx/conf/cli.hpp"
#include "host/host_io.hpp"

namespace democli {
namespace {

namespace conf = etlx::conf;

// Builds the declarative command table once, on first use. Capacities come from
// etlx_config.hpp (ETLX_CLI_MAX_COMMANDS / ETLX_CLI_MAX_OPTIONS).
conf::CliApp BuildApp() {
    conf::CliApp app;
    app.name              = "democli";
    app.short_description = "demo OTA-style CLI built on the etlx template";
    app.long_description  =
        "Mirrors the Mender update CLI command model: a declarative command\n"
        "table plus polymorphic actions, with no heap or exceptions.";

    conf::CliCommand show;
    show.name        = "show-artifact";
    show.description = "Print the currently installed artifact name";
    app.commands.push_back(show);

    conf::CliCommand check;
    check.name        = "check-update";
    check.description = "Query for a newer artifact and report the result";
    app.commands.push_back(check);

    conf::CliCommand install;
    install.name        = "install";
    install.description = "Download, verify and install an artifact";
    install.argument    = conf::CliArgument{"artifact", true};
    install.options.push_back(
        conf::CliOption{"--stop-before", "Halt the workflow before STATE", "STATE"});
    app.commands.push_back(install);

    conf::CliCommand commit;
    commit.name        = "commit";
    commit.description = "Commit a pending update (exit 2 if none)";
    commit.options.push_back(
        conf::CliOption{"--stop-before", "Halt before the given state", "STATE"});
    app.commands.push_back(commit);

    conf::CliCommand rollback;
    rollback.name        = "rollback";
    rollback.description = "Roll back a pending update (exit 2 if none)";
    rollback.options.push_back(
        conf::CliOption{"--stop-before", "Halt before the given state", "STATE"});
    app.commands.push_back(rollback);

    return app;
}

// Static storage for one instance of each action. ParseArguments configures and
// returns a pointer into this pool, so no dynamic allocation occurs.
struct ActionPool {
    ShowArtifactAction show_artifact;
    CheckUpdateAction  check_update;
    InstallAction      install;
    CommitAction       commit;
    RollbackAction     rollback;
    HelpAction         help;
};

ActionPool& Pool() {
    static ActionPool pool;
    return pool;
}

bool IsHelpFlag(etl::string_view s) { return s == "--help" || s == "-h"; }

} // namespace

const conf::CliApp& App() {
    static const conf::CliApp app = BuildApp();
    return app;
}

ExpectedActionPtr ParseArguments(int argc, const char* const argv[], Context&) {
    ActionPool& pool = Pool();

    // No command, or a top-level help request.
    if (argc < 2 || IsHelpFlag(etl::string_view{argv[1]}) ||
        etl::string_view{argv[1]} == "help") {
        // "help <command>" or "--help <command>" shows per-command help.
        if (argc >= 3) pool.help.SetCommand(etl::string_view{argv[2]});
        return &pool.help;
    }

    const etl::string_view cmd_name{argv[1]};
    const conf::CliCommand* cmd = conf::FindCommand(App(), cmd_name);
    if (cmd == nullptr) {
        return etlx::Fail(conf::Category, conf::UnknownCommand, cmd_name);
    }

    // A "--help" anywhere after the command name prints that command's help.
    for (int i = 2; i < argc; ++i) {
        if (IsHelpFlag(etl::string_view{argv[i]})) {
            pool.help.SetCommand(cmd_name);
            return &pool.help;
        }
    }

    // Walk the command's arguments/options.
    conf::CmdlineOptionsIterator it(argc - 2, argv + 2, *cmd);
    etl::string_view stop_before;
    etl::string_view argument;
    bool             have_argument = false;

    for (;;) {
        auto next = it.Next();
        if (!next) return etlx::Unexpected<>{next.error()};
        if (!next.value().has_value()) break;  // done

        const conf::OptionValue& ov = next.value().value();
        if (ov.is_argument) {
            if (have_argument) {
                return etlx::Fail(conf::Category, conf::TooManyArgs,
                                  "unexpected extra argument");
            }
            argument      = ov.value;
            have_argument = true;
        } else if (ov.option == "--stop-before") {
            stop_before = ov.value;
        }
    }

    // Dispatch to the matching action.
    if (cmd_name == "show-artifact") return &pool.show_artifact;
    if (cmd_name == "check-update")  return &pool.check_update;
    if (cmd_name == "install") {
        if (have_argument) pool.install.SetArtifact(argument);
        if (!stop_before.empty()) pool.install.SetStopBefore(stop_before);
        return &pool.install;
    }
    if (cmd_name == "commit")   return &pool.commit;
    if (cmd_name == "rollback") return &pool.rollback;

    // Unreachable: FindCommand already validated the name.
    return etlx::Fail(conf::Category, conf::UnknownCommand, cmd_name);
}

int Main(int argc, const char* const argv[]) {
    etlx::ports::host::FileWriter out;  // stdout
    Context ctx{out};

    auto action = ParseArguments(argc, argv, ctx);
    if (!action) {
        const etlx::error::Error& e = action.error();
        // Best-effort error line, then top-level help.
        out.Write(etlx::ConstByteSpan{
            reinterpret_cast<const uint8_t*>("error: "), 7});
        out.Write(etlx::ConstByteSpan{
            reinterpret_cast<const uint8_t*>(e.message.data()), e.message.size()});
        out.Write(etlx::ConstByteSpan{reinterpret_cast<const uint8_t*>("\n\n"), 2});
        conf::PrintCliHelp(App(), out);
        return ExitError;
    }

    etlx::error::Error e = action.value()->Execute(ctx);
    return e.ok() ? ExitOk : e.code;
}

} // namespace democli
