#pragma once

#include <deque>
#include <cstdint>

using TimestampMs = int64_t;
using DurationMs = int64_t;

bool circuitBreakerShouldTrip(const std::deque<TimestampMs>& recentCorrectionTimestamps, TimestampMs now, int thresholdPerWindow, DurationMs windowMs);
