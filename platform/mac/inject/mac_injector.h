#pragma once

#include "core/interfaces/IInjector.h"
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <CoreGraphics/CoreGraphics.h>

class MacInjector : public IInjector {
public:
    MacInjector();
    ~MacInjector() override;

    void start() override;
    void stop() override;
    void enqueue(const InjectionOp& op) override;
    void setValidToken(InjectionOp::Source source, int token) override;

    void postTaggedKeyEvent(CGKeyCode key, bool keyDown);
    void postTaggedUnicodeEvent(wchar_t ch);

private:
    std::atomic<bool> running_{false};
    std::thread injectorThread_;

    std::mutex queueMutex_;
    std::condition_variable cv_;
    std::deque<InjectionOp> queue_;

    std::mutex tokenMutex_;
    std::unordered_map<int, int> validTokens_;

    int paceMs_ = 6;

    int getValidToken(InjectionOp::Source source);
    void run();

    CGEventSourceRef eventSource_ = nullptr;
};
