#include "core/reliability/engine_mode.h"

EngineMode decideEngineMode(bool onBattery, ThermalState thermal) {
    if (onBattery || thermal == ThermalState::Serious || thermal == ThermalState::Critical) {
        return EngineMode::IdleOnlyTier2AndNgramGhost;
    }
    return EngineMode::Full;
}
