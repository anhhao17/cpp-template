#include "etlx/conf/cli.hpp"

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

namespace conf = etlx::conf;

namespace {

// Builds a command: `do-it [--loud] [--name VALUE] <target>`.
conf::CliCommand MakeCommand() {
    conf::CliCommand cmd;
    cmd.name        = "do-it";
    cmd.description = "do the thing";
    cmd.argument    = conf::CliArgument{"target", true};
    cmd.options.push_back(conf::CliOption{"--loud", "be loud", ""});
    cmd.options.push_back(conf::CliOption{"--name", "set a name", "VALUE"});
    return cmd;
}

// Collects all tokens the iterator yields, or returns the first error.
struct ParsedArgs {
    std::vector<conf::OptionValue> tokens;
    etlx::Optional<etlx::error::Error> error;
};

ParsedArgs Parse(const conf::CliCommand& cmd,
                 std::vector<const char*> args) {
    ParsedArgs out;
    conf::CmdlineOptionsIterator it(static_cast<int>(args.size()), args.data(), cmd);
    for (;;) {
        auto next = it.Next();
        if (!next) { out.error = next.error(); return out; }
        if (!next.value().has_value()) break;
        out.tokens.push_back(next.value().value());
    }
    return out;
}

TEST(CmdlineOptionsIterator, ParsesFlagValueAndArgument) {
    auto cmd = MakeCommand();
    auto p = Parse(cmd, {"--loud", "--name", "bob", "device0"});
    ASSERT_FALSE(p.error.has_value());
    ASSERT_EQ(p.tokens.size(), 3u);

    EXPECT_EQ(p.tokens[0].option, "--loud");
    EXPECT_TRUE(p.tokens[0].value.empty());

    EXPECT_EQ(p.tokens[1].option, "--name");
    EXPECT_EQ(p.tokens[1].value, "bob");

    EXPECT_TRUE(p.tokens[2].is_argument);
    EXPECT_EQ(p.tokens[2].value, "device0");
}

TEST(CmdlineOptionsIterator, AcceptsEqualsForm) {
    auto cmd = MakeCommand();
    auto p = Parse(cmd, {"--name=alice"});
    ASSERT_FALSE(p.error.has_value());
    ASSERT_EQ(p.tokens.size(), 1u);
    EXPECT_EQ(p.tokens[0].option, "--name");
    EXPECT_EQ(p.tokens[0].value, "alice");
}

TEST(CmdlineOptionsIterator, DoubleDashForcesPositional) {
    auto cmd = MakeCommand();
    auto p = Parse(cmd, {"--", "--name"});
    ASSERT_FALSE(p.error.has_value());
    ASSERT_EQ(p.tokens.size(), 1u);
    EXPECT_TRUE(p.tokens[0].is_argument);
    EXPECT_EQ(p.tokens[0].value, "--name");
}

TEST(CmdlineOptionsIterator, RejectsUnknownOption) {
    auto cmd = MakeCommand();
    auto p = Parse(cmd, {"--nope"});
    ASSERT_TRUE(p.error.has_value());
    EXPECT_EQ(p.error->code, conf::UnknownOption);
}

TEST(CmdlineOptionsIterator, RejectsMissingValue) {
    auto cmd = MakeCommand();
    auto p = Parse(cmd, {"--name"});  // value-taking option with no value
    ASSERT_TRUE(p.error.has_value());
    EXPECT_EQ(p.error->code, conf::MissingValue);
}

TEST(CmdlineOptionsIterator, RejectsValueOnFlag) {
    auto cmd = MakeCommand();
    auto p = Parse(cmd, {"--loud=1"});
    ASSERT_TRUE(p.error.has_value());
    EXPECT_EQ(p.error->code, conf::UnexpectedValue);
}

// Renders help into a std::string for substring assertions.
std::string RenderHelp(const conf::CliApp& app) {
    uint8_t buf[1024];
    etlx::io::SpanWriter w(etlx::ByteSpan{buf, sizeof(buf)});
    EXPECT_TRUE(conf::PrintCliHelp(app, w).has_value());
    return std::string(reinterpret_cast<char*>(buf), w.written());
}

TEST(PrintCliHelp, ListsCommands) {
    conf::CliApp app;
    app.name              = "tool";
    app.short_description = "a tool";
    conf::CliCommand a; a.name = "alpha"; a.description = "first";
    conf::CliCommand b; b.name = "beta";  b.description = "second";
    app.commands.push_back(a);
    app.commands.push_back(b);

    auto help = RenderHelp(app);
    EXPECT_NE(help.find("tool"), std::string::npos);
    EXPECT_NE(help.find("alpha"), std::string::npos);
    EXPECT_NE(help.find("beta"), std::string::npos);
    EXPECT_NE(help.find("first"), std::string::npos);
}

TEST(FindCommand, FindsAndMisses) {
    conf::CliApp app;
    conf::CliCommand a; a.name = "alpha";
    app.commands.push_back(a);
    EXPECT_NE(conf::FindCommand(app, "alpha"), nullptr);
    EXPECT_EQ(conf::FindCommand(app, "zzz"), nullptr);
}

} // namespace
