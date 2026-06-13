// Example: schedule work on the cooperative event loop. Uses a SimulatedClock
// so the run is deterministic and instant; swap in
// ports::host::MonotonicClock for real wall-clock timing (e.g. a daemon's
// periodic check-update tick). Build target: example_event_loop.
#include "etlx/events/loop.hpp"

#include <cstdio>

namespace {

int g_ticks = 0;

void OnImmediate() { std::printf("immediate callback ran\n"); }
void OnTick()      { std::printf("tick %d fired\n", ++g_ticks); }

} // namespace

int main() {
    etlx::time::SimulatedClock clock;
    etlx::events::EventLoop    loop(clock);

    loop.Post(etlx::events::Callback::create<OnImmediate>());
    loop.PostAfter(etlx::time::Seconds(1), etlx::events::Callback::create<OnTick>());
    loop.PostAfter(etlx::time::Seconds(3), etlx::events::Callback::create<OnTick>());
    loop.PostAfter(etlx::time::Seconds(2), etlx::events::Callback::create<OnTick>());

    std::printf("running loop with %zu timers...\n", loop.pending_timers());
    loop.Run();  // returns once every timer has fired
    std::printf("loop idle after %d ticks\n", g_ticks);
    return 0;
}
