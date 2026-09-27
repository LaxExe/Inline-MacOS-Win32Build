#include "core/buffers/buffer_gate.h"

bool appendCharIfAllowed(CurrentWordBuffer& buf, wchar_t ch, bool secureFieldActive) {
    if (secureFieldActive) {
        return false;
    }
    buf.chars.push_back(ch);
    return true;
}
