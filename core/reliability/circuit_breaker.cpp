#include "core/reliability/circuit_breaker.h"

bool circuitBreakerShouldTrip(const std::deque<TimestampMs>& recentCorrectionTimestamps, TimestampMs now, int thresholdPerWindow, DurationMs windowMs) {
    int count = 0;
    TimestampMs windowStart = now - windowMs;
    for (auto ts : recentCorrectionTimestamps) {
        if (ts >= windowStart && ts <= now) {
            count++;
        }
    }
    return count > thresholdPerWindow;
}
