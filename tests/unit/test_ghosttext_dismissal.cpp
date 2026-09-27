#include <gtest/gtest.h>
#include "core/ghosttext/dismissal.h"

TEST(GhostTextDismissal, MatchingCharKeepsSuggestion) {
    EXPECT_FALSE(shouldDismissGhostText(L"world", L'w', 0));
}
TEST(GhostTextDismissal, NonMatchingCharDismisses) {
    EXPECT_TRUE(shouldDismissGhostText(L"world", L'x', 0));
}
TEST(GhostTextDismissal, PartialMatchThenMismatchDismisses) {
    EXPECT_FALSE(shouldDismissGhostText(L"world", L'w', 0));
    EXPECT_FALSE(shouldDismissGhostText(L"world", L'o', 1));
    EXPECT_TRUE(shouldDismissGhostText(L"world", L'z', 2)); // "wo" then wrong char
}
TEST(GhostTextDismissal, NoPartialNarrowingAllowed) {
    // dismissal is binary match-or-gone, never "narrow the suggestion down"
    EXPECT_TRUE(shouldDismissGhostText(L"world", L'r', 1)); // skips ahead, not a valid narrowing
}
