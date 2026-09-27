#include "core/ghosttext/dismissal.h"

bool shouldDismissGhostText(const std::wstring& suggestion, wchar_t typedChar, size_t matchedSoFar) {
    if (suggestion.empty() || matchedSoFar >= suggestion.size()) {
        return true;
    }
    return suggestion[matchedSoFar] != typedChar;
}
