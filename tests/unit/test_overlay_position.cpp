#include <gtest/gtest.h>
#include "platform/mac/caret/overlay_position.h"

TEST(OverlayPosition, UsesAxBoundsWhenAvailable) {
    auto decision = decideOverlayPosition(CGRect{{10,20},{5,14}}, {}, false);
    EXPECT_TRUE(decision.visible);
    EXPECT_EQ(decision.rect.origin.x, 10);
}
TEST(OverlayPosition, FallsBackToHeuristicWhenAxUnavailable) {
    HeuristicEstimate h{ .x = 42, .confident = true };
    auto decision = decideOverlayPosition(std::nullopt, h, false);
    EXPECT_TRUE(decision.visible);
    EXPECT_EQ(decision.rect.origin.x, 42);
}
TEST(OverlayPosition, HidesWhenHeuristicInvalidated) {
    HeuristicEstimate h{ .x = 42, .confident = true };
    auto decision = decideOverlayPosition(std::nullopt, h, /*invalidatedSinceLastGood=*/true);
    EXPECT_FALSE(decision.visible); // must hide, never show at a stale/low-confidence position
}
TEST(OverlayPosition, HidesWhenNothingAvailable) {
    auto decision = decideOverlayPosition(std::nullopt, HeuristicEstimate{.confident=false}, false);
    EXPECT_FALSE(decision.visible);
}
