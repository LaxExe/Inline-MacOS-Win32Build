#include "platform/mac/permissions.h"
#import <Cocoa/Cocoa.h>
#import <ApplicationServices/ApplicationServices.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <cctype>

static std::atomic<bool> gLiveEventTapAttached{false};
static std::atomic<bool> gInputMonitoringLatched{false};
static std::atomic<int64_t> gLastImProbeNs{0};

void setLiveEventTapAttached(bool attached) {
    gLiveEventTapAttached.store(attached, std::memory_order_relaxed);
    if (attached) {
        gInputMonitoringLatched.store(true, std::memory_order_relaxed);
    }
}

bool isLiveEventTapAttached() {
    return gLiveEventTapAttached.load(std::memory_order_relaxed);
}

static CGEventRef passiveProbeCallback(CGEventTapProxy, CGEventType, CGEventRef event, void*) {
    return event;
}

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
    if (gInputMonitoringLatched.load(std::memory_order_relaxed)) {
        return true;
    }
    if (CGPreflightListenEventAccess() || isLiveEventTapAttached()) {
        gInputMonitoringLatched.store(true, std::memory_order_relaxed);
        return true;
    }
    // Throttled live check (at most once every 2 seconds) because CGPreflightListenEventAccess()
    // caches initial false for the process lifetime until restarted. Once granted, latches permanently.
    int64_t nowNs = std::chrono::steady_clock::now().time_since_epoch().count();
    int64_t lastNs = gLastImProbeNs.load(std::memory_order_relaxed);
    if (nowNs - lastNs < 2000000000LL) {
        return false;
    }
    gLastImProbeNs.store(nowNs, std::memory_order_relaxed);

    CFMachPortRef tap = CGEventTapCreate(
        kCGHIDEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionListenOnly,
        CGEventMaskBit(kCGEventKeyDown),
        passiveProbeCallback,
        nullptr
    );
    if (tap) {
        CFRelease(tap);
        gInputMonitoringLatched.store(true, std::memory_order_relaxed);
        return true;
    }
    return false;
}

void requestInputMonitoringPermission() {
    if (!CGPreflightListenEventAccess()) {
        CGRequestListenEventAccess();
    }
}

bool checkAccessibilityPermission(bool prompt) {
    NSDictionary *options = @{(__bridge id)kAXTrustedCheckOptionPrompt: @(prompt)};
    return AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
}

void openInputMonitoringSettings() {
    if (!CGPreflightListenEventAccess()) {
        CGRequestListenEventAccess();
    }
    NSURL *url = [NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_ListenEvent"];
    [[NSWorkspace sharedWorkspace] openURL:url];
}

void openAccessibilitySettings() {
    if (!checkAccessibilityPermission(false)) {
        checkAccessibilityPermission(true);
    }
    NSURL *url = [NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"];
    [[NSWorkspace sharedWorkspace] openURL:url];
}

void bringAppToFront() {
    [NSApp activateIgnoringOtherApps:YES];
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

    // 2. Check Global macOS Interface Style preference
    CFStringRef style = (CFStringRef)CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"), kCFPreferencesAnyApplication);
    if (style) {
        bool isDark = [(__bridge NSString*)style isEqualToString:@"Dark"];
        CFRelease(style);
        if (isDark) return true;
    }

    // 3. Check application effective appearance
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

@interface InlineReopenHandler : NSObject
@property (nonatomic, copy) void (^reopenBlock)(void);
- (void)handleReopen:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent;
@end

@implementation InlineReopenHandler
- (void)handleReopen:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent {
    (void)event;
    (void)replyEvent;
    if (self.reopenBlock) {
        self.reopenBlock();
    }
}
@end

static InlineReopenHandler *gReopenHandler = nil;

void registerAppReopenCallback(std::function<void()> callback) {
    if (!gReopenHandler) {
        gReopenHandler = [[InlineReopenHandler alloc] init];
    }
    gReopenHandler.reopenBlock = ^{
        if (callback) callback();
    };
    [[NSAppleEventManager sharedAppleEventManager] setEventHandler:gReopenHandler
                                                       andSelector:@selector(handleReopen:withReplyEvent:)
                                                     forEventClass:kCoreEventClass
                                                        andEventID:kAEReopenApplication];
}

static std::mutex gFrontAppMutex;
static std::string gFrontBundleId;
static std::string gFrontAppName;
static id gGlobalMouseMonitor = nil;
static id gLocalMouseMonitor = nil;
static std::function<void()> gCaretResetCallback = nullptr;

static std::string toLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

static void updateFrontmostAppInfo(NSRunningApplication *app) {
    if (!app) return;
    std::string bundleId = app.bundleIdentifier ? toLowerAscii([app.bundleIdentifier UTF8String]) : "";
    std::string appName = app.localizedName ? toLowerAscii([app.localizedName UTF8String]) : "";
    std::lock_guard<std::mutex> lock(gFrontAppMutex);
    gFrontBundleId = std::move(bundleId);
    gFrontAppName = std::move(appName);
}

void registerPassiveCaretResetMonitors(std::function<void()> onReset) {
    gCaretResetCallback = std::move(onReset);
    updateFrontmostAppInfo([[NSWorkspace sharedWorkspace] frontmostApplication]);

    if (!gGlobalMouseMonitor) {
        NSEventMask mask = NSEventMaskLeftMouseDown | NSEventMaskRightMouseDown | NSEventMaskOtherMouseDown;
        gGlobalMouseMonitor = [NSEvent addGlobalMonitorForEventsMatchingMask:mask handler:^(NSEvent * _Nonnull) {
            if (gCaretResetCallback) gCaretResetCallback();
        }];
        gLocalMouseMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:mask handler:^NSEvent * _Nullable(NSEvent * _Nonnull ev) {
            if (gCaretResetCallback) gCaretResetCallback();
            return ev;
        }];
    }

    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceDidActivateApplicationNotification
                                                                    object:nil
                                                                     queue:[NSOperationQueue mainQueue]
                                                                usingBlock:^(NSNotification * _Nonnull note) {
        NSRunningApplication *app = note.userInfo[NSWorkspaceApplicationKey];
        updateFrontmostAppInfo(app);
        if (gCaretResetCallback) gCaretResetCallback();
    }];
}

bool isFrontmostAppExcluded(const std::vector<std::string>& disabledApps) {
    if (disabledApps.empty()) return false;
    if (!gFrontAppMutex.try_lock()) return false;
    std::string bundleId = gFrontBundleId;
    std::string appName = gFrontAppName;
    gFrontAppMutex.unlock();

    if (bundleId.empty() && appName.empty()) return false;
    for (const auto& raw : disabledApps) {
        if (raw.empty()) continue;
        std::string entry = toLowerAscii(raw);
        if ((!bundleId.empty() && bundleId.find(entry) != std::string::npos) ||
            (!appName.empty() && (appName == entry || appName.find(entry) != std::string::npos))) {
            return true;
        }
    }
    return false;
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
