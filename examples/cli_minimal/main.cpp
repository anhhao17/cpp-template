// Example: define a 2-command CliApp, parse argv, and print help.
// Build target: example_cli_minimal. Try:
//   example_cli_minimal --help
//   example_cli_minimal greet --loud world
#include "etlx/conf/cli.hpp"
#include "host/host_io.hpp"

#include <cstdio>

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
    const conf::CliApp app = BuildApp();
    etlx::ports::host::FileWriter out;  // stdout

    if (argc < 2) {
        conf::PrintCliHelp(app, out);
        return 0;
    }

    const etl::string_view cmd_name{argv[1]};
    const conf::CliCommand* cmd = conf::FindCommand(app, cmd_name);
    if (cmd == nullptr) {
        std::printf("unknown command: %s\n\n", argv[1]);
        conf::PrintCliHelp(app, out);
        return 1;
    }

    bool             loud = false;
    etl::string_view name = "there";
    conf::CmdlineOptionsIterator it(argc - 2, argv + 2, *cmd);
    for (;;) {
        auto next = it.Next();
        if (!next) {
            std::printf("parse error: %s\n", next.error().message.c_str());
            return 1;
        }
        if (!next.value().has_value()) break;
        const conf::OptionValue& ov = next.value().value();
        if (ov.is_argument)             name = ov.value;
        else if (ov.option == "--loud") loud = true;
    }

    if (cmd_name == "version") {
        std::printf("minimal 1.0.0\n");
    } else {  // greet
        std::printf(loud ? "HELLO, %.*s!\n" : "hello, %.*s\n",
                    static_cast<int>(name.size()), name.data());
    }
    return 0;
}
