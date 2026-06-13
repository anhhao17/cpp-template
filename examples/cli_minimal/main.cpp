// Example: define a 2-command CliApp, parse argv, and print help. Help is
// rendered with conf::PrintCliHelp into an io::Writer; other output uses
// etlx::log. Build target: example_cli_minimal. Try:
//   example_cli_minimal --help
//   example_cli_minimal greet --loud world
#include "etlx/conf/cli.hpp"
#include "etlx/log/log.hpp"
#include "host/host_io.hpp"

namespace conf = etlx::conf;

namespace {

conf::CliApp BuildApp() {
    conf::CliApp app;
    app.name              = "minimal";
    app.short_description = "a two-command demo of the etlx CLI model";

    conf::CliCommand greet;
    greet.name        = "greet";
    greet.description = "Print a greeting";
    greet.argument    = conf::CliArgument{"name", false};
    greet.options.push_back(conf::CliOption{"--loud", "Shout the greeting", ""});
    app.commands.push_back(greet);

    conf::CliCommand version;
    version.name        = "version";
    version.description = "Print the version";
    app.commands.push_back(version);

    return app;
}

} // namespace

int main(int argc, char** argv) {
    static etlx::ports::host::StderrLogSink sink;
    etlx::log::SetSink(&sink);

    const conf::CliApp app = BuildApp();
    etlx::ports::host::FileWriter out;  // stdout, for rendered help

    if (argc < 2) {
        conf::PrintCliHelp(app, out);
        return 0;
    }

    const etl::string_view cmd_name{argv[1]};
    const conf::CliCommand* cmd = conf::FindCommand(app, cmd_name);
    if (cmd == nullptr) {
        ETLX_LOG_ERROR("unknown command: %s", argv[1]);
        conf::PrintCliHelp(app, out);
        return 1;
    }

    bool             loud = false;
    etl::string_view name = "there";
    conf::CmdlineOptionsIterator it(argc - 2, argv + 2, *cmd);
    for (;;) {
        auto next = it.Next();
        if (!next) {
            ETLX_LOG_ERROR("parse error: %s", next.error().message.c_str());
            return 1;
        }
        if (!next.value().has_value()) break;
        const conf::OptionValue& ov = next.value().value();
        if (ov.is_argument)             name = ov.value;
        else if (ov.option == "--loud") loud = true;
    }

    if (cmd_name == "version") {
        ETLX_LOG_INFO("minimal 1.0.0");
    } else {  // greet
        ETLX_LOG_INFO(loud ? "HELLO, %.*s!" : "hello, %.*s",
                      static_cast<int>(name.size()), name.data());
    }
    return 0;
}
