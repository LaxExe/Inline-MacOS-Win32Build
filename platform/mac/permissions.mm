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

bool isSystemInDarkMode() {
    // 1. Inspect any NSStatusBarWindow in the application
    for (NSWindow *window in [NSApp windows]) {
        NSString *name = [window className];
        if ([name containsString:@"Status"] || [name containsString:@"Menu"]) {
            __block bool dark = false;
            [window.effectiveAppearance performAsCurrentDrawingAppearance:^{
                NSColor *c = [[NSColor labelColor] colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
                if (c && ((0.299 * c.redComponent + 0.587 * c.greenComponent + 0.114 * c.blueComponent) > 0.5)) {
                    dark = true;
                }
            }];
            if (dark || [window.effectiveAppearance.name containsString:@"Dark"]) {
                return true;
            }
            return false;
        }
    }

    // 2. Query status bar button drawing appearance
    @try {
        NSStatusBar *bar = [NSStatusBar systemStatusBar];
        NSStatusItem *item = [bar statusItemWithLength:0.0];
        NSStatusBarButton *btn = [item button];
        __block bool isDark = false;
        if (btn) {
            NSAppearance *appr = btn.effectiveAppearance ?: (btn.window ? btn.window.effectiveAppearance : nil);
            if (appr) {
                [appr performAsCurrentDrawingAppearance:^{
                    NSColor *c = [[NSColor labelColor] colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
                    if (c && ((0.299 * c.redComponent + 0.587 * c.greenComponent + 0.114 * c.blueComponent) > 0.5)) {
                        isDark = true;
                    }
                }];
                if (!isDark && [appr.name containsString:@"Dark"]) {
                    isDark = true;
                }
            }
        }
        [bar removeStatusItem:item];
        if (isDark) return true;
    } @catch (id) {}

    // 3. Check Global macOS Interface Style preference
    CFStringRef style = (CFStringRef)CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"), kCFPreferencesAnyApplication);
    if (style) {
        bool isDark = [(__bridge NSString*)style isEqualToString:@"Dark"];
        CFRelease(style);
        if (isDark) return true;
    }

    // 4. Check application effective appearance
    NSAppearance *appAppearance = [NSApp effectiveAppearance];
    if (appAppearance && [appAppearance.name containsString:@"Dark"]) {
        return true;
    }

    return false;
}

static std::function<void()> gAppearanceCallback = nullptr;

void registerAppearanceNotifications(std::function<void()> callback) {
    gAppearanceCallback = callback;

    // Listen for interface theme change
    [[NSDistributedNotificationCenter defaultCenter] addObserverForName:@"AppleInterfaceThemeChangedNotification"
                                                                 object:nil
                                                                  queue:[NSOperationQueue mainQueue]
                                                             usingBlock:^(NSNotification * _Nonnull note) {
        if (gAppearanceCallback) gAppearanceCallback();
    }];

    // Listen for active space/desktop changes (moving between light and dark wallpapers/spaces)
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceActiveSpaceDidChangeNotification
                                                                    object:nil
                                                                     queue:[NSOperationQueue mainQueue]
                                                                usingBlock:^(NSNotification * _Nonnull note) {
        if (gAppearanceCallback) gAppearanceCallback();
    }];

    // Listen for display profile/parameter changes
    [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationDidChangeScreenParametersNotification
                                                      object:nil
                                                       queue:[NSOperationQueue mainQueue]
                                                  usingBlock:^(NSNotification * _Nonnull note) {
        if (gAppearanceCallback) gAppearanceCallback();
    }];
}

void updateNativeStatusBarIcon(bool active) {
    for (NSWindow *w in [NSApp windows]) {
        if ([w isKindOfClass:NSClassFromString(@"NSStatusBarWindow")]) {
            NSStatusBarButton *btn = nil;
            NSMutableArray *queue = [NSMutableArray arrayWithObject:w.contentView];
            while (queue.count > 0) {
                NSView *v = queue.firstObject;
                [queue removeObjectAtIndex:0];
                if ([v isKindOfClass:[NSStatusBarButton class]]) {
                    btn = (NSStatusBarButton *)v;
                    break;
                }
                [queue addObjectsFromArray:v.subviews];
            }
            if (btn) {
                NSImage *img = [NSImage imageWithSize:NSMakeSize(18, 18) flipped:NO drawingHandler:^BOOL(NSRect dstRect) {
                    BOOL isDark = NO;
                    NSAppearance *current = [NSAppearance currentDrawingAppearance];
                    if (current) {
                        NSAppearanceName match = [current bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua, NSAppearanceNameVibrantLight, NSAppearanceNameVibrantDark]];
                        if ([match isEqualToString:NSAppearanceNameDarkAqua] || [match isEqualToString:NSAppearanceNameVibrantDark] || [current.name containsString:@"Dark"]) {
                            isDark = YES;
                        }
                    }
                    if (!isDark) {
                        NSColor *fg = [[NSColor labelColor] colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
                        if (fg && (0.299 * fg.redComponent + 0.587 * fg.greenComponent + 0.114 * fg.blueComponent > 0.45)) {
                            isDark = YES;
                        }
                    }

                    // On dark/black menu bar: pure white #FFFFFF
                    // On light menu bar: dark charcoal #18181B
                    NSColor *barColor = isDark ? [NSColor colorWithCalibratedWhite:1.0 alpha:1.0]
                                               : [NSColor colorWithCalibratedRed:24/255.0 green:24/255.0 blue:27/255.0 alpha:1.0];
                    NSColor *dotColor = active ? [NSColor colorWithCalibratedRed:34.0/255.0 green:197.0/255.0 blue:94.0/255.0 alpha:1.0]
                                               : [NSColor colorWithCalibratedRed:245.0/255.0 green:158.0/255.0 blue:11.0/255.0 alpha:1.0];

                    // Vertical Bar: (x: 5.0, y: 3.0, w: 2.2, h: 12.0)
                    NSBezierPath *barPath = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(5.0, 3.0, 2.2, 12.0) xRadius:0.5 yRadius:0.5];
                    [barColor setFill];
                    [barPath fill];

                    // Status Dot: (x: 9.5, y: 3.2, w: 3.2, h: 3.2)
                    NSBezierPath *dotPath = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(9.5, 3.2, 3.2, 3.2) xRadius:0.6 yRadius:0.6];
                    [dotColor setFill];
                    [dotPath fill];

                    return YES;
                }];
                [img setTemplate:NO];
                [btn setImage:img];
            }
        }
    }
}
