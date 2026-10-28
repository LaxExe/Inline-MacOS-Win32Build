#include "platform/win/hook/win_keyboard_hook.h"
#include "platform/win/hook/synthetic_filter.h"
#include <chrono>

static WinKeyboardHook* g_hookInstance = nullptr;

static int64_t winVkToMacVk(uint64_t vk) {
    switch (vk) {
        case 'A': return kVK_ANSI_A;
        case 'B': return kVK_ANSI_B;
        case 'C': return kVK_ANSI_C;
        case 'D': return kVK_ANSI_D;
        case 'E': return kVK_ANSI_E;
        case 'F': return kVK_ANSI_F;
        case 'G': return kVK_ANSI_G;
        case 'H': return kVK_ANSI_H;
        case 'I': return kVK_ANSI_I;
        case 'J': return kVK_ANSI_J;
        case 'K': return kVK_ANSI_K;
        case 'L': return kVK_ANSI_L;
        case 'M': return kVK_ANSI_M;
        case 'N': return kVK_ANSI_N;
        case 'O': return kVK_ANSI_O;
        case 'P': return kVK_ANSI_P;
        case 'Q': return kVK_ANSI_Q;
        case 'R': return kVK_ANSI_R;
        case 'S': return kVK_ANSI_S;
        case 'T': return kVK_ANSI_T;
        case 'U': return kVK_ANSI_U;
        case 'V': return kVK_ANSI_V;
        case 'W': return kVK_ANSI_W;
        case 'X': return kVK_ANSI_X;
        case 'Y': return kVK_ANSI_Y;
        case 'Z': return kVK_ANSI_Z;
        case '0': return kVK_ANSI_0;
        case '1': return kVK_ANSI_1;
        case '2': return kVK_ANSI_2;
        case '3': return kVK_ANSI_3;
        case '4': return kVK_ANSI_4;
        case '5': return kVK_ANSI_5;
        case '6': return kVK_ANSI_6;
        case '7': return kVK_ANSI_7;
        case '8': return kVK_ANSI_8;
        case '9': return kVK_ANSI_9;
        case 0x20: return kVK_Space; // VK_SPACE
        case 0x0D: return kVK_Return; // VK_RETURN
        case 0x09: return kVK_Tab; // VK_TAB
        case 0x08: return kVK_Delete; // VK_BACK
        case 0x1B: return kVK_Escape; // VK_ESCAPE
        case 0x25: return kVK_LeftArrow; // VK_LEFT
        case 0x26: return kVK_UpArrow; // VK_UP
        case 0x27: return kVK_RightArrow; // VK_RIGHT
        case 0x28: return kVK_DownArrow; // VK_DOWN
        case 0x24: return kVK_Home; // VK_HOME
        case 0x23: return kVK_End; // VK_END
        case 0x21: return kVK_PageUp; // VK_PRIOR
        case 0x22: return kVK_PageDown; // VK_NEXT
        case 0x2E: return kVK_ForwardDelete; // VK_DELETE
        case 0xDE: return kVK_ANSI_Quote; // VK_OEM_7
        case 0xBE: return kVK_ANSI_Period; // VK_OEM_PERIOD
        case 0xBC: return kVK_ANSI_Comma; // VK_OEM_COMMA
        case 0xBD: return kVK_ANSI_Minus; // VK_OEM_MINUS
        case 0xBB: return kVK_ANSI_Equal; // VK_OEM_PLUS
        case 0xBA: return kVK_ANSI_Semicolon; // VK_OEM_1
        case 0xBF: return kVK_ANSI_Slash; // VK_OEM_2
        case 0xDC: return kVK_ANSI_Backslash; // VK_OEM_5
        case 0xDB: return kVK_ANSI_LeftBracket; // VK_OEM_4
        case 0xDD: return kVK_ANSI_RightBracket; // VK_OEM_6
        case 0xC0: return kVK_ANSI_Grave; // VK_OEM_3
        default: return vk;
    }
}

WinKeyboardHook::WinKeyboardHook() {
    g_hookInstance = this;
}

WinKeyboardHook::~WinKeyboardHook() {
    stop();
    if (g_hookInstance == this) {
        g_hookInstance = nullptr;
    }
}

bool WinKeyboardHook::start() {
    if (running_.load()) return true;
    running_.store(true);
    workerThread_ = std::thread(&WinKeyboardHook::workerThreadFunc, this);
    hookThread_ = std::thread(&WinKeyboardHook::hookThreadFunc, this);
    return true;
}

void WinKeyboardHook::stop() {
    if (!running_.load()) return;
    running_.store(false);
#ifdef _WIN32
    if (threadId_ != 0) {
        PostThreadMessage(threadId_, WM_QUIT, 0, 0);
    }
#endif
    queueCv_.notify_all();
    if (hookThread_.joinable()) hookThread_.join();
    if (workerThread_.joinable()) workerThread_.join();
}

bool WinKeyboardHook::isRunning() const {
    return running_.load();
}

void WinKeyboardHook::setKeyCallback(std::function<void(const RawKeyEvent&)> cb) {
    callback_ = std::move(cb);
}

void WinKeyboardHook::clearQueue() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    eventQueue_.clear();
}

#ifdef _WIN32
LRESULT CALLBACK WinKeyboardHook::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && g_hookInstance && g_hookInstance->running_.load()) {
        KBDLLHOOKSTRUCT* kbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            bool isInjected = (kbd->flags & LLKHF_INJECTED) != 0;
            RawKeyEvent ev;
            ev.keyCode = winVkToMacVk(kbd->vkCode);
            ev.flags = kbd->flags;
            ev.type = kCGEventKeyDown;
            ev.timestampNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();

            if (!isInjected) {
                std::lock_guard<std::mutex> lock(g_hookInstance->queueMutex_);
                g_hookInstance->eventQueue_.push_back(ev);
                g_hookInstance->queueCv_.notify_one();
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
#endif

void WinKeyboardHook::hookThreadFunc() {
#ifdef _WIN32
    threadId_ = GetCurrentThreadId();
    hHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    MSG msg;
    while (running_.load() && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (hHook_) {
        UnhookWindowsHookEx(hHook_);
        hHook_ = nullptr;
    }
#endif
}

void WinKeyboardHook::workerThreadFunc() {
    while (running_.load()) {
        RawKeyEvent ev;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this] {
                return !eventQueue_.empty() || !running_.load();
            });
            if (!running_.load() && eventQueue_.empty()) break;
            ev = eventQueue_.front();
            eventQueue_.pop_front();
        }
        if (callback_) {
            callback_(ev);
        }
    }
}
