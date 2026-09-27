#include <gtest/gtest.h>
#include "core/buffers/word_boundary.h"

TEST(WordBoundary, SpaceIsBoundary)         { EXPECT_TRUE(isWordBoundary(L' ')); }
TEST(WordBoundary, PeriodIsBoundary)        { EXPECT_TRUE(isWordBoundary(L'.')); }
TEST(WordBoundary, EnterIsBoundary)         { EXPECT_TRUE(isWordBoundary(L'\n')); }
TEST(WordBoundary, TabIsBoundary)           { EXPECT_TRUE(isWordBoundary(L'\t')); }
TEST(WordBoundary, LetterIsNotBoundary)     { EXPECT_FALSE(isWordBoundary(L'a')); }
TEST(WordBoundary, ApostropheIsNotBoundary) { EXPECT_FALSE(isWordBoundary(L'\'')); }
