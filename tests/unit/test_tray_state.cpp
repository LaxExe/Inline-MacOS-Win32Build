#include <gtest/gtest.h>
#include "gui/tray_state.h"

TEST(TrayState, ShowsActiveIconWhenEnabled)  { EXPECT_EQ(iconStateFor(/*masterEnabled=*/true),  TrayIconState::Active); }
TEST(TrayState, ShowsPausedIconWhenDisabled) { EXPECT_EQ(iconStateFor(/*masterEnabled=*/false), TrayIconState::Paused); }
