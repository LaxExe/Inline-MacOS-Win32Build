#include <gtest/gtest.h>
#include "core/injection/priority_queue.h"

TEST(PriorityQueue, GhostAcceptJumpsQueue) {
    std::deque<InjectionOp> q;
    enqueueOp(q, InjectionOp{InjectionOp::TYPE_TEXT, 0, L"foo", 1, InjectionOp::TIER2});
    enqueueOp(q, InjectionOp{InjectionOp::TYPE_TEXT, 0, L"bar", 1, InjectionOp::GHOST_ACCEPT});
    ASSERT_EQ(q.size(), 2u);
    EXPECT_EQ(q.front().source, InjectionOp::GHOST_ACCEPT); // jumped ahead of the already-queued tier-2 op
}
TEST(PriorityQueue, NormalOpsAreFIFO) {
    std::deque<InjectionOp> q;
    enqueueOp(q, InjectionOp{InjectionOp::TYPE_TEXT, 0, L"a", 1, InjectionOp::TIER1});
    enqueueOp(q, InjectionOp{InjectionOp::TYPE_TEXT, 0, L"b", 1, InjectionOp::TIER1});
    EXPECT_EQ(q.front().text, L"a");
}
