#pragma once

#include "core/interfaces/IKeyboardHook.h"
#include <thread>
#include <atomic>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef kCGEventKeyDown
#define kCGEventKeyDown 10
#endif

#ifndef kVK_ANSI_A
enum {
  kVK_ANSI_A                    = 0x00,
  kVK_ANSI_S                    = 0x01,
  kVK_ANSI_D                    = 0x02,
  kVK_ANSI_F                    = 0x03,
  kVK_ANSI_H                    = 0x04,
  kVK_ANSI_G                    = 0x05,
  kVK_ANSI_Z                    = 0x06,
  kVK_ANSI_X                    = 0x07,
  kVK_ANSI_C                    = 0x08,
  kVK_ANSI_V                    = 0x09,
  kVK_ANSI_B                    = 0x0B,
  kVK_ANSI_Q                    = 0x0C,
  kVK_ANSI_W                    = 0x0D,
  kVK_ANSI_E                    = 0x0E,
  kVK_ANSI_R                    = 0x0F,
  kVK_ANSI_Y                    = 0x10,
  kVK_ANSI_T                    = 0x11,
  kVK_ANSI_1                    = 0x12,
  kVK_ANSI_2                    = 0x13,
  kVK_ANSI_3                    = 0x14,
  kVK_ANSI_4                    = 0x15,
  kVK_ANSI_6                    = 0x16,
  kVK_ANSI_5                    = 0x17,
  kVK_ANSI_Equal                = 0x18,
  kVK_ANSI_9                    = 0x19,
  kVK_ANSI_7                    = 0x1A,
  kVK_ANSI_Minus                = 0x1B,
  kVK_ANSI_8                    = 0x1C,
  kVK_ANSI_0                    = 0x1D,
  kVK_ANSI_RightBracket         = 0x1E,
  kVK_ANSI_O                    = 0x1F,
  kVK_ANSI_U                    = 0x20,
  kVK_ANSI_LeftBracket          = 0x21,
  kVK_ANSI_I                    = 0x22,
  kVK_ANSI_P                    = 0x23,
  kVK_ANSI_L                    = 0x25,
  kVK_ANSI_J                    = 0x26,
  kVK_ANSI_Quote                = 0x27,
  kVK_ANSI_K                    = 0x28,
  kVK_ANSI_Semicolon            = 0x29,
  kVK_ANSI_Backslash            = 0x2A,
  kVK_ANSI_Comma                = 0x2B,
  kVK_ANSI_Slash                = 0x2C,
  kVK_ANSI_N                    = 0x2D,
  kVK_ANSI_M                    = 0x2E,
  kVK_ANSI_Period               = 0x2F,
  kVK_ANSI_Grave                = 0x32,
  kVK_Return                    = 0x24,
  kVK_Tab                       = 0x30,
  kVK_Space                     = 0x31,
  kVK_Delete                    = 0x33,
  kVK_Escape                    = 0x35,
  kVK_LeftArrow                 = 0x7B,
  kVK_RightArrow                = 0x7C,
  kVK_DownArrow                 = 0x7D,
  kVK_UpArrow                   = 0x7E,
  kVK_Home                      = 0x73,
  kVK_End                       = 0x77,
  kVK_PageUp                    = 0x74,
  kVK_PageDown                  = 0x79,
  kVK_ForwardDelete             = 0x75
};
#endif

class WinKeyboardHook : public IKeyboardHook {
public:
    WinKeyboardHook();
    ~WinKeyboardHook() override;

    bool start() override;
    void stop() override;
    bool isRunning() const override;
    void setKeyCallback(std::function<void(const RawKeyEvent&)> cb) override;
    void setShouldInterceptTab(std::function<bool()> fn) { shouldInterceptTab_ = std::move(fn); }
    void clearQueue();

#ifdef _WIN32
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
#endif

private:
    std::function<void(const RawKeyEvent&)> callback_;
    std::function<bool()> shouldInterceptTab_;
    std::atomic<bool> running_{false};
    std::thread hookThread_;
    std::thread workerThread_;

    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::deque<RawKeyEvent> eventQueue_;

#ifdef _WIN32
    HHOOK hHook_ = nullptr;
    DWORD threadId_ = 0;
#endif

    void hookThreadFunc();
    void workerThreadFunc();
};
