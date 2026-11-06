#include <gtest/gtest.h>
#include "core/buffers/word_boundary.h"

TEST(WordBoundary, SpaceIsBoundary)         { EXPECT_TRUE(isWordBoundary(L' ')); }
TEST(WordBoundary, PeriodIsBoundary)        { EXPECT_TRUE(isWordBoundary(L'.')); }
TEST(WordBoundary, EnterIsBoundary)         { EXPECT_TRUE(isWordBoundary(L'\n')); }
TEST(WordBoundary, TabIsBoundary)           { EXPECT_TRUE(isWordBoundary(L'\t')); }
TEST(WordBoundary, LetterIsNotBoundary)     { EXPECT_FALSE(isWordBoundary(L'a')); }
TEST(WordBoundary, ApostropheIsNotBoundary) { EXPECT_FALSE(isWordBoundary(L'\'')); }

TEST(WordBoundary, CommonPrefixOptimization) {
    // recieve -> receive: prefix "rec" (length 3).
    // Word len 7, prefix 3 => delete (7-3)+1 = 5 backspaces, retype "eive "
    size_t pRecieve = commonPrefixLength(L"recieve", L"receive");
    EXPECT_EQ(pRecieve, 3u);
    EXPECT_EQ(7 - pRecieve + 1, 5u);

    // teh -> the: prefix "t" (length 1).
    // Word len 3, prefix 1 => delete (3-1)+1 = 3 backspaces, retype "he "
    size_t pTeh = commonPrefixLength(L"teh", L"the");
    EXPECT_EQ(pTeh, 1u);
    EXPECT_EQ(3 - pTeh + 1, 3u);

    // wierd -> weird: prefix "w" (length 1).
    // Word len 5, prefix 1 => delete (5-1)+1 = 5 backspaces, retype "eird "
    size_t pWierd = commonPrefixLength(L"wierd", L"weird");
    EXPECT_EQ(pWierd, 1u);
    EXPECT_EQ(5 - pWierd + 1, 5u);

    // Completely disjoint words: prefix length 0
    EXPECT_EQ(commonPrefixLength(L"apple", L"banana"), 0u);

    // Identical words: full length
    EXPECT_EQ(commonPrefixLength(L"word", L"word"), 4u);
}
