// Example: drive the update state machine through install -> commit, and show
// that an illegal event (committing before install finishes) is ignored.
// Output via etlx::log. Build target: example_update_fsm.
#include "etlx/log/log.hpp"
#include "etlx/sm/update_fsm.hpp"
#include "host/host_io.hpp"

namespace {

void Show(const char* what, etlx::sm::UpdateFsm& fsm) {
    ETLX_LOG_INFO("%-26s -> %s", what, fsm.state_name());
}

} // namespace

int main() {
    static etlx::ports::host::StderrLogSink sink;
    etlx::log::SetSink(&sink);

    etlx::sm::UpdateFsm fsm;
    ETLX_LOG_INFO("%-26s -> %s", "initial", fsm.state_name());

    // Illegal: cannot commit before the workflow reaches AwaitCommit.
    fsm.Commit();
    Show("Commit (too early, ignored)", fsm);

    Show("Start",  (fsm.Start(),  fsm));
    Show("Step",   (fsm.Step(),   fsm));   // Verify
    Show("Step",   (fsm.Step(),   fsm));   // Install
    Show("Step",   (fsm.Step(),   fsm));   // AwaitCommit
    Show("Commit", (fsm.Commit(), fsm));   // Done

    ETLX_LOG_INFO("--- second run: rollback path ---");
    etlx::sm::UpdateFsm fsm2;
    fsm2.Start();
    fsm2.Step();   // Verify
    fsm2.Abort();
    Show("Abort during Verify", fsm2);     // Failed
    return 0;
}
