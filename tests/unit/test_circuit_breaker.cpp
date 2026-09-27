#include <gtest/gtest.h>
#include "core/reliability/circuit_breaker.h"

TEST(CircuitBreaker, TripsAtNineCorrectionsInTwoSeconds) {
    std::deque<TimestampMs> ts;
    for (int i = 0; i < 9; ++i) ts.push_back(i * 100); // 9 corrections across 800ms
    EXPECT_TRUE(circuitBreakerShouldTrip(ts, /*now=*/900, /*threshold=*/8, /*windowMs=*/2000));
}
TEST(CircuitBreaker, DoesNotTripAtNormalTypingRate) {
    std::deque<TimestampMs> ts = {0, 500, 1000, 1500}; // 4 corrections over 2s, human-plausible
    EXPECT_FALSE(circuitBreakerShouldTrip(ts, /*now=*/1500, /*threshold=*/8, /*windowMs=*/2000));
}
TEST(CircuitBreaker, IgnoresEventsOutsideWindow) {
    std::deque<TimestampMs> ts = {0, 100, 200, 5000, 5100, 5200}; // old burst should have expired
    EXPECT_FALSE(circuitBreakerShouldTrip(ts, /*now=*/5200, /*threshold=*/8, /*windowMs=*/2000));
}
