#include "etlx/sm/update_fsm.hpp"

#include <gtest/gtest.h>

namespace sm = etlx::sm;

namespace {

TEST(UpdateFsm, StartsIdle) {
    sm::UpdateFsm fsm;
    EXPECT_EQ(fsm.state(), sm::IdleId);
}

TEST(UpdateFsm, HappyPathToDone) {
    sm::UpdateFsm fsm;
    EXPECT_EQ(fsm.Start(), sm::DownloadId);
    EXPECT_EQ(fsm.Step(),  sm::VerifyId);
    EXPECT_EQ(fsm.Step(),  sm::InstallId);
    EXPECT_EQ(fsm.Step(),  sm::AwaitCommitId);
    EXPECT_EQ(fsm.Commit(), sm::DoneId);
}

TEST(UpdateFsm, RollbackFromAwaitCommit) {
    sm::UpdateFsm fsm;
    fsm.Start();
    fsm.Step();
    fsm.Step();
    fsm.Step();
    ASSERT_EQ(fsm.state(), sm::AwaitCommitId);
    EXPECT_EQ(fsm.Rollback(), sm::FailedId);
}

TEST(UpdateFsm, AbortDuringWorkflowFails) {
    sm::UpdateFsm fsm;
    fsm.Start();          // Download
    EXPECT_EQ(fsm.Abort(), sm::FailedId);
}

TEST(UpdateFsm, IllegalEventsAreIgnored) {
    sm::UpdateFsm fsm;
    // Commit before reaching AwaitCommit must not change state.
    EXPECT_EQ(fsm.Commit(), sm::IdleId);
    EXPECT_EQ(fsm.Step(),   sm::IdleId);   // Next has no effect in Idle
    EXPECT_EQ(fsm.Start(),  sm::DownloadId);
    // Start again mid-workflow is ignored (Download doesn't handle Start).
    EXPECT_EQ(fsm.Start(),  sm::DownloadId);
}

TEST(UpdateFsm, TerminalStatesAreStable) {
    sm::UpdateFsm fsm;
    fsm.Start(); fsm.Step(); fsm.Step(); fsm.Step();
    fsm.Commit();
    ASSERT_EQ(fsm.state(), sm::DoneId);
    fsm.Step(); fsm.Commit(); fsm.Abort();
    EXPECT_EQ(fsm.state(), sm::DoneId);  // nothing moves it out of Done
}

TEST(StateName, KnownIds) {
    EXPECT_STREQ(sm::StateName(sm::IdleId), "Idle");
    EXPECT_STREQ(sm::StateName(sm::AwaitCommitId), "AwaitCommit");
    EXPECT_STREQ(sm::StateName(sm::FailedId), "Failed");
}

} // namespace
