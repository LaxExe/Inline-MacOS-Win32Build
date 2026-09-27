#import <Cocoa/Cocoa.h>
#import <gtest/gtest.h>
#include "platform/mac/hook/synthetic_filter.h"
#include "platform/mac/hook/mac_keyboard_hook.h"

TEST(HookIntegration, FiltersSynthesizedEvents) {
    // 1. Verify synthetic tagging is recognized
    EXPECT_TRUE(isSyntheticEvent(kOurAppMagicTagValue));
    EXPECT_FALSE(isSyntheticEvent(0));
}
