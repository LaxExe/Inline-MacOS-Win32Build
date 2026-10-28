#include "platform/win/permissions.h"

PermissionAction decidePermissionAction(bool inputMonitoringGranted, bool accessibilityGranted, bool isFirstRun) {
    if (inputMonitoringGranted && accessibilityGranted) {
        return PermissionAction::Proceed;
    }
    if (isFirstRun) {
        if (!inputMonitoringGranted) {
            return PermissionAction::PromptInputMonitoring;
        }
        return PermissionAction::PromptAccessibility;
    }
    if (!inputMonitoringGranted) {
        return PermissionAction::DirectToInputMonitoringSettings;
    }
    return PermissionAction::DirectToAccessibilitySettings;
}

bool checkInputMonitoringPermission() {
    return true;
}

bool checkAccessibilityPermission(bool prompt) {
    (void)prompt;
    return true;
}

void openInputMonitoringSettings() {
}

void openAccessibilitySettings() {
}
