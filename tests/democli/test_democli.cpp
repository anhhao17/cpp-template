#include "apps/democli/cli.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

// Drives ParseArguments + Execute, capturing output into a string and returning
// the action's exit code (NoError maps to ExitOk).
struct RunResult {
    int         exit_code = -1;
    std::string output;
    bool        parse_ok = true;
    etlx::error::Code parse_error_code = 0;
};

RunResult RunCli(std::vector<const char*> argv) {
    uint8_t buf[2048];
    etlx::io::SpanWriter w(etlx::ByteSpan{buf, sizeof(buf)});
    democli::Context ctx{w};

    RunResult rr;
    auto action = democli::ParseArguments(static_cast<int>(argv.size()),
                                          argv.data(), ctx);
    if (!action) {
        rr.parse_ok = false;
        rr.parse_error_code = action.error().code;
        return rr;
    }
    auto err = action.value()->Execute(ctx);
    rr.exit_code = err.ok() ? democli::ExitOk : err.code;
    rr.output.assign(reinterpret_cast<char*>(buf), w.written());
    return rr;
}

TEST(Democli, ShowArtifact) {
    auto r = RunCli({"democli", "show-artifact"});
    EXPECT_EQ(r.exit_code, democli::ExitOk);
    EXPECT_NE(r.output.find("artifact:"), std::string::npos);
}

TEST(Democli, InstallRunsFullWorkflowAndAsksReboot) {
    auto r = RunCli({"democli", "install", "image-1.1.0"});
    EXPECT_EQ(r.exit_code, democli::ExitRebootNeeded);
    EXPECT_NE(r.output.find("download ok"), std::string::npos);
    EXPECT_NE(r.output.find("install ok"), std::string::npos);
}

TEST(Democli, InstallStopBeforeHalts) {
    auto r = RunCli({"democli", "install", "image-1.1.0", "--stop-before", "verify"});
    EXPECT_EQ(r.exit_code, democli::ExitOk);
    EXPECT_NE(r.output.find("download ok"), std::string::npos);
    EXPECT_NE(r.output.find("stopping before verify"), std::string::npos);
    EXPECT_EQ(r.output.find("verify ok"), std::string::npos);  // never reached
}

TEST(Democli, InstallWithoutArtifactErrors) {
    // install declares a mandatory argument; with none, Execute reports error.
    auto r = RunCli({"democli", "install"});
    EXPECT_EQ(r.exit_code, democli::ExitError);
}

TEST(Democli, CommitWithNothingPendingExitsTwo) {
    auto r = RunCli({"democli", "commit"});
    EXPECT_EQ(r.exit_code, democli::ExitNoUpdate);
}

TEST(Democli, UnknownCommandIsParseError) {
    auto r = RunCli({"democli", "frobnicate"});
    EXPECT_FALSE(r.parse_ok);
    EXPECT_EQ(r.parse_error_code, etlx::conf::UnknownCommand);
}

TEST(Democli, HelpForNoArgs) {
    auto r = RunCli({"democli"});
    EXPECT_EQ(r.exit_code, democli::ExitOk);
    EXPECT_NE(r.output.find("Commands:"), std::string::npos);
}

TEST(Democli, RejectsUnknownOption) {
    auto r = RunCli({"democli", "install", "img", "--bogus"});
    EXPECT_FALSE(r.parse_ok);
    EXPECT_EQ(r.parse_error_code, etlx::conf::UnknownOption);
}

} // namespace
