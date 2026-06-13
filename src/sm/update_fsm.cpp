#include "etlx/sm/update_fsm.hpp"

namespace etlx::sm {

const char* StateName(etl::fsm_state_id_t id) {
    switch (id) {
        case IdleId:        return "Idle";
        case DownloadId:    return "Download";
        case VerifyId:      return "Verify";
        case InstallId:     return "Install";
        case AwaitCommitId: return "AwaitCommit";
        case DoneId:        return "Done";
        case FailedId:      return "Failed";
        default:            return "Unknown";
    }
}

UpdateFsm::UpdateFsm() : etl::fsm(0) {
    state_list_[IdleId]        = &idle_;
    state_list_[DownloadId]    = &download_;
    state_list_[VerifyId]      = &verify_;
    state_list_[InstallId]     = &install_;
    state_list_[AwaitCommitId] = &await_commit_;
    state_list_[DoneId]        = &done_;
    state_list_[FailedId]      = &failed_;

    set_states(state_list_, NumStates);
    start();
}

} // namespace etlx::sm
