#ifndef ETLX_EVENTS_LOOP_HPP
#define ETLX_EVENTS_LOOP_HPP

#include "etlx/core/result.hpp"
#include "etlx/core/time.hpp"
#include "etlx/etlx_config.hpp"

#include <etl/delegate.h>
#include <etl/queue.h>
#include <etl/vector.h>

// Single-threaded cooperative event loop with timers, in the spirit of
// Mender's asio loop but tiny. Callbacks are etl::delegate (no std::function /
// heap); the immediate queue and timer table are fixed-capacity. The loop idles
// between timers via the Clock, so it works on a host or wraps an RTOS tick.
namespace etlx::events {

extern const error::Category Category;

enum Code : error::Code {
    None      = 0,
    QueueFull = 1,
    NoTimers  = 2,
};

using Callback = etl::delegate<void()>;

class EventLoop {
public:
    explicit EventLoop(time::Clock& clock) : clock_(clock) {}

    // Schedules a callback to run on the next RunOnce/Run iteration.
    Status Post(Callback cb);

    // Schedules a callback to run once after `delay`.
    Status PostAfter(time::Duration delay, Callback cb);

    // Runs all currently-queued immediate callbacks and any timers already due,
    // without sleeping. Returns the number of callbacks invoked.
    size_t RunOnce();

    // Runs until there is no pending work, sleeping via the Clock until the next
    // timer is due. Stop() makes it return after the current iteration.
    void Run();

    void Stop() { running_ = false; }

    size_t pending_immediate() const { return immediate_.size(); }
    size_t pending_timers() const { return timers_.size(); }
    bool   idle() const { return immediate_.empty() && timers_.empty(); }

private:
    struct TimerEntry {
        time::TimePoint due;
        Callback        cb;
    };

    // Returns the index of the soonest timer, or -1 if none.
    int EarliestTimer() const;

    time::Clock&                                          clock_;
    etl::queue<Callback, ETLX_EVENTS_QUEUE_DEPTH>         immediate_;
    etl::vector<TimerEntry, ETLX_EVENTS_MAX_TIMERS>       timers_;
    bool                                                  running_ = false;
};

} // namespace etlx::events

#endif // ETLX_EVENTS_LOOP_HPP
