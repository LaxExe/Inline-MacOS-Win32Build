#include "platform/mac/hook/synthetic_filter.h"
#include <chrono>
#include <atomic>

static std::atomic<uint64_t> gSuppressUntilMs{0};

static uint64_t getNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

bool isSyntheticEvent(int64_t sourceUserDataTag) {
    return sourceUserDataTag == kOurAppMagicTagValue;
}

void suppressHookFor(uint64_t durationMs) {
    (void)durationMs;
}

