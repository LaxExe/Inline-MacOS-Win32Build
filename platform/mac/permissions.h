#pragma once

#include <functional>
#include <string>
#include <vector>

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
void requestInputMonitoringPermission();
void openInputMonitoringSettings();
void openAccessibilitySettings();
void setLiveEventTapAttached(bool attached);
bool isLiveEventTapAttached();
void bringAppToFront();
void disableAppNap();
bool isSystemInDarkMode();
void registerAppearanceNotifications(std::function<void()> callback);
void registerAppReopenCallback(std::function<void()> callback);
void registerPassiveCaretResetMonitors(std::function<void()> onReset);
bool isFrontmostAppExcluded(const std::vector<std::string>& disabledApps);
void updateNativeStatusBarIcon(bool active);
