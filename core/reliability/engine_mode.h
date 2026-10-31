#pragma once

enum class ThermalState {
    Nominal,
    Fair,
    Serious,
    Critical
};

enum class EngineMode {
    Full,
    IdleOnlyTier2AndNgramGhost
};

EngineMode decideEngineMode(bool onBattery, ThermalState thermal, bool lowPowerMode = false);
int ghostDebounceMsFor(bool onBattery, ThermalState thermal, bool lowPowerMode = false);
