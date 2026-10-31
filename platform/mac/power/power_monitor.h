#pragma once

#include "core/reliability/engine_mode.h"

class PowerMonitor {
public:
    static bool isOnBatteryPower();
    static bool isLowPowerModeEnabled();
    static ThermalState currentThermalState();
    static EngineMode currentEngineMode();
    static int currentGhostDebounceMs();
};
