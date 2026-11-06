#include <gtest/gtest.h>
#include "core/reliability/engine_mode.h"

TEST(EngineMode, FullPowerOnACAndNormalThermal) {
    EXPECT_EQ(decideEngineMode(/*onBattery=*/false, ThermalState::Nominal, /*lowPowerMode=*/false), EngineMode::Full);
    EXPECT_EQ(ghostDebounceMsFor(/*onBattery=*/false, ThermalState::Nominal, /*lowPowerMode=*/false), 40);
}
TEST(EngineMode, BatteryUses80MsDebounceAndStaysFullUnlessLowPowerMode) {
    EXPECT_EQ(decideEngineMode(/*onBattery=*/true, ThermalState::Nominal, /*lowPowerMode=*/false), EngineMode::Full);
    EXPECT_EQ(ghostDebounceMsFor(/*onBattery=*/true, ThermalState::Nominal, /*lowPowerMode=*/false), 80);
    EXPECT_EQ(decideEngineMode(/*onBattery=*/true, ThermalState::Nominal, /*lowPowerMode=*/true), EngineMode::IdleOnlyTier2AndNgramGhost);
}
TEST(EngineMode, ThrottlesUnderThermalPressure) {
    EXPECT_EQ(decideEngineMode(/*onBattery=*/false, ThermalState::Serious, /*lowPowerMode=*/false), EngineMode::IdleOnlyTier2AndNgramGhost);
}
