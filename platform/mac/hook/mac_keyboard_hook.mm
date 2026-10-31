#include "platform/mac/hook/mac_keyboard_hook.h"
#include "platform/mac/hook/synthetic_filter.h"
#include "platform/mac/hook/tap_recovery.h"
#include "platform/mac/permissions.h"
#include "core/reliability/privacy_logger.h"
#include <iostream>
#include <chrono>
#include <pthread/qos.h>

MacKeyboardHook::MacKeyboardHook() = default;

MacKeyboardHook::~MacKeyboardHook() {
    stop();
}

bool MacKeyboardHook::start() {
    if (running_) return true;
    running_ = true;
    hookThread_ = std::thread(&MacKeyboardHook::hookThreadFunc, this);
    return true;
}

void MacKeyboardHook::stop() {
    if (!running_) return;
    running_ = false;
    setLiveEventTapAttached(false);

    // 1. Immediately disable tap so WindowServer stops routing events
    if (tap_) {
        CGEventTapEnable(tap_, false);
    }

    // 2. Detach run loop source and stop runloop
    if (runLoop_) {
        if (runLoopSource_) {
            CFRunLoopRemoveSource(runLoop_, runLoopSource_, kCFRunLoopCommonModes);
        }
        CFRunLoopStop(runLoop_);
    }

    // 3. Wait for hook thread to join
    if (hookThread_.joinable()) {
        hookThread_.join();
    }

    // 4. Release CoreGraphics handles
    if (runLoopSource_) {
        CFRelease(runLoopSource_);
        runLoopSource_ = nullptr;
    }
    if (tap_) {
        CFRelease(tap_);
        tap_ = nullptr;
    }
    runLoop_ = nullptr;
}

bool MacKeyboardHook::isRunning() const {
    return running_;
}

void MacKeyboardHook::setKeyCallback(std::function<void(const RawKeyEvent&)> cb) {
    callback_ = std::move(cb);
}

void MacKeyboardHook::clearQueue() {
    // Queue eliminated: events are processed directly on the dedicated real-time hook thread
}

CGEventRef MacKeyboardHook::eventTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* userInfo) {
    (void)proxy;
    auto* self = static_cast<MacKeyboardHook*>(userInfo);
    if (!self) return event;

    // 1. Handle tap disable immediately and re-enable (<0.001ms)
    if (shouldReenableTap(type)) {
        if (self->tap_) {
            CGEventTapEnable(self->tap_, true);
        }
        return event;
    }

    // 2. Filter out our own synthetic events FIRST (<0.001ms)
    int64_t tag = CGEventGetIntegerValueField(event, kCGEventSourceUserData);
    if (isSyntheticEvent(tag)) {
        return event;
    }

    // 3. Zero-allocation, lock-free direct execution on QOS_CLASS_USER_INTERACTIVE hook thread
    RawKeyEvent raw;
    raw.keyCode = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
    raw.flags = CGEventGetFlags(event);
    raw.type = static_cast<uint32_t>(type);
    raw.timestampNs = CGEventGetTimestamp(event);
    raw.isAutorepeat = (CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0);

    const bool activeTap = self->isActiveTap_.load(std::memory_order_relaxed);
    bool swallow = (type == kCGEventKeyDown && raw.keyCode == 48 /* kVK_Tab */ &&
                    self->shouldInterceptTab_ && self->shouldInterceptTab_());

    if (self->filterCallback_) {
        if (self->filterCallback_(raw, activeTap)) {
            swallow = true;
        }
    } else if (self->callback_) {
        self->callback_(raw);
    }

    if (swallow && activeTap) {
        return nullptr; // Swallow event before host app sees it
    }

    return event;
}

void MacKeyboardHook::hookThreadFunc() {
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);

    // Subscribe ONLY to KeyDown so mouse clicks across macOS are never routed through an active tap
    CGEventMask mask = CGEventMaskBit(kCGEventKeyDown);

    while (running_ && !tap_) {
        // Step 1: Prefer active tap at tail of session chain so typo delimiters can be swallowed
        // before the target app renders the wrong word + space.
        tap_ = CGEventTapCreate(
            kCGSessionEventTap,
            kCGTailAppendEventTap,
            kCGEventTapOptionDefault,
            mask,
            MacKeyboardHook::eventTapCallback,
            this
        );
        if (tap_) {
            isActiveTap_.store(true, std::memory_order_relaxed);
            break;
        }

        // Fallback to listen-only if Accessibility is not yet granted
        tap_ = CGEventTapCreate(
            kCGSessionEventTap,
            kCGHeadInsertEventTap,
            kCGEventTapOptionListenOnly,
            mask,
            MacKeyboardHook::eventTapCallback,
            this
        );
        if (!tap_) {
            tap_ = CGEventTapCreate(
                kCGAnnotatedSessionEventTap,
                kCGHeadInsertEventTap,
                kCGEventTapOptionListenOnly,
                mask,
                MacKeyboardHook::eventTapCallback,
                this
            );
        }
        if (!tap_) {
            tap_ = CGEventTapCreate(
                kCGHIDEventTap,
                kCGHeadInsertEventTap,
                kCGEventTapOptionListenOnly,
                mask,
                MacKeyboardHook::eventTapCallback,
                this
            );
        }
        if (tap_) {
            isActiveTap_.store(false, std::memory_order_relaxed);
            break;
        }
        for (int i = 0; i < 10 && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    if (!running_ || !tap_) {
        return;
    }

    setLiveEventTapAttached(true);
    runLoop_ = CFRunLoopGetCurrent();
    runLoopSource_ = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap_, 0);
    CFRunLoopAddSource(runLoop_, runLoopSource_, kCFRunLoopCommonModes);
    CGEventTapEnable(tap_, true);

    PrivacyLogger::instance().logEvent("CGEventTapAttached", 0, 0);

    CFRunLoopRun();
    setLiveEventTapAttached(false);
}
