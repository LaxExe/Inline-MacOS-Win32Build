#include <gtest/gtest.h>
#include "core/tier2/supervisor_policy.h"

TEST(SupervisorPolicy, RestartsOnCrash) {
    EXPECT_EQ(decideSupervisorAction(ProcessState::Crashed, 0, 500), SupervisorAction::KillAndRestart);
}
TEST(SupervisorPolicy, RestartsOnTimeoutExceeded) {
    EXPECT_EQ(decideSupervisorAction(ProcessState::Running, /*elapsed=*/600, /*ceiling=*/500), SupervisorAction::KillAndRestart);
}
TEST(SupervisorPolicy, LeavesHealthyProcessAlone) {
    EXPECT_EQ(decideSupervisorAction(ProcessState::Running, /*elapsed=*/120, /*ceiling=*/500), SupervisorAction::None);
}
