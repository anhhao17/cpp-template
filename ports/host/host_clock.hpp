#ifndef ETLX_PORTS_HOST_CLOCK_HPP
#define ETLX_PORTS_HOST_CLOCK_HPP

#include "etlx/core/time.hpp"

// Host monotonic clock: Now() via CLOCK_MONOTONIC, SleepUntil() via nanosleep.
// An MCU port would read a tick counter and idle the core instead.
namespace etlx::ports::host {

class MonotonicClock final : public etlx::time::Clock {
public:
    etlx::time::TimePoint Now() override;
    void                  SleepUntil(etlx::time::TimePoint t) override;
};

} // namespace etlx::ports::host

#endif // ETLX_PORTS_HOST_CLOCK_HPP
