#pragma once

enum class PermissionAction {
    PromptInputMonitoring,
    PromptAccessibility,
    Proceed,
    DirectToInputMonitoringSettings,
    DirectToAccessibilitySettings
};

PermissionAction decidePermissionAction(bool inputMonitoringGranted, bool accessibilityGranted, bool isFirstRun);

bool checkInputMonitoringPermission();
bool checkAccessibilityPermission(bool prompt);
void openInputMonitoringSettings();
void openAccessibilitySettings();
void disableAppNap();
