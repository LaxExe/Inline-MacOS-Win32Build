#include "gui/hotkey_blocklist.h"

bool isHotkeyBlocklisted(KeyCode key, ModifierFlags mods) {
    // Exact match for Cmd+Q
    if (key == KeyCode::Q && mods == Mod::Cmd) {
        return true;
    }
    // Block standard system single-modifier shortcuts
    if (mods == Mod::Cmd) {
        if (key == KeyCode::W || key == KeyCode::C || key == KeyCode::V ||
            key == KeyCode::X || key == KeyCode::Z || key == KeyCode::A ||
            key == KeyCode::Space) {
            return true;
        }
    }
    return false;
}
