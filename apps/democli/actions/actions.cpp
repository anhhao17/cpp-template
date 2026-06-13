#include "apps/democli/actions/actions.hpp"

#include "etlx/conf/cli.hpp"
#include "etlx/log/log.hpp"
#include "etlx/sm/update_fsm.hpp"

#include <cstdint>

namespace democli {
namespace {

// democli's own error category. Action exit codes are carried as error codes.
const etlx::error::Category Category{"democli"};

// Writes a string view to the context's output writer.
etlx::Status Put(Context& ctx, etl::string_view s) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(s.data());
    return ctx.out.Write(etlx::ConstByteSpan{bytes, s.size()});
}

// Builds an Error carrying an exit code, so Main can surface it as the process
// status without a separate channel.
etlx::error::Error ExitWith(int code, etl::string_view msg) {
    return etlx::error::Make(Category, code, msg);
}

} // namespace

etlx::error::Error ShowArtifactAction::Execute(Context& ctx) {
    Put(ctx, "artifact: device-image-1.0.0\n");
    return etlx::error::NoError;
}

etlx::error::Error CheckUpdateAction::Execute(Context& ctx) {
    ETLX_LOG_INFO("checking for updates");
    Put(ctx, "current:   device-image-1.0.0\n");
    Put(ctx, "available: device-image-1.1.0\n");
    Put(ctx, "update available\n");
    return etlx::error::NoError;
}

etlx::error::Error InstallAction::Execute(Context& ctx) {
    if (artifact_.empty()) {
        return ExitWith(ExitError, "install requires an artifact argument");
    }

    Put(ctx, "installing ");
    Put(ctx, etl::string_view{artifact_.data(), artifact_.size()});
    Put(ctx, "\n");

    // Drive the real update FSM. Start moves Idle -> Download; each phase is
    // performed and then Step() advances the machine. --stop-before halts by
    // simply not sending the next event.
    etlx::sm::UpdateFsm fsm;
    fsm.Start();  // -> Download

    static constexpr etl::string_view phases[] = {"download", "verify", "install"};
    for (auto phase : phases) {
        if (!stop_before_.empty() &&
            etl::string_view{stop_before_.data(), stop_before_.size()} == phase) {
            Put(ctx, "stopping before ");
            Put(ctx, phase);
            Put(ctx, "\n");
            return etlx::error::NoError;
        }
        Put(ctx, "  ");
        Put(ctx, phase);
        Put(ctx, " ok\n");
        fsm.Step();  // advance to the next workflow state
    }
    // Machine is now at AwaitCommit, awaiting a reboot + commit/rollback.
    Put(ctx, "install complete; reboot required\n");
    return ExitWith(ExitRebootNeeded, "reboot required");
}

etlx::error::Error CommitAction::Execute(Context& ctx) {
    // Nothing was staged in this demo, so report "no update in progress".
    Put(ctx, "no update in progress\n");
    return ExitWith(ExitNoUpdate, "no update to commit");
}

etlx::error::Error RollbackAction::Execute(Context& ctx) {
    Put(ctx, "no update in progress\n");
    return ExitWith(ExitNoUpdate, "no update to roll back");
}

etlx::error::Error HelpAction::Execute(Context& ctx) {
    if (command_.empty()) {
        return etlx::conf::PrintCliHelp(App(), ctx.out).has_value()
                   ? etlx::error::NoError
                   : ExitWith(ExitError, "help failed");
    }
    auto st = etlx::conf::PrintCliCommandHelp(
        App(), etl::string_view{command_.data(), command_.size()}, ctx.out);
    return st.has_value() ? etlx::error::NoError : st.error();
}

} // namespace democli
