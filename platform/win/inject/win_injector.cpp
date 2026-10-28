#include "platform/win/inject/win_injector.h"
#include "platform/win/hook/synthetic_filter.h"
#include "core/injection/token_validation.h"
#include "core/injection/priority_queue.h"
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

WinInjector::WinInjector() = default;

WinInjector::~WinInjector() {
    stop();
}

void WinInjector::start() {
    if (running_.load()) return;
    running_.store(true);
    injectorThread_ = std::thread(&WinInjector::run, this);
}

void WinInjector::stop() {
    if (!running_.load()) return;
    running_.store(false);
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queue_.clear();
    }
    cv_.notify_all();
    if (injectorThread_.joinable()) {
        injectorThread_.join();
    }
}

void WinInjector::enqueue(const InjectionOp& op) {
    if (!running_.load()) return;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (!running_.load()) return;
        enqueueOp(queue_, op);
    }
    cv_.notify_one();
}

void WinInjector::setValidToken(InjectionOp::Source source, int token) {
    std::lock_guard<std::mutex> lock(tokenMutex_);
    validTokens_[static_cast<int>(source)] = token;
}

int WinInjector::getValidToken(InjectionOp::Source source) {
    std::lock_guard<std::mutex> lock(tokenMutex_);
    auto it = validTokens_.find(static_cast<int>(source));
    if (it != validTokens_.end()) return it->second;
    return 0;
}

void WinInjector::clearQueue() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    queue_.clear();
}

void WinInjector::postTaggedKeyEvent(uint16_t vkCode, bool keyDown) {
#ifdef _WIN32
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vkCode;
    input.ki.dwFlags = keyDown ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
#else
    (void)vkCode;
    (void)keyDown;
#endif
}

void WinInjector::postTaggedUnicodeEvent(wchar_t ch) {
#ifdef _WIN32
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wScan = static_cast<WORD>(ch);
    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wScan = static_cast<WORD>(ch);
    inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

    SendInput(2, inputs, sizeof(INPUT));
#else
    (void)ch;
#endif
}

void WinInjector::run() {
    while (running_.load()) {
        InjectionOp op;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            cv_.wait(lock, [this] {
                return !queue_.empty() || !running_.load();
            });
            if (!running_.load() && queue_.empty()) break;
            op = queue_.front();
            queue_.pop_front();
        }

        int currentToken = getValidToken(op.source);
        if (!shouldApplyOp(op, currentToken)) {
            continue;
        }

        switch (op.type) {
            case InjectionOp::BACKSPACE_N:
                for (int i = 0; i < op.count; ++i) {
#ifdef _WIN32
                    postTaggedKeyEvent(VK_BACK, true);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    postTaggedKeyEvent(VK_BACK, false);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif
                }
                break;
            case InjectionOp::TYPE_TEXT:
                for (wchar_t ch : op.text) {
                    postTaggedUnicodeEvent(ch);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                break;
            case InjectionOp::REPLACE_TEXT:
                for (int i = 0; i < op.count; ++i) {
#ifdef _WIN32
                    postTaggedKeyEvent(VK_BACK, true);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    postTaggedKeyEvent(VK_BACK, false);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                for (wchar_t ch : op.text) {
                    postTaggedUnicodeEvent(ch);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                break;
            case InjectionOp::MOVE_CARET:
                break;
        }
    }
}
