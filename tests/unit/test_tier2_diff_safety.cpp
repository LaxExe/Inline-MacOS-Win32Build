#include <gtest/gtest.h>
#include "core/tier2/diff_safety.h"

TEST(DiffSafety, AllowsSmallDiff) {
    std::vector<WordDiff> diff = { {2, L"teh", L"the"} }; // 1 of 10 words changed
    EXPECT_FALSE(exceedsChangeCeiling(diff, /*totalWords=*/10, /*ceilingFraction=*/0.4f));
}
TEST(DiffSafety, RejectsLargeDiff) {
    std::vector<WordDiff> diff = { {0,L"a",L"x"},{1,L"b",L"y"},{2,L"c",L"z"},{3,L"d",L"w"},{4,L"e",L"v"} };
    EXPECT_TRUE(exceedsChangeCeiling(diff, /*totalWords=*/10, /*ceilingFraction=*/0.4f)); // 5/10 = 50% > 40%
}
TEST(DiffSafety, ExactlyAtCeilingIsAllowed) {
    std::vector<WordDiff> diff = { {0,L"a",L"x"},{1,L"b",L"y"},{2,L"c",L"z"},{3,L"d",L"w"} };
    EXPECT_FALSE(exceedsChangeCeiling(diff, /*totalWords=*/10, /*ceilingFraction=*/0.4f)); // exactly 40%
}
