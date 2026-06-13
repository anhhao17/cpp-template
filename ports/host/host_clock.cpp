#include "host/host_clock.hpp"

#include <ctime>

namespace etlx::ports::host {

namespace tm = etlx::time;

tm::TimePoint MonotonicClock::Now() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const int64_t ms = static_cast<int64_t>(ts.tv_sec) * 1000 +
                       static_cast<int64_t>(ts.tv_nsec) / 1000000;
    return tm::TimePoint{ms};
}

void MonotonicClock::SleepUntil(tm::TimePoint t) {
    const tm::TimePoint now = Now();
    if (!(now < t)) return;  // already past the deadline
    const int64_t delta_ms = (t - now).ms;
    timespec req{};
    req.tv_sec  = static_cast<time_t>(delta_ms / 1000);
    req.tv_nsec = static_cast<long>((delta_ms % 1000) * 1000000);
    nanosleep(&req, nullptr);
}

} // namespace etlx::ports::host
