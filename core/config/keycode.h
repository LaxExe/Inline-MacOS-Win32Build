#pragma once

#include <cstdint>
#include <string>

enum class KeyCode : uint32_t {
    None = 0,
    A = 1, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Return, Tab, Space, Backspace, Escape,
    LeftArrow, RightArrow, UpArrow, DownArrow,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Unknown = 9999
};

namespace Mod {
    constexpr uint32_t None = 0;
    constexpr uint32_t Cmd = 1 << 0;
    constexpr uint32_t Option = 1 << 1;
    constexpr uint32_t Shift = 1 << 2;
    constexpr uint32_t Control = 1 << 3;
}
using ModifierFlags = uint32_t;
