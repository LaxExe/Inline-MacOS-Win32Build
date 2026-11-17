#include <gtest/gtest.h>
#include "gui/hotkey_blocklist.h"

TEST(HotkeyBlocklist, RejectsCmdQ)    { EXPECT_TRUE(isHotkeyBlocklisted(KeyCode::Q, Mod::Cmd)); }
TEST(HotkeyBlocklist, AllowsDefault)  { EXPECT_FALSE(isHotkeyBlocklisted(KeyCode::K, Mod::Cmd | Mod::Option | Mod::Shift)); }
TEST(HotkeyBlocklist, AllowsAddToKnownWordsHotkeys) {
    EXPECT_FALSE(isHotkeyBlocklisted(KeyCode::D, Mod::Cmd | Mod::Option | Mod::Shift));
    EXPECT_FALSE(isHotkeyBlocklisted(KeyCode::I, Mod::Cmd | Mod::Option | Mod::Shift));
    EXPECT_FALSE(isHotkeyBlocklisted(KeyCode::G, Mod::Cmd | Mod::Option | Mod::Shift));
}
