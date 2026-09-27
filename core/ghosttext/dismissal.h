#pragma once

#include <string>
#include <cstddef>

bool shouldDismissGhostText(const std::wstring& suggestion, wchar_t typedChar, size_t matchedSoFar);
