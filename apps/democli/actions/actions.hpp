#ifndef DEMOCLI_ACTIONS_HPP
#define DEMOCLI_ACTIONS_HPP

#include "apps/democli/cli.hpp"

#include <etl/string.h>
#include <etl/string_view.h>

// Concrete actions for democli. Each is constructed in-place inside a static
// ActionPool (see actions.cpp) so no dynamic allocation is needed.
namespace democli {

// Prints the currently-installed artifact name (read from storage in a real
// build; a fixed placeholder here).
class ShowArtifactAction final : public Action {
public:
    etlx::error::Error Execute(Context& ctx) override;
};

// Pretends to query a server for a newer artifact and reports the result.
class CheckUpdateAction final : public Action {
public:
    etlx::error::Error Execute(Context& ctx) override;
};

// Runs the download->verify->install workflow for an artifact. Supports
// --stop-before STATE to halt partway, matching Mender's flag.
class InstallAction final : public Action {
public:
    void SetArtifact(etl::string_view a) { artifact_.assign(a.data(), a.size()); }
    void SetStopBefore(etl::string_view s) { stop_before_.assign(s.data(), s.size()); }
    etlx::error::Error Execute(Context& ctx) override;

private:
    etl::string<128> artifact_;
    etl::string<32>  stop_before_;
};

// Commits or rolls back a pending update. Returns ExitNoUpdate when there is
// nothing in progress.
class CommitAction final : public Action {
public:
    etlx::error::Error Execute(Context& ctx) override;
};

class RollbackAction final : public Action {
public:
    etlx::error::Error Execute(Context& ctx) override;
};

// Prints top-level or per-command help.
class HelpAction final : public Action {
public:
    void SetCommand(etl::string_view c) { command_.assign(c.data(), c.size()); }
    etlx::error::Error Execute(Context& ctx) override;

private:
    etl::string<32> command_;  // empty => top-level help
};

} // namespace democli

#endif // DEMOCLI_ACTIONS_HPP
