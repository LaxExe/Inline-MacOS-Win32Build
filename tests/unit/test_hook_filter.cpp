#include <gtest/gtest.h>
#include "platform/mac/hook/synthetic_filter.h"
#include "platform/mac/hook/tap_recovery.h"

TEST(SyntheticFilter, RecognizesOurOwnTag)        { EXPECT_TRUE(isSyntheticEvent(kOurAppMagicTagValue)); }
TEST(SyntheticFilter, RejectsUntaggedRealEvent)   { EXPECT_FALSE(isSyntheticEvent(0)); }
TEST(SyntheticFilter, RejectsForeignToolsTag)     { EXPECT_FALSE(isSyntheticEvent(0x1234)); }

TEST(TapRecovery, ReenablesOnTimeout)    { EXPECT_TRUE(shouldReenableTap(kCGEventTapDisabledByTimeout)); }
TEST(TapRecovery, ReenablesOnUserInput)  { EXPECT_TRUE(shouldReenableTap(kCGEventTapDisabledByUserInput)); }
TEST(TapRecovery, DoesNotReenableOnKeyDown) { EXPECT_FALSE(shouldReenableTap(kCGEventKeyDown)); }
