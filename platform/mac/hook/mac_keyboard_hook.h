#pragma once

#include "core/interfaces/IKeyboardHook.h"
#include <CoreGraphics/CoreGraphics.h>
#include <thread>
#include <atomic>
#include <deque>
#include <mutex>
#include <condition_variable>

class MacKeyboardHook : public IKeyboardHook {
public:
    MacKeyboardHook();
    ~MacKeyboardHook() override;

    bool start() override;
    void stop() override;
    bool isRunning() const override;
    void setKeyCallback(std::function<void(const RawKeyEvent&)> cb) override;
    void setShouldInterceptTab(std::function<bool()> fn) { shouldInterceptTab_ = std::move(fn); }
    void clearQueue();

    static CGEventRef eventTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* userInfo);

private:
    std::function<void(const RawKeyEvent&)> callback_;
    std::function<bool()> shouldInterceptTab_;
    std::atomic<bool> running_{false};
    std::thread hookThread_;
    std::thread workerThread_;

    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::deque<RawKeyEvent> eventQueue_;

    CFMachPortRef tap_ = nullptr;
    CFRunLoopRef runLoop_ = nullptr;
    CFRunLoopSourceRef runLoopSource_ = nullptr;

    void hookThreadFunc();
    void workerThreadFunc();
};
