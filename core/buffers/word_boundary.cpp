#include "core/buffers/word_boundary.h"

bool isWordBoundary(wchar_t ch) {
    if (ch == L' ' || ch == L'.' || ch == L'\n' || ch == L'\r' || ch == L'\t' ||
        ch == L',' || ch == L'!' || ch == L'?' || ch == L';' || ch == L':' ||
        ch == L'(' || ch == L')' || ch == L'[' || ch == L']' || ch == L'{' ||
        ch == L'}' || ch == L'"' || ch == L'/' || ch == L'\\' || ch == L'\0') {
        return true;
    }
    // Apostrophe and hyphen within words are not boundaries
    if (ch == L'\'' || ch == L'’' || ch == L'-') {
        return false;
    }
    return false;
}
