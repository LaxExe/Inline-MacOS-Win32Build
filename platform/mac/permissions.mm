#include "platform/mac/permissions.h"
#include "platform/mac/caret/mac_caret_tracker.h"
#include "platform/mac/hook/synthetic_filter.h"
#import <Cocoa/Cocoa.h>
#import <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <unistd.h>
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
        if ([name containsString:@"Status"]) {
            NSAppearance *app = window.effectiveAppearance;
            if (app) {
                NSAppearanceName match = [app bestMatchFromAppearancesWithNames:@[
                    NSAppearanceNameAqua, NSAppearanceNameDarkAqua,
                    NSAppearanceNameVibrantLight, NSAppearanceNameVibrantDark
                ]];
                if ([match isEqualToString:NSAppearanceNameDarkAqua] ||
                    [match isEqualToString:NSAppearanceNameVibrantDark]) {
                    return true;
                }
                if ([match isEqualToString:NSAppearanceNameAqua] ||
                    [match isEqualToString:NSAppearanceNameVibrantLight]) {
                    return false;
                }
            }
        }
    }

    // 2. Global macOS Interface Style preference
    CFStringRef style = (CFStringRef)CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"), kCFPreferencesAnyApplication);
    if (style) {
        bool isDark = [(__bridge NSString*)style isEqualToString:@"Dark"];
        CFRelease(style);
        if (isDark) return true;
    }

    // 3. Application effective appearance
    NSAppearance *appAppearance = [NSApp effectiveAppearance];
    if (appAppearance) {
        NSAppearanceName match = [appAppearance bestMatchFromAppearancesWithNames:@[
            NSAppearanceNameAqua, NSAppearanceNameDarkAqua
        ]];
        if ([match isEqualToString:NSAppearanceNameDarkAqua]) {
            return true;
        }
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

    // Warm up accessibility immediately if this is a Chromium/Electron/browser application
    if ([app processIdentifier] > 0) {
        MacCaretTracker::enableChromiumAccessibilityIfNeeded([app processIdentifier]);
    }
}

void registerPassiveCaretResetMonitors(std::function<void()> onReset) {
    gCaretResetCallback = std::move(onReset);
    updateFrontmostAppInfo([[NSWorkspace sharedWorkspace] frontmostApplication]);

    if (!gGlobalMouseMonitor) {
        NSEventMask mask = NSEventMaskLeftMouseDown | NSEventMaskRightMouseDown | NSEventMaskOtherMouseDown | NSEventMaskScrollWheel;
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

    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceActiveSpaceDidChangeNotification
                                                                    object:nil
                                                                     queue:[NSOperationQueue mainQueue]
                                                                usingBlock:^(NSNotification * _Nonnull) {
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

void updateNativeStatusBarIcon(bool active, int /*themeOverride*/) {
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
                    // Check actual macOS status bar appearance:
                    // When the menu bar is dark/black, the bar is pure white #FFFFFF.
                    // When the menu bar is light, the bar is dark charcoal #18181B.
                    BOOL isDark = YES; // Default to dark menu bar for high contrast
                    NSAppearance *current = [NSAppearance currentDrawingAppearance];
                    if (!current && btn) current = btn.effectiveAppearance;
                    if (!current && btn.window) current = btn.window.effectiveAppearance;
                    if (current) {
                        NSAppearanceName match = [current bestMatchFromAppearancesWithNames:@[
                            NSAppearanceNameAqua, NSAppearanceNameDarkAqua,
                            NSAppearanceNameVibrantLight, NSAppearanceNameVibrantDark
                        ]];
                        if ([match isEqualToString:NSAppearanceNameDarkAqua] ||
                            [match isEqualToString:NSAppearanceNameVibrantDark] ||
                            [current.name containsString:@"Dark"]) {
                            isDark = YES;
                        } else if ([match isEqualToString:NSAppearanceNameAqua] ||
                                   [match isEqualToString:NSAppearanceNameVibrantLight]) {
                            isDark = NO;
                        }
                    } else {
                        isDark = isSystemInDarkMode();
                    }

                    // On dark/black menu bar: pure white #FFFFFF
                    // On light menu bar: dark charcoal #18181B
                    NSColor *barColor = isDark ? [NSColor colorWithCalibratedWhite:1.0 alpha:1.0]
                                               : [NSColor colorWithCalibratedRed:24/255.0 green:24/255.0 blue:27/255.0 alpha:1.0];
                    NSColor *dotColor = active ? [NSColor colorWithCalibratedRed:34.0/255.0 green:197.0/255.0 blue:94.0/255.0 alpha:1.0]
                                               : [NSColor colorWithCalibratedRed:245.0/255.0 green:158.0/255.0 blue:11.0/255.0 alpha:1.0];

                    // Vertical Bar: reduced width from 2.2 to 1.4 for a sleek, thin profile
                    NSBezierPath *barPath = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(5.5, 3.0, 1.4, 12.0) xRadius:0.5 yRadius:0.5];
                    [barColor setFill];
                    [barPath fill];

                    // Status Dot:
                    NSBezierPath *dotPath = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(8.8, 3.2, 3.0, 3.0) xRadius:0.6 yRadius:0.6];
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

std::string getSelectedTextFromFrontmostApp() {
    @autoreleasepool {
        NSRunningApplication* front = [[NSWorkspace sharedWorkspace] frontmostApplication];
        pid_t frontPid = front ? [front processIdentifier] : 0;
        if (frontPid <= 0 || frontPid == getpid()) return "";

        // 1. Accessibility query: fast, zero clipboard alteration
        AXUIElementRef elem = nullptr;
        AXUIElementRef appElem = AXUIElementCreateApplication(frontPid);
        if (appElem) {
            AXUIElementSetMessagingTimeout(appElem, 0.08f);
            AXUIElementSetAttributeValue(appElem, CFSTR("AXEnhancedUserInterface"), kCFBooleanTrue);
            AXUIElementSetAttributeValue(appElem, CFSTR("AXManualAccessibility"), kCFBooleanTrue);

            CFTypeRef focusVal = nullptr;
            if (AXUIElementCopyAttributeValue(appElem, kAXFocusedUIElementAttribute, &focusVal) == kAXErrorSuccess && focusVal) {
                if (CFGetTypeID(focusVal) == AXUIElementGetTypeID()) {
                    elem = static_cast<AXUIElementRef>(focusVal);
                } else {
                    CFRelease(focusVal);
                }
            }

            if (!elem) {
                CFTypeRef winVal = nullptr;
                if (AXUIElementCopyAttributeValue(appElem, kAXFocusedWindowAttribute, &winVal) == kAXErrorSuccess && winVal) {
                    if (CFGetTypeID(winVal) == AXUIElementGetTypeID()) {
                        CFTypeRef winFocus = nullptr;
                        if (AXUIElementCopyAttributeValue(static_cast<AXUIElementRef>(winVal), kAXFocusedUIElementAttribute, &winFocus) == kAXErrorSuccess && winFocus) {
                            if (CFGetTypeID(winFocus) == AXUIElementGetTypeID()) {
                                elem = static_cast<AXUIElementRef>(winFocus);
                            } else {
                                CFRelease(winFocus);
                            }
                        }
                    }
                    CFRelease(winVal);
                }
            }
            CFRelease(appElem);
        }

        if (!elem) {
            AXUIElementRef sys = AXUIElementCreateSystemWide();
            if (sys) {
                AXUIElementSetMessagingTimeout(sys, 0.08f);
                CFTypeRef sysFocus = nullptr;
                if (AXUIElementCopyAttributeValue(sys, kAXFocusedUIElementAttribute, &sysFocus) == kAXErrorSuccess && sysFocus) {
                    if (CFGetTypeID(sysFocus) == AXUIElementGetTypeID()) {
                        elem = static_cast<AXUIElementRef>(sysFocus);
                    } else {
                        CFRelease(sysFocus);
                    }
                }
                CFRelease(sys);
            }
        }

        if (elem) {
            AXUIElementSetMessagingTimeout(elem, 0.08f);
            CFTypeRef selVal = nullptr;
            if (AXUIElementCopyAttributeValue(elem, kAXSelectedTextAttribute, &selVal) == kAXErrorSuccess && selVal) {
                if (CFGetTypeID(selVal) == CFStringGetTypeID()) {
                    NSString* ns = (__bridge NSString*)selVal;
                    if (ns && [ns length] > 0) {
                        std::string res = [ns UTF8String];
                        CFRelease(selVal);
                        CFRelease(elem);
                        return res;
                    }
                }
                CFRelease(selVal);
            }

            CFTypeRef rangeVal = nullptr;
            if (AXUIElementCopyAttributeValue(elem, kAXSelectedTextRangeAttribute, &rangeVal) == kAXErrorSuccess && rangeVal) {
                if (CFGetTypeID(rangeVal) == AXValueGetTypeID()) {
                    CFRange range;
                    if (AXValueGetValue(static_cast<AXValueRef>(rangeVal), kAXValueTypeCFRange, &range) && range.length > 0) {
                        CFTypeRef strVal = nullptr;
                        if (AXUIElementCopyParameterizedAttributeValue(elem, kAXStringForRangeParameterizedAttribute, rangeVal, &strVal) == kAXErrorSuccess && strVal) {
                            if (CFGetTypeID(strVal) == CFStringGetTypeID()) {
                                NSString* ns = (__bridge NSString*)strVal;
                                if (ns && [ns length] > 0) {
                                    std::string res = [ns UTF8String];
                                    CFRelease(strVal);
                                    CFRelease(rangeVal);
                                    CFRelease(elem);
                                    return res;
                                }
                            }
                            CFRelease(strVal);
                        }
                    }
                }
                CFRelease(rangeVal);
            }
            CFRelease(elem);
        }

        // 2. Clipboard fallback (Cmd+C) for non-AX applications
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        NSInteger oldChangeCount = [pb changeCount];

        NSArray<NSPasteboardItem*>* origItems = [pb pasteboardItems];
        NSMutableArray<NSDictionary<NSPasteboardType, NSData*>*>* savedData = [NSMutableArray array];
        for (NSPasteboardItem* itm in origItems) {
            NSMutableDictionary<NSPasteboardType, NSData*>* dict = [NSMutableDictionary dictionary];
            for (NSPasteboardType t in [itm types]) {
                NSData* d = [itm dataForType:t];
                if (d) dict[t] = d;
            }
            if (dict.count > 0) [savedData addObject:dict];
        }

        CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStatePrivate);
        CGEventRef cDown = CGEventCreateKeyboardEvent(src, (CGKeyCode)kVK_ANSI_C, true);
        CGEventSetFlags(cDown, kCGEventFlagMaskCommand);
        CGEventSetIntegerValueField(cDown, kCGEventSourceUserData, kOurAppMagicTagValue);

        CGEventRef cUp = CGEventCreateKeyboardEvent(src, (CGKeyCode)kVK_ANSI_C, false);
        CGEventSetFlags(cUp, kCGEventFlagMaskCommand);
        CGEventSetIntegerValueField(cUp, kCGEventSourceUserData, kOurAppMagicTagValue);

        if (frontPid > 0) {
            CGEventPostToPid(frontPid, cDown);
            CGEventPostToPid(frontPid, cUp);
        } else {
            CGEventPost(kCGSessionEventTap, cDown);
            CGEventPost(kCGSessionEventTap, cUp);
        }

        CFRelease(cDown);
        CFRelease(cUp);
        CFRelease(src);

        for (int i = 0; i < 24; ++i) {
            usleep(5000);
            if ([pb changeCount] != oldChangeCount) break;
        }

        std::string result = "";
        if ([pb changeCount] != oldChangeCount) {
            NSString* s = [pb stringForType:NSPasteboardTypeString];
            if (s && [s length] > 0) {
                result = [s UTF8String];
            }
            if (savedData.count > 0) {
                [pb clearContents];
                NSMutableArray<NSPasteboardItem*>* restoreList = [NSMutableArray array];
                for (NSDictionary<NSPasteboardType, NSData*>* dict in savedData) {
                    NSPasteboardItem* item = [[NSPasteboardItem alloc] init];
                    for (NSPasteboardType t in dict) {
                        [item setData:dict[t] forType:t];
                    }
                    [restoreList addObject:item];
                }
                [pb writeObjects:restoreList];
            }
        }

        return result;
    }
}

bool replaceSelectedTextInFrontmostApp(const std::string& newText) {
    @autoreleasepool {
        NSRunningApplication* front = [[NSWorkspace sharedWorkspace] frontmostApplication];
        pid_t frontPid = front ? [front processIdentifier] : 0;
        if (frontPid <= 0 || frontPid == getpid()) return false;

        // 1. Try Accessibility API first
        AXUIElementRef appElem = AXUIElementCreateApplication(frontPid);
        if (appElem) {
            AXUIElementSetMessagingTimeout(appElem, 0.1f);
            CFTypeRef focusVal = nullptr;
            if (AXUIElementCopyAttributeValue(appElem, kAXFocusedUIElementAttribute, &focusVal) == kAXErrorSuccess && focusVal) {
                if (CFGetTypeID(focusVal) == AXUIElementGetTypeID()) {
                    AXUIElementRef elem = static_cast<AXUIElementRef>(focusVal);
                    CFStringRef cfNew = CFStringCreateWithCString(kCFAllocatorDefault, newText.c_str(), kCFStringEncodingUTF8);
                    AXError err = AXUIElementSetAttributeValue(elem, kAXSelectedTextAttribute, cfNew);
                    CFRelease(cfNew);
                    CFRelease(focusVal);
                    CFRelease(appElem);
                    if (err == kAXErrorSuccess) {
                        return true;
                    }
                } else {
                    CFRelease(focusVal);
                }
            }
            CFRelease(appElem);
        }

        // 2. Fallback: Put newText on general pasteboard and simulate Cmd+V
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        [pb setString:[NSString stringWithUTF8String:newText.c_str()] forType:NSPasteboardTypeString];

        CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStatePrivate);
        CGEventRef vDown = CGEventCreateKeyboardEvent(src, (CGKeyCode)kVK_ANSI_V, true);
        CGEventSetFlags(vDown, kCGEventFlagMaskCommand);
        CGEventSetIntegerValueField(vDown, kCGEventSourceUserData, kOurAppMagicTagValue);

        CGEventRef vUp = CGEventCreateKeyboardEvent(src, (CGKeyCode)kVK_ANSI_V, false);
        CGEventSetFlags(vUp, kCGEventFlagMaskCommand);
        CGEventSetIntegerValueField(vUp, kCGEventSourceUserData, kOurAppMagicTagValue);

        if (frontPid > 0) {
            CGEventPostToPid(frontPid, vDown);
            CGEventPostToPid(frontPid, vUp);
        } else {
            CGEventPost(kCGSessionEventTap, vDown);
            CGEventPost(kCGSessionEventTap, vUp);
        }

        CFRelease(vDown);
        CFRelease(vUp);
        CFRelease(src);
        return true;
    }
}
