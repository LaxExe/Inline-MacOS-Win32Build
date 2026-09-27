#pragma once

enum class TrayIconState {
    Active,
    Paused,
    Disabled
};

TrayIconState iconStateFor(bool masterEnabled);
