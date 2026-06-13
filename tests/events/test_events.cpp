#include "etlx/events/loop.hpp"

#include <vector>

#include <gtest/gtest.h>

namespace ev = etlx::events;
namespace etm = etlx::time;

namespace {

// Free-function callbacks record their order of execution in a shared vector
// (etl::delegate binds free functions without capture).
std::vector<int> g_order;
void F1() { g_order.push_back(1); }
void F2() { g_order.push_back(2); }
void F3() { g_order.push_back(3); }

ev::Callback Cb(void (*fn)()) {
    return ev::Callback(fn);  // delegate from a free-function pointer
}

TEST(EventLoop, ImmediateRunsOnRunOnce) {
    g_order.clear();
    etm::SimulatedClock sim;
    ev::EventLoop loop(sim);

    ASSERT_TRUE(loop.Post(Cb(F1)).has_value());
    EXPECT_EQ(loop.RunOnce(), 1u);
    EXPECT_EQ(g_order, (std::vector<int>{1}));
    EXPECT_TRUE(loop.idle());
}

TEST(EventLoop, TimersFireInDueOrder) {
    g_order.clear();
    etm::SimulatedClock sim;
    ev::EventLoop loop(sim);

    loop.PostAfter(etm::Seconds(3), Cb(F3));
    loop.PostAfter(etm::Seconds(1), Cb(F1));
    loop.PostAfter(etm::Seconds(2), Cb(F2));

    loop.Run();
    EXPECT_EQ(g_order, (std::vector<int>{1, 2, 3}));
    EXPECT_TRUE(loop.idle());
}

TEST(EventLoop, RunOnceOnlyFiresDueTimers) {
    g_order.clear();
    etm::SimulatedClock sim;
    ev::EventLoop loop(sim);

    loop.PostAfter(etm::Seconds(5), Cb(F1));
    EXPECT_EQ(loop.RunOnce(), 0u);  // not due yet
    EXPECT_EQ(loop.pending_timers(), 1u);

    sim.Advance(etm::Seconds(5));
    EXPECT_EQ(loop.RunOnce(), 1u);  // now due
    EXPECT_EQ(g_order, (std::vector<int>{1}));
}

TEST(EventLoop, PostReturnsQueueFull) {
    etm::SimulatedClock sim;
    ev::EventLoop loop(sim);

    etlx::Status last = etlx::Ok();
    for (int i = 0; i < ETLX_EVENTS_QUEUE_DEPTH + 4; ++i) {
        last = loop.Post(Cb(F1));
    }
    ASSERT_FALSE(last.has_value());
    EXPECT_EQ(last.error().code, ev::QueueFull);
}

} // namespace
