#include <gtest/gtest.h>
#include "core/tier1/corrector.h"

class Tier1Test : public ::testing::Test {
protected:
    static inline SymSpellDict dict;
    static inline AppConfig cfg;
    static void SetUpTestSuite() {
        dict.loadDictionary("dictionaries/frequency_dictionary_en_82_765.txt", 0, 1);
        cfg.maxDictionaryEditDistance = 2;
    }
};

TEST_F(Tier1Test, FixesStrayDigit)        { EXPECT_EQ(correctWord(L"this9", dict, cfg).corrected, L"this"); }
TEST_F(Tier1Test, FixesAdjacentTranspose) { EXPECT_EQ(correctWord(L"teh",   dict, cfg).corrected, L"the");  }
TEST_F(Tier1Test, FixesDistanceTwoTypo)   { EXPECT_EQ(correctWord(L"breidfly", dict, cfg).corrected, L"briefly"); }

TEST_F(Tier1Test, LeavesOutOfScopeWordsUncorrected) {
    // ty0wop / ht exceed what distance-2 lookup can disambiguate — tier-1 must NOT force a guess
    EXPECT_FALSE(correctWord(L"ty0wop", dict, cfg).didCorrect);
    EXPECT_FALSE(correctWord(L"ht",     dict, cfg).didCorrect);
}

TEST_F(Tier1Test, LeavesRealUncommonWordAlone) {
    // A name/technical term not in the 82K dict but with no close common-word match under the
    // score-gap threshold (§3.4) must not be "corrected" to an unrelated common word.
    dict.mergePersonalDictionary({{L"kubernetes", 1}});
    EXPECT_FALSE(correctWord(L"kubernetes", dict, cfg).didCorrect);
}

TEST_F(Tier1Test, FixesContractionsAndCleanups) {
    EXPECT_EQ(correctWord(L"dont", dict, cfg).corrected, L"don't");
    EXPECT_EQ(correctWord(L"cant", dict, cfg).corrected, L"can't");
    EXPECT_EQ(correctWord(L"im", dict, cfg).corrected, L"I'm");
    EXPECT_EQ(correctWord(L"i", dict, cfg).corrected, L"I");
    EXPECT_EQ(correctWord(L"knwo", dict, cfg).corrected, L"know");
    EXPECT_EQ(correctWord(L"smothly", dict, cfg).corrected, L"smoothly");
}

TEST_F(Tier1Test, CapitalizesAtSentenceStart) {
    AppConfig sentCfg = cfg;
    sentCfg.sentenceCorrectionEnabled = true;
    EXPECT_EQ(correctWord(L"hello", dict, sentCfg, /*isSentenceStart=*/true).corrected, L"Hello");
    EXPECT_EQ(correctWord(L"dont", dict, sentCfg, /*isSentenceStart=*/true).corrected, L"Don't");
    EXPECT_EQ(correctWord(L"teh", dict, sentCfg, /*isSentenceStart=*/true).corrected, L"The");
}

TEST_F(Tier1Test, AvoidsDistanceTwoOnShortWords) {
    // "asdf" is length 4, distance 2 from "and" — must NOT be corrected to "and"
    EXPECT_FALSE(correctWord(L"asdf", dict, cfg).didCorrect);
}

TEST_F(Tier1Test, DoesNotCapitalizeWhenSentenceCorrectionDisabled) {
    AppConfig noSentCfg = cfg;
    noSentCfg.sentenceCorrectionEnabled = false;
    EXPECT_FALSE(correctWord(L"hey", dict, noSentCfg, /*isSentenceStart=*/true).didCorrect);
    EXPECT_EQ(correctWord(L"hey", dict, noSentCfg, /*isSentenceStart=*/true).corrected, L"hey");
}

TEST_F(Tier1Test, RespectsEditDistanceCeiling) {
    AppConfig cfg1 = cfg; cfg1.maxDictionaryEditDistance = 1;
    // breidfly is distance 2 from "briefly" — with the ceiling set to 1, it must NOT be corrected
    EXPECT_FALSE(correctWord(L"breidfly", dict, cfg1).didCorrect);
}

TEST_F(Tier1Test, FixesDistanceTwoVocabularyTypos) {
    EXPECT_EQ(correctWord(L"breidfly", dict, cfg).corrected, L"briefly");
    EXPECT_EQ(correctWord(L"sharlpy", dict, cfg).corrected, L"sharply");
    EXPECT_EQ(correctWord(L"ccourse", dict, cfg).corrected, L"course");
    EXPECT_EQ(correctWord(L"th", dict, cfg).corrected, L"the");
}

TEST_F(Tier1Test, FixesRunTogetherWords) {
    EXPECT_EQ(correctWord(L"theclass", dict, cfg).corrected, L"the class");
    EXPECT_EQ(correctWord(L"ofevening", dict, cfg).corrected, L"of evening");
    EXPECT_EQ(correctWord(L"timerighte", dict, cfg).corrected, L"time right");
    EXPECT_EQ(correctWord(L"ths", dict, cfg).corrected, L"this");
    EXPECT_EQ(correctWord(L"intdo", dict, cfg).corrected, L"into");
}
