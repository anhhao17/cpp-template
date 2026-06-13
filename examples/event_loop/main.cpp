// Example: schedule work on the cooperative event loop. Uses a SimulatedClock
// so the run is deterministic and instant; swap in
// ports::host::MonotonicClock for real wall-clock timing (e.g. a daemon's
// periodic check-update tick). Output via etlx::log. Build target:
// example_event_loop.
#include "etlx/events/loop.hpp"
#include "etlx/log/log.hpp"
#include "host/host_io.hpp"

namespace {

int g_ticks = 0;

void OnImmediate() { ETLX_LOG_INFO("immediate callback ran"); }
void OnTick()      { ETLX_LOG_INFO("tick %d fired", ++g_ticks); }

} // namespace

int main() {
    static etlx::ports::host::StderrLogSink sink;
    etlx::log::SetSink(&sink);

    etlx::time::SimulatedClock clock;
    etlx::events::EventLoop    loop(clock);

    loop.Post(etlx::events::Callback::create<OnImmediate>());
    loop.PostAfter(etlx::time::Seconds(1), etlx::events::Callback::create<OnTick>());
    loop.PostAfter(etlx::time::Seconds(3), etlx::events::Callback::create<OnTick>());
    loop.PostAfter(etlx::time::Seconds(2), etlx::events::Callback::create<OnTick>());

    ETLX_LOG_INFO("running loop with %zu timers...", loop.pending_timers());
    loop.Run();  // returns once every timer has fired
    ETLX_LOG_INFO("loop idle after %d ticks", g_ticks);
    return 0;
}
