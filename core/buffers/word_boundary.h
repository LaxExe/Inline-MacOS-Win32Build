#pragma once

#include <cwctype>
#include <string>

bool isWordBoundary(wchar_t ch);

inline size_t commonPrefixLength(const std::wstring& a, const std::wstring& b) {
    size_t p = 0;
    while (p < a.size() && p < b.size() && a[p] == b[p]) {
        ++p;
    }
    return p;
}
