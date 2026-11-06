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
    void executeImmediate(const InjectionOp& op);
    void setValidToken(InjectionOp::Source source, int token) override;
    void clearQueue();

    void postTaggedKeyEvent(CGKeyCode key, bool keyDown);
    void postTaggedUnicodeEvent(wchar_t ch);
    void postTaggedUnicodeString(const std::wstring& text);
    void postBackspaces(int count);

private:
    std::atomic<bool> running_{false};
    std::thread injectorThread_;

    std::mutex queueMutex_;
    std::condition_variable cv_;
    std::deque<InjectionOp> queue_;

    std::atomic<int> validTokens_[4]{{0}, {0}, {0}, {0}};

    int getValidToken(InjectionOp::Source source) const;
    void run();

    CGEventSourceRef eventSource_ = nullptr;
    CGEventRef backspaceDownEvent_ = nullptr;
    CGEventRef backspaceUpEvent_ = nullptr;
    CGEventRef unicodeDownEvent_ = nullptr;
    CGEventRef unicodeUpEvent_ = nullptr;
};
