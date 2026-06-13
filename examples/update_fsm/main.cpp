// Example: drive the update state machine through install -> commit, and show
// that an illegal event (committing before install finishes) is ignored.
// Build target: example_update_fsm.
#include "etlx/sm/update_fsm.hpp"

#include <cstdio>

namespace {

void Show(const char* what, etlx::sm::UpdateFsm& fsm) {
    std::printf("%-26s -> %s\n", what, fsm.state_name());
}

} // namespace

int main() {
    etlx::sm::UpdateFsm fsm;
    std::printf("initial                    -> %s\n", fsm.state_name());

    // Illegal: cannot commit before the workflow reaches AwaitCommit.
    fsm.Commit();
    Show("Commit (too early, ignored)", fsm);

    Show("Start",  (fsm.Start(),  fsm));
    Show("Step",   (fsm.Step(),   fsm));   // Verify
    Show("Step",   (fsm.Step(),   fsm));   // Install
    Show("Step",   (fsm.Step(),   fsm));   // AwaitCommit
    Show("Commit", (fsm.Commit(), fsm));   // Done

    std::printf("\n--- second run: rollback path ---\n");
    etlx::sm::UpdateFsm fsm2;
    fsm2.Start();
    fsm2.Step();   // Verify
    fsm2.Abort();
    Show("Abort during Verify", fsm2);     // Failed
    return 0;
}
