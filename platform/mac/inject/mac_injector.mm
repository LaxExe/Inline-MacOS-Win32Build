#include "platform/mac/inject/mac_injector.h"
#include "platform/mac/hook/synthetic_filter.h"
#include "core/injection/token_validation.h"
#include "core/injection/priority_queue.h"
#include <chrono>
#include <Carbon/Carbon.h>

MacInjector::MacInjector() {
    eventSource_ = CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
}

MacInjector::~MacInjector() {
    stop();
    if (eventSource_) {
        CFRelease(eventSource_);
        eventSource_ = nullptr;
    }
}

void MacInjector::start() {
    if (running_) return;
    running_ = true;
    injectorThread_ = std::thread(&MacInjector::run, this);
}

void MacInjector::stop() {
    if (!running_) return;
    running_ = false;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queue_.clear();
    }
    cv_.notify_all();
    if (injectorThread_.joinable()) {
        injectorThread_.join();
    }
}

void MacInjector::enqueue(const InjectionOp& op) {
    if (!running_) return;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (!running_) return;
        enqueueOp(queue_, op);
    }
    cv_.notify_one();
}

void MacInjector::setValidToken(InjectionOp::Source source, int token) {
    std::lock_guard<std::mutex> lock(tokenMutex_);
    validTokens_[static_cast<int>(source)] = token;
}

int MacInjector::getValidToken(InjectionOp::Source source) {
    std::lock_guard<std::mutex> lock(tokenMutex_);
    auto it = validTokens_.find(static_cast<int>(source));
    if (it != validTokens_.end()) return it->second;
    return 0;
}

static CGKeyCode charToKeyCode(wchar_t ch) {
    switch (ch) {
        case L'a': case L'A': return kVK_ANSI_A;
        case L'b': case L'B': return kVK_ANSI_B;
        case L'c': case L'C': return kVK_ANSI_C;
        case L'd': case L'D': return kVK_ANSI_D;
        case L'e': case L'E': return kVK_ANSI_E;
        case L'f': case L'F': return kVK_ANSI_F;
        case L'g': case L'G': return kVK_ANSI_G;
        case L'h': case L'H': return kVK_ANSI_H;
        case L'i': case L'I': return kVK_ANSI_I;
        case L'j': case L'J': return kVK_ANSI_J;
        case L'k': case L'K': return kVK_ANSI_K;
        case L'l': case L'L': return kVK_ANSI_L;
        case L'm': case L'M': return kVK_ANSI_M;
        case L'n': case L'N': return kVK_ANSI_N;
        case L'o': case L'O': return kVK_ANSI_O;
        case L'p': case L'P': return kVK_ANSI_P;
        case L'q': case L'Q': return kVK_ANSI_Q;
        case L'r': case L'R': return kVK_ANSI_R;
        case L's': case L'S': return kVK_ANSI_S;
        case L't': case L'T': return kVK_ANSI_T;
        case L'u': case L'U': return kVK_ANSI_U;
        case L'v': case L'V': return kVK_ANSI_V;
        case L'w': case L'W': return kVK_ANSI_W;
        case L'x': case L'X': return kVK_ANSI_X;
        case L'y': case L'Y': return kVK_ANSI_Y;
        case L'z': case L'Z': return kVK_ANSI_Z;
        case L'0': case L')': return kVK_ANSI_0;
        case L'1': case L'!': return kVK_ANSI_1;
        case L'2': case L'@': return kVK_ANSI_2;
        case L'3': case L'#': return kVK_ANSI_3;
        case L'4': case L'$': return kVK_ANSI_4;
        case L'5': case L'%': return kVK_ANSI_5;
        case L'6': case L'^': return kVK_ANSI_6;
        case L'7': case L'&': return kVK_ANSI_7;
        case L'8': case L'*': return kVK_ANSI_8;
        case L'9': case L'(': return kVK_ANSI_9;
        case L' ': return kVK_Space;
        case L'\n': return kVK_Return;
        case L'\t': return kVK_Tab;
        case L'\'': case L'"': return kVK_ANSI_Quote;
        case L'.': case L'>': return kVK_ANSI_Period;
        case L',': case L'<': return kVK_ANSI_Comma;
        case L'-': case L'_': return kVK_ANSI_Minus;
        case L'=': case L'+': return kVK_ANSI_Equal;
        case L';': case L':': return kVK_ANSI_Semicolon;
        case L'/': case L'?': return kVK_ANSI_Slash;
        case L'\\': case L'|': return kVK_ANSI_Backslash;
        case L'[': case L'{': return kVK_ANSI_LeftBracket;
        case L']': case L'}': return kVK_ANSI_RightBracket;
        case L'`': case L'~': return kVK_ANSI_Grave;
        default: return (CGKeyCode)0;
    }
}

void MacInjector::postTaggedKeyEvent(CGKeyCode key, bool keyDown) {
    CGEventRef e = CGEventCreateKeyboardEvent(eventSource_, key, keyDown);
    if (!e) return;
    CGEventSetFlags(e, 0);
    CGEventSetIntegerValueField(e, kCGEventSourceUserData, kOurAppMagicTagValue);
    CGEventPost(kCGSessionEventTap, e);
    CFRelease(e);
}

void MacInjector::postTaggedUnicodeEvent(wchar_t ch) {
    UniChar u = static_cast<UniChar>(ch);

    CGEventRef eDown = CGEventCreateKeyboardEvent(eventSource_, 0, true);
    if (eDown) {
        CGEventSetFlags(eDown, 0);
        CGEventKeyboardSetUnicodeString(eDown, 1, &u);
        CGEventSetIntegerValueField(eDown, kCGEventSourceUserData, kOurAppMagicTagValue);
        CGEventPost(kCGSessionEventTap, eDown);
        CFRelease(eDown);
    }

    CGEventRef eUp = CGEventCreateKeyboardEvent(eventSource_, 0, false);
    if (eUp) {
        CGEventSetFlags(eUp, 0);
        CGEventKeyboardSetUnicodeString(eUp, 1, &u);
        CGEventSetIntegerValueField(eUp, kCGEventSourceUserData, kOurAppMagicTagValue);
        CGEventPost(kCGSessionEventTap, eUp);
        CFRelease(eUp);
    }
}

void MacInjector::run() {
    while (running_) {
        InjectionOp op;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            cv_.wait(lock, [this]() {
                return !running_ || !queue_.empty();
            });
            if (!running_) break;
            op = queue_.front();
            queue_.pop_front();
        }

        int currentToken = getValidToken(op.source);
        if (!shouldApplyOp(op, currentToken)) {
            continue; // stale op, drop silently
        }

        switch (op.type) {
            case InjectionOp::BACKSPACE_N:
                for (int i = 0; i < op.count; ++i) {
                    postTaggedKeyEvent(kVK_Delete, true);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    postTaggedKeyEvent(kVK_Delete, false);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
                    postTaggedKeyEvent(kVK_Delete, true);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    postTaggedKeyEvent(kVK_Delete, false);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
