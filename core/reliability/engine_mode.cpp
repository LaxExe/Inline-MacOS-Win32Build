#include "core/reliability/engine_mode.h"

EngineMode decideEngineMode(bool onBattery, ThermalState thermal, bool lowPowerMode) {
    (void)onBattery;
    if (lowPowerMode || thermal == ThermalState::Serious || thermal == ThermalState::Critical) {
        return EngineMode::IdleOnlyTier2AndNgramGhost;
    }
    return EngineMode::Full;
}

int ghostDebounceMsFor(bool onBattery, ThermalState thermal, bool lowPowerMode) {
    if (lowPowerMode || thermal == ThermalState::Serious || thermal == ThermalState::Critical) {
        return 60;
    }
    if (onBattery) {
        return 80;
    }
    return 40;
}
