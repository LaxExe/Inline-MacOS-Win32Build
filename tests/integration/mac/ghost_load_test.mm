#import <Cocoa/Cocoa.h>
#import <gtest/gtest.h>
#include "core/tier2/diff_safety.h"
#include "core/injection/token_validation.h"

TEST(GhostLoadTest, StaleTokenDropUnderLoad) {
    // Proves that when user types ahead, generation token mismatch safely discards stale tier-2 op
    int sentenceToken = 1;
    InjectionOp slowOp{
        .type = InjectionOp::TYPE_TEXT,
        .count = 0,
        .text = L"corrected sentence",
        .generation_token = sentenceToken,
        .source = InjectionOp::TIER2
    };

    // User moved to sentenceToken 2
    int currentActiveToken = 2;
    EXPECT_FALSE(shouldApplyOp(slowOp, currentActiveToken));
}
