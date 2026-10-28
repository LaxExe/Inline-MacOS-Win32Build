#include "platform/win/hotkey/global_hotkey.h"

#ifdef _WIN32
#include <windows.h>
#endif

GlobalHotkeyManager& GlobalHotkeyManager::instance() {
    static GlobalHotkeyManager mgr;
    return mgr;
}

GlobalHotkeyManager::GlobalHotkeyManager() = default;

GlobalHotkeyManager::~GlobalHotkeyManager() {
    unregisterHotkey();
}

bool GlobalHotkeyManager::registerHotkey(KeyCode key, ModifierFlags mods, std::function<void()> callback) {
    unregisterHotkey();
    callback_ = std::move(callback);
    registered_ = true;

#ifdef _WIN32
    UINT fsModifiers = 0;
    if (mods & ModifierFlags::Cmd)   fsModifiers |= MOD_WIN;
    if (mods & ModifierFlags::Alt)   fsModifiers |= MOD_ALT;
    if (mods & ModifierFlags::Shift) fsModifiers |= MOD_SHIFT;
    if (mods & ModifierFlags::Ctrl)  fsModifiers |= MOD_CONTROL;

    RegisterHotKey(NULL, 1, fsModifiers, static_cast<UINT>(key));
#else
    (void)key;
    (void)mods;
#endif
    return true;
}

void GlobalHotkeyManager::unregisterHotkey() {
    if (!registered_) return;
#ifdef _WIN32
    UnregisterHotKey(NULL, 1);
#endif
    registered_ = false;
    callback_ = nullptr;
}
