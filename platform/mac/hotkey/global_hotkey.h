#pragma once

#include "core/config/keycode.h"
#include <functional>
#include <unordered_map>
#include <cstdint>

class GlobalHotkeyManager {
public:
    static GlobalHotkeyManager& instance();

    uint32_t registerHotkey(KeyCode key, ModifierFlags mods, std::function<void()> callback);
    void unregisterHotkey(uint32_t id);
    void unregisterHotkey();
    void unregisterAll();

    void dispatchHotkey(uint32_t id);

private:
    GlobalHotkeyManager();
    ~GlobalHotkeyManager();

    struct HotkeyItem {
        uint32_t id = 0;
        KeyCode key = KeyCode::None;
        ModifierFlags mods = 0;
        void* hotKeyRef = nullptr;
        std::function<void()> callback;
    };

    std::unordered_map<uint32_t, HotkeyItem> hotkeys_;
    uint32_t nextId_ = 1;
    void* eventHandlerRef_ = nullptr;

    void installEventHandlerIfNeeded();
};
