#include "platform/mac/hook/mac_keyboard_hook.h"
#include "platform/mac/hook/synthetic_filter.h"
#include "platform/mac/hook/tap_recovery.h"
#include "core/reliability/privacy_logger.h"
#include <iostream>
#include <chrono>

MacKeyboardHook::MacKeyboardHook() = default;

MacKeyboardHook::~MacKeyboardHook() {
    stop();
}

bool MacKeyboardHook::start() {
    if (running_) return true;
    running_ = true;
    workerThread_ = std::thread(&MacKeyboardHook::workerThreadFunc, this);
    hookThread_ = std::thread(&MacKeyboardHook::hookThreadFunc, this);
    return true;
}

void MacKeyboardHook::stop() {
    if (!running_) return;
    running_ = false;

    // 1. Immediately disable tap so WindowServer stops routing events
    if (tap_) {
        CGEventTapEnable(tap_, false);
    }

    // 2. Clear pending events and wake worker
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        eventQueue_.clear();
    }
    queueCv_.notify_all();

    // 3. Detach run loop source and stop runloop
    if (runLoop_) {
        if (runLoopSource_) {
            CFRunLoopRemoveSource(runLoop_, runLoopSource_, kCFRunLoopCommonModes);
        }
        CFRunLoopStop(runLoop_);
    }

    // 4. Wait for threads to join
    if (hookThread_.joinable()) {
        hookThread_.join();
    }
    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    // 5. Release CoreGraphics handles
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

CGEventRef MacKeyboardHook::eventTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* userInfo) {
    auto* self = static_cast<MacKeyboardHook*>(userInfo);
    if (!self) return event;

    // 1. Handle tap disable immediately and re-enable
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

    // 3. Tag, timestamp, and push to worker queue (<0.005ms)
    RawKeyEvent raw;
    raw.keyCode = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
    raw.flags = CGEventGetFlags(event);
    raw.type = static_cast<uint32_t>(type);
    raw.timestampNs = CGEventGetTimestamp(event);

    bool interceptTab = (type == kCGEventKeyDown && raw.keyCode == 48 /* kVK_Tab */ &&
                         self->shouldInterceptTab_ && self->shouldInterceptTab_());

    {
        std::lock_guard<std::mutex> lock(self->queueMutex_);
        self->eventQueue_.push_back(raw);
    }
    self->queueCv_.notify_one();

    if (interceptTab) {
        return nullptr; // Consume Tab key event so host app does not insert tab or switch focus
    }

    return event;
}

void MacKeyboardHook::workerThreadFunc() {
    while (running_) {
        RawKeyEvent raw;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this]() {
                return !running_ || !eventQueue_.empty();
            });
            if (!running_) break;
            raw = eventQueue_.front();
            eventQueue_.pop_front();
        }

        if (callback_) {
            callback_(raw);
        }
    }
}

void MacKeyboardHook::hookThreadFunc() {
    CGEventMask mask = CGEventMaskBit(kCGEventKeyDown)
                     | CGEventMaskBit(kCGEventKeyUp)
                     | CGEventMaskBit(kCGEventFlagsChanged);

    while (running_ && !tap_) {
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
        if (!tap_) {
            tap_ = CGEventTapCreate(
                kCGSessionEventTap,
                kCGHeadInsertEventTap,
                kCGEventTapOptionDefault,
                mask,
                MacKeyboardHook::eventTapCallback,
                this
            );
        }
        if (!tap_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    if (!running_ || !tap_) {
        return;
    }

    runLoop_ = CFRunLoopGetCurrent();
    runLoopSource_ = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap_, 0);
    CFRunLoopAddSource(runLoop_, runLoopSource_, kCFRunLoopCommonModes);
    CGEventTapEnable(tap_, true);

    PrivacyLogger::instance().logEvent("CGEventTapAttached", 0, 0);

    CFRunLoopRun();
}
