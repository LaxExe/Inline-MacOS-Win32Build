#include "platform/win/hook/synthetic_filter.h"

bool isSyntheticEvent(int64_t sourceUserDataTag) {
    return sourceUserDataTag == kOurAppMagicTagValue;
}

void suppressHookFor(uint64_t durationMs) {
    (void)durationMs;
}
