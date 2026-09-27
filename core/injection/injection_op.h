#pragma once

#include <string>

struct InjectionOp {
    enum Type { BACKSPACE_N, TYPE_TEXT, REPLACE_TEXT, MOVE_CARET } type = TYPE_TEXT;
    int count = 0;
    std::wstring text;
    int generation_token = 0;
    enum Source { TIER1, TIER2, GHOST_ACCEPT, UNDO_CORRECTION } source = TIER1;
};
