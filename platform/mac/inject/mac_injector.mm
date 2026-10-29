#include "platform/mac/inject/mac_injector.h"
#include "platform/mac/hook/synthetic_filter.h"
#include "core/injection/token_validation.h"
#include "core/injection/priority_queue.h"
#include <chrono>
#include <algorithm>
#include <string>
#include <vector>
#include <atomic>
#include <pthread/qos.h>
#import <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

static std::atomic<pid_t> gFrontmostPid{0};

@interface FrontAppWatcher : NSObject
@end

@implementation FrontAppWatcher
- (instancetype)init {
    self = [super init];
    if (self) {
        NSRunningApplication* front = [[NSWorkspace sharedWorkspace] frontmostApplication];
        if (front) {
            gFrontmostPid.store([front processIdentifier]);
        }
        [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                                                               selector:@selector(appActivated:)
                                                                   name:NSWorkspaceDidActivateApplicationNotification
                                                                 object:nil];
    }
    return self;
}
- (void)appActivated:(NSNotification*)note {
    NSRunningApplication* app = note.userInfo[NSWorkspaceApplicationKey];
    if (app) {
        gFrontmostPid.store([app processIdentifier]);
    }
}
@end

static FrontAppWatcher* gWatcher = nil;

static void ensureFrontAppWatcher() {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        gWatcher = [[FrontAppWatcher alloc] init];
    });
}

static void postEvent(CGEventRef e) {
    pid_t targetPid = gFrontmostPid.load();
    if (targetPid <= 0) {
        NSRunningApplication* front = [[NSWorkspace sharedWorkspace] frontmostApplication];
        if (front) {
            targetPid = [front processIdentifier];
            gFrontmostPid.store(targetPid);
        }
    }
    if (targetPid > 0 && targetPid != getpid()) {
        CGEventPostToPid(targetPid, e);
    } else {
        CGEventPost(kCGSessionEventTap, e);
    }
}

MacInjector::MacInjector() {
    ensureFrontAppWatcher();
    eventSource_ = CGEventSourceCreate(kCGEventSourceStatePrivate);

    // Pre-allocate and warm up reusable CGEventRef handles at startup to eliminate
    // CoreGraphics/HIToolbox ~64ms lazy initialization on first fix and avoid per-event allocations.
    backspaceDownEvent_ = CGEventCreateKeyboardEvent(eventSource_, kVK_Delete, true);
    if (backspaceDownEvent_) {
        CGEventSetFlags(backspaceDownEvent_, 0);
        CGEventSetIntegerValueField(backspaceDownEvent_, kCGEventSourceUserData, kOurAppMagicTagValue);
    }
    backspaceUpEvent_ = CGEventCreateKeyboardEvent(eventSource_, kVK_Delete, false);
    if (backspaceUpEvent_) {
        CGEventSetFlags(backspaceUpEvent_, 0);
        CGEventSetIntegerValueField(backspaceUpEvent_, kCGEventSourceUserData, kOurAppMagicTagValue);
    }
    unicodeDownEvent_ = CGEventCreateKeyboardEvent(eventSource_, 0, true);
    if (unicodeDownEvent_) {
        CGEventSetFlags(unicodeDownEvent_, 0);
        CGEventSetIntegerValueField(unicodeDownEvent_, kCGEventSourceUserData, kOurAppMagicTagValue);
    }
    unicodeUpEvent_ = CGEventCreateKeyboardEvent(eventSource_, 0, false);
    if (unicodeUpEvent_) {
        CGEventSetFlags(unicodeUpEvent_, 0);
        CGEventSetIntegerValueField(unicodeUpEvent_, kCGEventSourceUserData, kOurAppMagicTagValue);
    }
}

MacInjector::~MacInjector() {
    stop();
    if (backspaceDownEvent_) { CFRelease(backspaceDownEvent_); backspaceDownEvent_ = nullptr; }
    if (backspaceUpEvent_) { CFRelease(backspaceUpEvent_); backspaceUpEvent_ = nullptr; }
    if (unicodeDownEvent_) { CFRelease(unicodeDownEvent_); unicodeDownEvent_ = nullptr; }
    if (unicodeUpEvent_) { CFRelease(unicodeUpEvent_); unicodeUpEvent_ = nullptr; }
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

void MacInjector::clearQueue() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    queue_.clear();
}

void MacInjector::setValidToken(InjectionOp::Source source, int token) {
    const int idx = static_cast<int>(source);
    if (idx >= 0 && idx < 4) {
        validTokens_[idx].store(token, std::memory_order_release);
    }
}

int MacInjector::getValidToken(InjectionOp::Source source) const {
    const int idx = static_cast<int>(source);
    if (idx >= 0 && idx < 4) {
        return validTokens_[idx].load(std::memory_order_acquire);
    }
    return 0;
}

void MacInjector::executeImmediate(const InjectionOp& op) {
    switch (op.type) {
        case InjectionOp::BACKSPACE_N:
            postBackspaces(op.count);
            break;
        case InjectionOp::TYPE_TEXT:
            postTaggedUnicodeString(op.text);
            break;
        case InjectionOp::REPLACE_TEXT:
            postBackspaces(op.count);
            postTaggedUnicodeString(op.text);
            break;
        case InjectionOp::MOVE_CARET:
            break;
    }
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
    postEvent(e);
    CFRelease(e);
}

void MacInjector::postTaggedUnicodeEvent(wchar_t ch) {
    UniChar u = static_cast<UniChar>(ch);

    CGEventRef eDown = CGEventCreateKeyboardEvent(eventSource_, 0, true);
    if (eDown) {
        CGEventSetFlags(eDown, 0);
        CGEventKeyboardSetUnicodeString(eDown, 1, &u);
        CGEventSetIntegerValueField(eDown, kCGEventSourceUserData, kOurAppMagicTagValue);
        postEvent(eDown);
        CFRelease(eDown);
    }

    CGEventRef eUp = CGEventCreateKeyboardEvent(eventSource_, 0, false);
    if (eUp) {
        CGEventSetFlags(eUp, 0);
        CGEventKeyboardSetUnicodeString(eUp, 1, &u);
        CGEventSetIntegerValueField(eUp, kCGEventSourceUserData, kOurAppMagicTagValue);
        postEvent(eUp);
        CFRelease(eUp);
    }
}

// Post one key-down/key-up pair that types up to kMaxUnicodeChunk UTF-16 units at once.
// CGEventKeyboardSetUnicodeString accepts a multi-character payload, so a whole corrected word
// goes out as a single event pair instead of 2 events (and a sleep) per character.
static constexpr size_t kMaxUnicodeChunk = 16;

static void toUtf16(const std::wstring& in, std::vector<UniChar>& out) {
    out.clear();
    out.reserve(in.size());
    for (wchar_t wc : in) {
        uint32_t cp = static_cast<uint32_t>(wc);
        if (cp > 0xFFFF) {
            cp -= 0x10000;
            out.push_back(static_cast<UniChar>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<UniChar>(0xDC00 + (cp & 0x3FF)));
        } else {
            out.push_back(static_cast<UniChar>(cp));
        }
    }
}

void MacInjector::postTaggedUnicodeString(const std::wstring& text) {
    if (text.empty()) return;

    // Fast stack buffer for words <= 64 UTF-16 units (zero heap allocations)
    UniChar stackUnits[64];
    size_t totalUnits = 0;
    std::vector<UniChar> heapUnits;
    UniChar* buf = stackUnits;

    if (text.size() * 2 <= 64) {
        for (wchar_t wc : text) {
            uint32_t cp = static_cast<uint32_t>(wc);
            if (cp > 0xFFFF) {
                cp -= 0x10000;
                stackUnits[totalUnits++] = static_cast<UniChar>(0xD800 + (cp >> 10));
                stackUnits[totalUnits++] = static_cast<UniChar>(0xDC00 + (cp & 0x3FF));
            } else {
                stackUnits[totalUnits++] = static_cast<UniChar>(cp);
            }
        }
    } else {
        toUtf16(text, heapUnits);
        buf = heapUnits.data();
        totalUnits = heapUnits.size();
    }

    for (size_t off = 0; off < totalUnits; off += kMaxUnicodeChunk) {
        size_t n = std::min(kMaxUnicodeChunk, totalUnits - off);
        // Never split a surrogate pair across two events.
        if (n > 0 && off + n < totalUnits && (buf[off + n - 1] & 0xFC00) == 0xD800) {
            --n;
        }
        if (unicodeDownEvent_ && unicodeUpEvent_) {
            CGEventSetFlags(unicodeDownEvent_, 0);
            CGEventKeyboardSetUnicodeString(unicodeDownEvent_, n, &buf[off]);
            postEvent(unicodeDownEvent_);

            CGEventSetFlags(unicodeUpEvent_, 0);
            CGEventKeyboardSetUnicodeString(unicodeUpEvent_, n, &buf[off]);
            postEvent(unicodeUpEvent_);
        } else {
            for (int down = 1; down >= 0; --down) {
                CGEventRef e = CGEventCreateKeyboardEvent(eventSource_, 0, down != 0);
                if (!e) continue;
                CGEventSetFlags(e, 0);
                CGEventKeyboardSetUnicodeString(e, n, &buf[off]);
                CGEventSetIntegerValueField(e, kCGEventSourceUserData, kOurAppMagicTagValue);
                postEvent(e);
                CFRelease(e);
            }
        }
    }
}

void MacInjector::postBackspaces(int count) {
    if (backspaceDownEvent_ && backspaceUpEvent_) {
        for (int i = 0; i < count; ++i) {
            CGEventSetFlags(backspaceDownEvent_, 0);
            postEvent(backspaceDownEvent_);
            CGEventSetFlags(backspaceUpEvent_, 0);
            postEvent(backspaceUpEvent_);
        }
    } else {
        for (int i = 0; i < count; ++i) {
            postTaggedKeyEvent(kVK_Delete, true);
            postTaggedKeyEvent(kVK_Delete, false);
        }
    }
}

void MacInjector::run() {
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
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

        // No inter-event sleeps: each CGEventPost already takes tens of microseconds and the
        // session event queue preserves ordering, so a full replacement completes in ~1-2 ms
        // instead of ~30 ms. The shorter the window, the less chance the user's next keystrokes
        // interleave with the replacement.
        switch (op.type) {
            case InjectionOp::BACKSPACE_N:
                postBackspaces(op.count);
                break;
            case InjectionOp::TYPE_TEXT:
                postTaggedUnicodeString(op.text);
                break;
            case InjectionOp::REPLACE_TEXT:
                postBackspaces(op.count);
                postTaggedUnicodeString(op.text);
                break;
            case InjectionOp::MOVE_CARET:
                break;
        }
    }
}
