#include "etlx/events/loop.hpp"

namespace etlx::events {

const error::Category Category{"events"};

Status EventLoop::Post(Callback cb) {
    if (immediate_.full()) return Fail(Category, QueueFull, "immediate queue full");
    immediate_.push(cb);
    return Ok();
}

Status EventLoop::PostAfter(time::Duration delay, Callback cb) {
    if (timers_.full()) return Fail(Category, QueueFull, "timer table full");
    timers_.push_back(TimerEntry{clock_.Now() + delay, cb});
    return Ok();
}

int EventLoop::EarliestTimer() const {
    int  best     = -1;
    for (size_t i = 0; i < timers_.size(); ++i) {
        if (best < 0 || timers_[i].due < timers_[static_cast<size_t>(best)].due) {
            best = static_cast<int>(i);
        }
    }
    return best;
}

size_t EventLoop::RunOnce() {
    size_t ran = 0;

    // Drain immediate callbacks queued so far. A callback may enqueue more work;
    // that runs on the next iteration, keeping each RunOnce bounded.
    const size_t batch = immediate_.size();
    for (size_t i = 0; i < batch; ++i) {
        Callback cb = immediate_.front();
        immediate_.pop();
        cb();
        ++ran;
    }

    // Fire every timer whose deadline has passed.
    const time::TimePoint now = clock_.Now();
    bool fired = true;
    while (fired) {
        fired = false;
        for (size_t i = 0; i < timers_.size(); ++i) {
            if (!(now < timers_[i].due)) {  // due <= now
                Callback cb = timers_[i].cb;
                timers_.erase(timers_.begin() + i);
                cb();
                ++ran;
                fired = true;
                break;  // restart: the callback may have changed the table
            }
        }
    }
    return ran;
}

void EventLoop::Run() {
    running_ = true;
    while (running_) {
        RunOnce();
        if (immediate_.empty()) {
            if (timers_.empty()) break;  // nothing left to do
            // Sleep until the next timer is due, then loop to fire it.
            const int idx = EarliestTimer();
            clock_.SleepUntil(timers_[static_cast<size_t>(idx)].due);
        }
    }
    running_ = false;
}

} // namespace etlx::events
