#ifndef ETLX_CORE_TIME_HPP
#define ETLX_CORE_TIME_HPP

#include <cstdint>

// Minimal time abstraction. Durations and time points are plain millisecond
// counts so they cost nothing and need no <chrono>; a platform port supplies
// the actual clock via Clock::NowMs(). Keeping this independent of the host
// clock is what lets tests inject a fake clock.
namespace etlx::time {

// A span of time in milliseconds.
struct Duration {
    int64_t ms = 0;

    constexpr Duration() = default;
    constexpr explicit Duration(int64_t milliseconds) : ms(milliseconds) {}

    constexpr Duration operator+(Duration o) const { return Duration{ms + o.ms}; }
    constexpr Duration operator-(Duration o) const { return Duration{ms - o.ms}; }
    constexpr bool operator<(Duration o) const { return ms < o.ms; }
    constexpr bool operator==(Duration o) const { return ms == o.ms; }
};

// An absolute instant, measured in milliseconds since an arbitrary epoch chosen
// by the clock port.
struct TimePoint {
    int64_t ms = 0;

    constexpr TimePoint() = default;
    constexpr explicit TimePoint(int64_t milliseconds) : ms(milliseconds) {}

    constexpr Duration  operator-(TimePoint o) const { return Duration{ms - o.ms}; }
    constexpr TimePoint operator+(Duration d) const { return TimePoint{ms + d.ms}; }
    constexpr bool operator<(TimePoint o) const { return ms < o.ms; }
    constexpr bool operator==(TimePoint o) const { return ms == o.ms; }
};

constexpr Duration Milliseconds(int64_t n) { return Duration{n}; }
constexpr Duration Seconds(int64_t n) { return Duration{n * 1000}; }

// Abstract monotonic clock. Ports (host, RTOS, bare-metal) implement Now() and
// SleepUntil(); the event loop uses both so it can idle between timers.
struct Clock {
    virtual TimePoint Now() = 0;
    // Blocks (or, for a simulated clock, advances) until at least `t`.
    virtual void SleepUntil(TimePoint t) = 0;
    virtual ~Clock() = default;
};

// A deterministic clock for tests: time only moves when SleepUntil advances it.
class SimulatedClock final : public Clock {
public:
    explicit SimulatedClock(TimePoint start = TimePoint{0}) : now_(start) {}
    TimePoint Now() override { return now_; }
    void SleepUntil(TimePoint t) override { if (now_ < t) now_ = t; }
    void Advance(Duration d) { now_ = now_ + d; }

private:
    TimePoint now_;
};

} // namespace etlx::time

#endif // ETLX_CORE_TIME_HPP
