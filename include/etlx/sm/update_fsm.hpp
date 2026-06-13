#ifndef ETLX_SM_UPDATE_FSM_HPP
#define ETLX_SM_UPDATE_FSM_HPP

#include <etl/fsm.h>
#include <etl/message.h>

// Update workflow as an etl::fsm, modelling Mender's state machine:
//
//   Idle --Start--> Download --Next--> Verify --Next--> Install
//        --Next--> AwaitCommit --Commit--> Done
//                              --Rollback--> Failed
//   (any active state) --Abort--> Failed
//
// The FSM enforces the legal order of operations (e.g. you cannot Commit before
// reaching AwaitCommit); a driver such as the democli install action steps it
// and honours --stop-before by simply not sending the next event.
namespace etlx::sm {

enum StateId : etl::fsm_state_id_t {
    IdleId = 0,
    DownloadId,
    VerifyId,
    InstallId,
    AwaitCommitId,
    DoneId,
    FailedId,
    NumStates
};

const char* StateName(etl::fsm_state_id_t id);

namespace msg {
enum MessageId {
    StartId = 1,
    NextId,
    CommitId,
    RollbackId,
    AbortId
};
struct Start    : etl::message<StartId> {};
struct Next     : etl::message<NextId> {};
struct Commit   : etl::message<CommitId> {};
struct Rollback : etl::message<RollbackId> {};
struct Abort    : etl::message<AbortId> {};
} // namespace msg

class UpdateFsm : public etl::fsm {
public:
    UpdateFsm();

    // Convenience wrappers around receive(); each returns the resulting state.
    etl::fsm_state_id_t Start()    { receive(msg::Start{});    return get_state_id(); }
    etl::fsm_state_id_t Step()     { receive(msg::Next{});     return get_state_id(); }
    etl::fsm_state_id_t Commit()   { receive(msg::Commit{});   return get_state_id(); }
    etl::fsm_state_id_t Rollback() { receive(msg::Rollback{}); return get_state_id(); }
    etl::fsm_state_id_t Abort()    { receive(msg::Abort{});    return get_state_id(); }

    etl::fsm_state_id_t state() const { return get_state_id(); }
    const char*         state_name() const { return StateName(get_state_id()); }

private:
    // One state object per StateId, registered in order with the base fsm.
    class Idle        : public etl::fsm_state<UpdateFsm, Idle, IdleId, msg::Start> {
    public:
        etl::fsm_state_id_t on_event(const msg::Start&) { return DownloadId; }
        etl::fsm_state_id_t on_event_unknown(const etl::imessage&) { return No_State_Change; }
    };
    class Download    : public etl::fsm_state<UpdateFsm, Download, DownloadId, msg::Next, msg::Abort> {
    public:
        etl::fsm_state_id_t on_event(const msg::Next&)  { return VerifyId; }
        etl::fsm_state_id_t on_event(const msg::Abort&) { return FailedId; }
        etl::fsm_state_id_t on_event_unknown(const etl::imessage&) { return No_State_Change; }
    };
    class Verify      : public etl::fsm_state<UpdateFsm, Verify, VerifyId, msg::Next, msg::Abort> {
    public:
        etl::fsm_state_id_t on_event(const msg::Next&)  { return InstallId; }
        etl::fsm_state_id_t on_event(const msg::Abort&) { return FailedId; }
        etl::fsm_state_id_t on_event_unknown(const etl::imessage&) { return No_State_Change; }
    };
    class Install     : public etl::fsm_state<UpdateFsm, Install, InstallId, msg::Next, msg::Abort> {
    public:
        etl::fsm_state_id_t on_event(const msg::Next&)  { return AwaitCommitId; }
        etl::fsm_state_id_t on_event(const msg::Abort&) { return FailedId; }
        etl::fsm_state_id_t on_event_unknown(const etl::imessage&) { return No_State_Change; }
    };
    class AwaitCommit : public etl::fsm_state<UpdateFsm, AwaitCommit, AwaitCommitId,
                                              msg::Commit, msg::Rollback, msg::Abort> {
    public:
        etl::fsm_state_id_t on_event(const msg::Commit&)   { return DoneId; }
        etl::fsm_state_id_t on_event(const msg::Rollback&) { return FailedId; }
        etl::fsm_state_id_t on_event(const msg::Abort&)    { return FailedId; }
        etl::fsm_state_id_t on_event_unknown(const etl::imessage&) { return No_State_Change; }
    };
    class Done        : public etl::fsm_state<UpdateFsm, Done, DoneId> {
    public:
        etl::fsm_state_id_t on_event_unknown(const etl::imessage&) { return No_State_Change; }
    };
    class Failed      : public etl::fsm_state<UpdateFsm, Failed, FailedId> {
    public:
        etl::fsm_state_id_t on_event_unknown(const etl::imessage&) { return No_State_Change; }
    };

    Idle             idle_;
    Download         download_;
    Verify           verify_;
    Install          install_;
    AwaitCommit      await_commit_;
    Done             done_;
    Failed           failed_;
    etl::ifsm_state* state_list_[NumStates];
};

} // namespace etlx::sm

#endif // ETLX_SM_UPDATE_FSM_HPP
