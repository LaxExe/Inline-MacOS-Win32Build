#include "platform/mac/power/power_monitor.h"
#import <Foundation/Foundation.h>
#import <IOKit/ps/IOPowerSources.h>
#import <IOKit/ps/IOPSKeys.h>

bool PowerMonitor::isOnBatteryPower() {
    @autoreleasepool {
        CFTypeRef blob = IOPSCopyPowerSourcesInfo();
        if (!blob) return false;
        CFStringRef type = IOPSGetProvidingPowerSourceType(blob);
        bool onBattery = false;
        if (type && CFEqual(type, CFSTR(kIOPSBatteryPowerValue))) {
            onBattery = true;
        }
        CFRelease(blob);
        return onBattery;
    }
}

bool PowerMonitor::isLowPowerModeEnabled() {
    @autoreleasepool {
        if (@available(macOS 12.0, *)) {
            return [[NSProcessInfo processInfo] isLowPowerModeEnabled] ? true : false;
        }
        return false;
    }
}

ThermalState PowerMonitor::currentThermalState() {
    @autoreleasepool {
        NSProcessInfoThermalState s = [[NSProcessInfo processInfo] thermalState];
        switch (s) {
            case NSProcessInfoThermalStateNominal:  return ThermalState::Nominal;
            case NSProcessInfoThermalStateFair:     return ThermalState::Fair;
            case NSProcessInfoThermalStateSerious:  return ThermalState::Serious;
            case NSProcessInfoThermalStateCritical: return ThermalState::Critical;
        }
        return ThermalState::Nominal;
    }
}

EngineMode PowerMonitor::currentEngineMode() {
    return decideEngineMode(isOnBatteryPower(), currentThermalState(), isLowPowerModeEnabled());
}

int PowerMonitor::currentGhostDebounceMs() {
    return ghostDebounceMsFor(isOnBatteryPower(), currentThermalState(), isLowPowerModeEnabled());
}
