#pragma once

#include "core/config/keycode.h"
#include <functional>

class GlobalHotkeyManager {
public:
    static GlobalHotkeyManager& instance();

    bool registerHotkey(KeyCode key, ModifierFlags mods, std::function<void()> callback);
    void unregisterHotkey();

private:
    GlobalHotkeyManager();
    ~GlobalHotkeyManager();

    std::function<void()> callback_;
    void* hotKeyRef_ = nullptr;
    void* eventHandlerRef_ = nullptr;

    static void* getCarbonEventHandler();
};
