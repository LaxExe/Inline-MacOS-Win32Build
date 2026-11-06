#pragma once

#include "core/interfaces/IKeyboardHook.h"
#include <CoreGraphics/CoreGraphics.h>
#include <thread>
#include <atomic>
#include <functional>

class MacKeyboardHook : public IKeyboardHook {
public:
    using KeyFilterCallback = std::function<bool(const RawKeyEvent&, bool isActiveTap)>;

    MacKeyboardHook();
    ~MacKeyboardHook() override;

    bool start() override;
    void stop() override;
    bool isRunning() const override;
    void setKeyCallback(std::function<void(const RawKeyEvent&)> cb) override;
    void setKeyFilterCallback(KeyFilterCallback cb) { filterCallback_ = std::move(cb); }
    void setShouldInterceptTab(std::function<bool()> fn) { shouldInterceptTab_ = std::move(fn); }
    void clearQueue();
    bool isActiveTap() const { return isActiveTap_.load(std::memory_order_relaxed); }

    static CGEventRef eventTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* userInfo);

private:
    std::function<void(const RawKeyEvent&)> callback_;
    KeyFilterCallback filterCallback_;
    std::function<bool()> shouldInterceptTab_;
    std::atomic<bool> running_{false};
    std::atomic<bool> isActiveTap_{false};
    std::thread hookThread_;

    CFMachPortRef tap_ = nullptr;
    CFRunLoopRef runLoop_ = nullptr;
    CFRunLoopSourceRef runLoopSource_ = nullptr;

    void hookThreadFunc();
};
