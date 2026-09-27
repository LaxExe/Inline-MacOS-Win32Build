#include <gtest/gtest.h>
#include "core/injection/token_validation.h"

TEST(TokenValidation, AppliesMatchingToken) {
    InjectionOp op{InjectionOp::TYPE_TEXT, 0, L"the", /*generation_token=*/5, InjectionOp::TIER1};
    EXPECT_TRUE(shouldApplyOp(op, /*currentTokenForSource=*/5));
}
TEST(TokenValidation, DropsStaleToken) {
    InjectionOp op{InjectionOp::TYPE_TEXT, 0, L"the", /*generation_token=*/5, InjectionOp::TIER1};
    EXPECT_FALSE(shouldApplyOp(op, /*currentTokenForSource=*/6)); // user moved on
}
