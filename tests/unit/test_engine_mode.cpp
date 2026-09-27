#include <gtest/gtest.h>
#include "core/reliability/engine_mode.h"

TEST(EngineMode, FullPowerOnACAndNormalThermal) {
    EXPECT_EQ(decideEngineMode(/*onBattery=*/false, ThermalState::Nominal), EngineMode::Full);
}
TEST(EngineMode, ThrottlesOnBattery) {
    EXPECT_EQ(decideEngineMode(/*onBattery=*/true, ThermalState::Nominal), EngineMode::IdleOnlyTier2AndNgramGhost);
}
TEST(EngineMode, ThrottlesUnderThermalPressure) {
    EXPECT_EQ(decideEngineMode(/*onBattery=*/false, ThermalState::Serious), EngineMode::IdleOnlyTier2AndNgramGhost);
}
