#pragma once

#include <cstdint>

constexpr int64_t kOurAppMagicTagValue = 0x5F3A9C1E2B7D44LL;

bool isSyntheticEvent(int64_t sourceUserDataTag);
void suppressHookFor(uint64_t durationMs);
