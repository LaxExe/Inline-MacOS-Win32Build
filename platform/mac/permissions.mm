#include "platform/mac/permissions.h"
#import <Cocoa/Cocoa.h>
#import <ApplicationServices/ApplicationServices.h>

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

static id gAppNapActivity = nil;

void disableAppNap() {
    if (!gAppNapActivity) {
        gAppNapActivity = [[NSProcessInfo processInfo] beginActivityWithOptions:(NSActivityUserInitiatedAllowingIdleSystemSleep | NSActivityLatencyCritical)
                                                                         reason:@"Realtime keystroke correction"];
    }
}

bool checkInputMonitoringPermission() {
    return CGPreflightListenEventAccess();
}

bool checkAccessibilityPermission(bool prompt) {
    NSDictionary *options = @{(__bridge id)kAXTrustedCheckOptionPrompt: @(prompt)};
    return AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
}

void openInputMonitoringSettings() {
    NSURL *url = [NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_ListenEvent"];
    [[NSWorkspace sharedWorkspace] openURL:url];
}

void openAccessibilitySettings() {
    NSURL *url = [NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"];
    [[NSWorkspace sharedWorkspace] openURL:url];
}
