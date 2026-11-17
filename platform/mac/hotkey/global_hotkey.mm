#include "platform/mac/hotkey/global_hotkey.h"
#import <Carbon/Carbon.h>

static EventHandlerUPP gHotKeyFunction = nullptr;

static OSStatus hotKeyHandler(EventHandlerCallRef nextHandler, EventRef theEvent, void *userData) {
    EventHotKeyID hkId;
    OSStatus err = GetEventParameter(theEvent, kEventParamDirectObject, typeEventHotKeyID, NULL, sizeof(hkId), NULL, &hkId);
    if (err == noErr && hkId.signature == 'TKHT') {
        GlobalHotkeyManager::instance().dispatchHotkey(hkId.id);
    }
    return noErr;
}

GlobalHotkeyManager& GlobalHotkeyManager::instance() {
    static GlobalHotkeyManager inst;
    return inst;
}

GlobalHotkeyManager::GlobalHotkeyManager() = default;

GlobalHotkeyManager::~GlobalHotkeyManager() {
    unregisterAll();
}

static UInt32 keycodeToMacVirtualKey(KeyCode key) {
    switch (key) {
        case KeyCode::A: return kVK_ANSI_A;
        case KeyCode::B: return kVK_ANSI_B;
        case KeyCode::C: return kVK_ANSI_C;
        case KeyCode::D: return kVK_ANSI_D;
        case KeyCode::E: return kVK_ANSI_E;
        case KeyCode::F: return kVK_ANSI_F;
        case KeyCode::G: return kVK_ANSI_G;
        case KeyCode::H: return kVK_ANSI_H;
        case KeyCode::I: return kVK_ANSI_I;
        case KeyCode::J: return kVK_ANSI_J;
        case KeyCode::K: return kVK_ANSI_K;
        case KeyCode::L: return kVK_ANSI_L;
        case KeyCode::M: return kVK_ANSI_M;
        case KeyCode::N: return kVK_ANSI_N;
        case KeyCode::O: return kVK_ANSI_O;
        case KeyCode::P: return kVK_ANSI_P;
        case KeyCode::Q: return kVK_ANSI_Q;
        case KeyCode::R: return kVK_ANSI_R;
        case KeyCode::S: return kVK_ANSI_S;
        case KeyCode::T: return kVK_ANSI_T;
        case KeyCode::U: return kVK_ANSI_U;
        case KeyCode::V: return kVK_ANSI_V;
        case KeyCode::W: return kVK_ANSI_W;
        case KeyCode::X: return kVK_ANSI_X;
        case KeyCode::Y: return kVK_ANSI_Y;
        case KeyCode::Z: return kVK_ANSI_Z;
        case KeyCode::Num0: return kVK_ANSI_0;
        case KeyCode::Num1: return kVK_ANSI_1;
        case KeyCode::Num2: return kVK_ANSI_2;
        case KeyCode::Num3: return kVK_ANSI_3;
        case KeyCode::Num4: return kVK_ANSI_4;
        case KeyCode::Num5: return kVK_ANSI_5;
        case KeyCode::Num6: return kVK_ANSI_6;
        case KeyCode::Num7: return kVK_ANSI_7;
        case KeyCode::Num8: return kVK_ANSI_8;
        case KeyCode::Num9: return kVK_ANSI_9;
        case KeyCode::Return: return kVK_Return;
        case KeyCode::Tab: return kVK_Tab;
        case KeyCode::Space: return kVK_Space;
        default: return kVK_ANSI_K;
    }
}

static UInt32 modifiersToCarbonModifiers(ModifierFlags mods) {
    UInt32 carbonMods = 0;
    if (mods & Mod::Cmd) carbonMods |= cmdKey;
    if (mods & Mod::Option) carbonMods |= optionKey;
    if (mods & Mod::Shift) carbonMods |= shiftKey;
    if (mods & Mod::Control) carbonMods |= controlKey;
    return carbonMods;
}

void GlobalHotkeyManager::installEventHandlerIfNeeded() {
    if (!eventHandlerRef_) {
        EventTypeSpec eventType;
        eventType.eventClass = kEventClassKeyboard;
        eventType.eventKind = kEventHotKeyPressed;

        if (!gHotKeyFunction) {
            gHotKeyFunction = NewEventHandlerUPP(hotKeyHandler);
        }
        InstallApplicationEventHandler(gHotKeyFunction, 1, &eventType, NULL, (EventHandlerRef*)&eventHandlerRef_);
    }
}

uint32_t GlobalHotkeyManager::registerHotkey(KeyCode key, ModifierFlags mods, std::function<void()> callback) {
    if (key == KeyCode::None || !callback) return 0;

    installEventHandlerIfNeeded();

    // Check if duplicate (key, mods) already registered — replace it
    for (auto it = hotkeys_.begin(); it != hotkeys_.end(); ++it) {
        if (it->second.key == key && it->second.mods == mods) {
            unregisterHotkey(it->first);
            break;
        }
    }

    uint32_t id = nextId_++;
    EventHotKeyID hotKeyID;
    hotKeyID.signature = 'TKHT';
    hotKeyID.id = id;

    UInt32 vkey = keycodeToMacVirtualKey(key);
    UInt32 cmods = modifiersToCarbonModifiers(mods);

    EventHotKeyRef ref = nullptr;
    OSStatus status = RegisterEventHotKey(vkey, cmods, hotKeyID, GetApplicationEventTarget(), 0, &ref);
    if (status != noErr) {
        return 0;
    }

    HotkeyItem item;
    item.id = id;
    item.key = key;
    item.mods = mods;
    item.hotKeyRef = ref;
    item.callback = std::move(callback);

    hotkeys_[id] = std::move(item);
    return id;
}

void GlobalHotkeyManager::unregisterHotkey(uint32_t id) {
    auto it = hotkeys_.find(id);
    if (it != hotkeys_.end()) {
        if (it->second.hotKeyRef) {
            UnregisterEventHotKey(static_cast<EventHotKeyRef>(it->second.hotKeyRef));
        }
        hotkeys_.erase(it);
    }
}

void GlobalHotkeyManager::unregisterHotkey() {
    unregisterAll();
}

void GlobalHotkeyManager::unregisterAll() {
    for (auto& pair : hotkeys_) {
        if (pair.second.hotKeyRef) {
            UnregisterEventHotKey(static_cast<EventHotKeyRef>(pair.second.hotKeyRef));
        }
    }
    hotkeys_.clear();

    if (eventHandlerRef_) {
        RemoveEventHandler(static_cast<EventHandlerRef>(eventHandlerRef_));
        eventHandlerRef_ = nullptr;
    }
}

void GlobalHotkeyManager::dispatchHotkey(uint32_t id) {
    auto it = hotkeys_.find(id);
    if (it != hotkeys_.end() && it->second.callback) {
        it->second.callback();
    }
}
