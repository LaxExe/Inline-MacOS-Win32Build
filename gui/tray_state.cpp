#include "gui/tray_state.h"

TrayIconState iconStateFor(bool masterEnabled) {
    return masterEnabled ? TrayIconState::Active : TrayIconState::Paused;
}
