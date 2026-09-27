#include <gtest/gtest.h>
#include "gui/hotkey_blocklist.h"

TEST(HotkeyBlocklist, RejectsCmdQ)    { EXPECT_TRUE(isHotkeyBlocklisted(KeyCode::Q, Mod::Cmd)); }
TEST(HotkeyBlocklist, AllowsDefault)  { EXPECT_FALSE(isHotkeyBlocklisted(KeyCode::K, Mod::Cmd | Mod::Option | Mod::Shift)); }
