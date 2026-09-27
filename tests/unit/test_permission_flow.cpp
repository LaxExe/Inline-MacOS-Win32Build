#include <gtest/gtest.h>
#include "platform/mac/permissions.h"

TEST(PermissionFlow, FirstRunBothMissingPromptsInputMonitoringFirst) {
    auto action = decidePermissionAction(/*inputMon=*/false, /*accessibility=*/false, /*firstRun=*/true);
    EXPECT_EQ(action, PermissionAction::PromptInputMonitoring);
}
TEST(PermissionFlow, InputMonitoringGrantedThenPromptsAccessibility) {
    auto action = decidePermissionAction(/*inputMon=*/true, /*accessibility=*/false, /*firstRun=*/true);
    EXPECT_EQ(action, PermissionAction::PromptAccessibility);
}
TEST(PermissionFlow, BothGrantedProceeds) {
    auto action = decidePermissionAction(/*inputMon=*/true, /*accessibility=*/true, /*firstRun=*/false);
    EXPECT_EQ(action, PermissionAction::Proceed);
}
TEST(PermissionFlow, LaterRunMissingOneSilentlyDirectsToSettings) {
    // not first run: no re-prompt dialog, just a directed Settings link, per §4
    auto action = decidePermissionAction(/*inputMon=*/false, /*accessibility=*/true, /*firstRun=*/false);
    EXPECT_EQ(action, PermissionAction::DirectToInputMonitoringSettings);
}
