#include <gtest/gtest.h>
#include "core/ghost/ghost_layout.h"
#include "core/ghost/ghost_session.h"
#include "core/predict/ngram_predictor.h"
#include "core/predict/suggestion_filter.h"
#include "core/predict/token_healing.h"
#include "core/tier1/symspell_dict.h"

namespace {
// Monospace 8px/char metric for deterministic layout testing
double mono8Measure(const std::wstring& s, const std::string&, double) {
    return static_cast<double>(s.size()) * 8.0;
}
} // namespace

TEST(GhostLayout, FitsEntirelyOnFirstLineWhenEnoughSpace) {
    CaretGeometry cg;
    cg.caret = {100.0, 50.0, 2.0, 18.0};
    cg.field = {20.0, 40.0, 400.0, 200.0};
    cg.lineStartX = 28.0;
    cg.lineHeight = 20.0;
    cg.isSingleLine = false;
    cg.atEditableEnd = true;

    GhostLayoutResult res = computeGhostLayout(L" the meeting", cg, false, mono8Measure);
    ASSERT_TRUE(res.valid);
    ASSERT_EQ(res.lines.size(), 1u);
    EXPECT_EQ(res.lines[0].text, L"the meeting");
    EXPECT_DOUBLE_EQ(res.lines[0].x, 101.2);
    EXPECT_DOUBLE_EQ(res.lines[0].y, 50.0);
}

TEST(GhostLayout, WrapsOverflowingWordsToLineStartXOnNextLine) {
    CaretGeometry cg;
    // Field right edge = 20 + 200 = 220; padR = 8 -> usable right = 212
    // Caret right edge = 160 + 1 = 161 -> remaining on line 1 = 51px (fits up to 6 chars at 8px/char)
    cg.caret = {160.0, 50.0, 2.0, 18.0};
    cg.field = {20.0, 40.0, 200.0, 200.0};
    cg.lineStartX = 28.0;
    cg.lineHeight = 20.0;
    cg.isSingleLine = false;
    cg.atEditableEnd = true;
    cg.caretAtEOF = true;

    GhostLayoutResult res = computeGhostLayout(L" to the team tomorrow", cg, false, mono8Measure);
    ASSERT_TRUE(res.valid);
    ASSERT_GE(res.lines.size(), 2u);
    EXPECT_EQ(res.lines[0].text, L"to the");
    EXPECT_DOUBLE_EQ(res.lines[0].x, 161.2);
    EXPECT_DOUBLE_EQ(res.lines[0].y, 50.0);

    EXPECT_DOUBLE_EQ(res.lines[1].x, 28.0);
    EXPECT_DOUBLE_EQ(res.lines[1].y, 70.0);
}

TEST(GhostLayout, TruncatesCleanlyInSingleLineField) {
    CaretGeometry cg;
    cg.caret = {160.0, 50.0, 2.0, 18.0};
    cg.field = {20.0, 40.0, 200.0, 28.0};
    cg.lineStartX = 28.0;
    cg.lineHeight = 20.0;
    cg.isSingleLine = true;
    cg.atEditableEnd = true;

    GhostLayoutResult res = computeGhostLayout(L" to the meeting", cg, false, mono8Measure);
    ASSERT_TRUE(res.valid);
    ASSERT_EQ(res.lines.size(), 1u);
    EXPECT_EQ(res.lines[0].text, L"to the");
    EXPECT_EQ(res.fittedText, L"to the");
}

TEST(GhostLayout, HidesMidWordSuffixWhenSuffixOverflowsFirstLine) {
    CaretGeometry cg;
    // Caret right = 202, field right = 220 - 6 = 214 -> remaining = 12px (only 1 char fits)
    cg.caret = {200.0, 50.0, 2.0, 18.0};
    cg.field = {20.0, 40.0, 200.0, 200.0};
    cg.lineStartX = 28.0;
    cg.lineHeight = 20.0;
    cg.isSingleLine = false;
    cg.atEditableEnd = true;

    // Mid-word suffix "rrow" (32px) cannot wrap without splitting the word across lines
    GhostLayoutResult res = computeGhostLayout(L"rrow", cg, true, mono8Measure);
    EXPECT_FALSE(res.valid);
}

TEST(GhostSession, TabSwallowMatrixAndAutorepeatGuard) {
    GhostSession session;
    EXPECT_FALSE(session.isVisible());

    constexpr int64_t kMacKeyTab = 48;
    constexpr uint64_t kFlagShift = 0x00020000ULL;
    constexpr uint64_t kFlagCmd   = 0x00100000ULL;

    EXPECT_FALSE(session.shouldSwallowKey(kMacKeyTab, 0, false, true, KeyCode::Tab, 1000));

    uint64_t gen = session.currentGeneration();
    PredictionResult pred;
    pred.text = L" the meeting";
    pred.confidence = 0.9;
    pred.wordCount = 2;
    pred.fromLLM = false;
    std::wstring rendered;
    ASSERT_TRUE(session.applyCandidatePrediction(gen, L"ctx", pred, rendered));
    EXPECT_TRUE(session.isVisible());

    // Plain Tab when ghost is visible -> swallow!
    EXPECT_TRUE(session.shouldSwallowKey(kMacKeyTab, 0, false, true, KeyCode::Tab, 1000));

    // Shift+Tab -> never swallow
    EXPECT_FALSE(session.shouldSwallowKey(kMacKeyTab, kFlagShift, false, true, KeyCode::Tab, 1000));

    // Cmd+Tab -> never swallow
    EXPECT_FALSE(session.shouldSwallowKey(kMacKeyTab, kFlagCmd, false, true, KeyCode::Tab, 1000));

    // Consume whole suggestion
    std::wstring chunk, rem;
    ASSERT_TRUE(session.consumeAccept(/*acceptMode=*/0, /*nowNs=*/1000000000LL, chunk, rem));
    EXPECT_EQ(chunk, L" the meeting");
    EXPECT_TRUE(rem.empty());
    EXPECT_FALSE(session.isVisible());

    // Immediate autorepeat Tab within 500ms after accepting -> swallowed so it doesn't indent!
    EXPECT_TRUE(session.shouldSwallowKey(kMacKeyTab, 0, /*isAutorepeat=*/true, true, KeyCode::Tab, 1200000000LL));
    // Non-autorepeat Tab after ghost is gone -> passes through!
    EXPECT_FALSE(session.shouldSwallowKey(kMacKeyTab, 0, /*isAutorepeat=*/false, true, KeyCode::Tab, 1200000000LL));
}

TEST(GhostSession, WordByWordAcceptAdvancesRemainingGhostText) {
    GhostSession session;
    uint64_t gen = session.currentGeneration();
    PredictionResult pred{L" the meeting tomorrow", 0.9, 3, false, 0};
    std::wstring rendered;
    ASSERT_TRUE(session.applyCandidatePrediction(gen, L"ctx", pred, rendered));

    std::wstring chunk, rem;
    ASSERT_TRUE(session.consumeAccept(/*acceptMode=*/1, 1000, chunk, rem));
    EXPECT_EQ(chunk, L" the");
    EXPECT_TRUE(session.isVisible());
    EXPECT_EQ(session.currentVisibleText(), L" meeting tomorrow");

    ASSERT_TRUE(session.consumeAccept(/*acceptMode=*/1, 2000, chunk, rem));
    EXPECT_EQ(chunk, L" meeting");
    EXPECT_TRUE(session.isVisible());
    EXPECT_EQ(session.currentVisibleText(), L" tomorrow");

    ASSERT_TRUE(session.consumeAccept(/*acceptMode=*/1, 3000, chunk, rem));
    EXPECT_EQ(chunk, L" tomorrow");
    EXPECT_FALSE(session.isVisible());
}

TEST(GhostSession, TypeThroughConsumesMatchingPrefixAndReconcilesLLM) {
    GhostSession session;
    uint64_t gen = session.currentGeneration();

    // Instant layer shows "ank you" for "th|ank you"
    PredictionResult p0{L"ank you", 0.8, 2, false, 0};
    std::wstring rendered;
    EXPECT_TRUE(session.applyCandidatePrediction(gen, L"th", p0, rendered));
    EXPECT_EQ(session.currentVisibleText(), L"ank you");

    // User types 'a' -> advances to "nk you"
    std::wstring rem;
    EXPECT_TRUE(session.onCharacterTyped(L'a', rem));
    EXPECT_EQ(session.currentVisibleText(), L"nk you");
    gen = session.currentGeneration();

    // Smart LLM arrives extending "nk you" -> "nk you so much"
    PredictionResult p1{L"nk you so much", 0.9, 4, true, 0};
    EXPECT_TRUE(session.applyCandidatePrediction(gen, L"tha", p1, rendered));
    EXPECT_EQ(session.currentVisibleText(), L"nk you so much");

    // Smart LLM arrives with conflicting replacement "nksgiving" -> rejected to prevent visual jump
    PredictionResult pConflict{L"nksgiving", 0.9, 1, true, 0};
    EXPECT_FALSE(session.applyCandidatePrediction(gen, L"tha", pConflict, rendered));
    EXPECT_EQ(session.currentVisibleText(), L"nk you so much");
}

TEST(SuggestionFilter, BlocksProfanityUnlessAlreadyInUserContextAndBlocksPII) {
    EXPECT_TRUE(isSuggestionSafe(L" the meeting tomorrow", L"looking forward to "));
    EXPECT_FALSE(isSuggestionSafe(L" fuck that", L"I want to "));
    EXPECT_TRUE(isSuggestionSafe(L" shit happens", L"well shit "));
    EXPECT_FALSE(isSuggestionSafe(L" user@example.com", L"email me at "));
    EXPECT_FALSE(isSuggestionSafe(L" 4155551234", L"call me at "));
}

TEST(TokenHealing, SplitsTrailingPartialWordAndRequiresSpacePrefixAfterWhitespace) {
    auto midWord = splitForTokenHealing("Thank you for your ", "pati");
    EXPECT_EQ(midWord.contextWithoutPartial, "Thank you for your");
    EXPECT_EQ(midWord.requiredTokenPrefix, " pati");

    auto afterSpace = splitForTokenHealing("Looking forward to ", "");
    EXPECT_EQ(afterSpace.contextWithoutPartial, "Looking forward to");
    EXPECT_EQ(afterSpace.requiredTokenPrefix, " ");
}

TEST(NgramPredictor, CompletesActiveWordAndChainsNextWordsInstantly) {
    SymSpellDict dict;
    dict.addWord(L"thank", 50000000);
    dict.addWord(L"thanksgiving", 100000);
    dict.addWord(L"important", 40000000);
    dict.addWord(L"going", 80000000);
    dict.addWord(L"you", 90000000);
    NgramPredictor predictor(dict);

    // 1. Mid-word completion + multi-word phrase continuation ("tha" -> "nk you so much")
    auto p1 = predictor.predict({}, L"tha", AppConfig::PredictionLength::Balanced, true);
    EXPECT_EQ(p1.text, L"nk you so much");

    // 2. Completed word before Space ("thank" -> " you so much")
    auto p2 = predictor.predict({}, L"thank", AppConfig::PredictionLength::Balanced, true);
    EXPECT_EQ(p2.text, L" you so much");

    // 3. Single-char prefix after context word ("thank " + "y" -> "ou so much")
    auto p3 = predictor.predict({L"thank"}, L"y", AppConfig::PredictionLength::Balanced, false);
    EXPECT_EQ(p3.text, L"ou so much");

    // 4. Mid-word completion + phrase ("imp" -> "ortant to make sure")
    auto p4 = predictor.predict({}, L"imp", AppConfig::PredictionLength::Balanced, false);
    EXPECT_EQ(p4.text, L"ortant to make sure");

    // 5. One word mode (SingleWord): ONLY completes the current active word ("tha" -> "nk") without chaining
    auto pSingle = predictor.predict({}, L"tha", AppConfig::PredictionLength::SingleWord, true);
    EXPECT_EQ(pSingle.text, L"nk");
    EXPECT_EQ(pSingle.wordCount, 1);
}

TEST(GhostLayout, PlacesMidWordSuggestionStrictlyToRightOfCaret) {
    CaretGeometry cg;
    cg.caret = {100.0, 50.0, 2.5, 18.0};
    cg.field = {20.0, 40.0, 400.0, 200.0};
    cg.lineStartX = 28.0;
    cg.lineHeight = 20.0;
    cg.isSingleLine = false;
    cg.atEditableEnd = true;

    // Mid-word suggestion should sit strictly to the right of caret (100.0 + 1.2 = 101.2)
    GhostLayoutResult res = computeGhostLayout(L"nks", cg, true, mono8Measure);
    ASSERT_TRUE(res.valid);
    ASSERT_EQ(res.lines.size(), 1u);
    EXPECT_EQ(res.lines[0].text, L"nks");
    EXPECT_DOUBLE_EQ(res.lines[0].x, 101.2);
    EXPECT_DOUBLE_EQ(res.lines[0].y, 50.0);
}

TEST(GhostLayout, PreservesLeadingSpaceWhenDocumentEndsWithWordChar) {
    CaretGeometry cg;
    cg.caret = {100.0, 50.0, 2.5, 18.0};
    cg.field = {20.0, 40.0, 400.0, 200.0};
    cg.lineStartX = 28.0;
    cg.lineHeight = 20.0;
    cg.isSingleLine = false;
    cg.atEditableEnd = true;
    cg.textTail = L"meeting"; // Document ends with letter 'g' (no trailing space)

    // In One-Word mode or word completion, " tomorrow" must retain its leading space
    // so it doesn't touch or collide with "meeting"
    GhostLayoutResult res = computeGhostLayout(L" tomorrow", cg, false, mono8Measure);
    ASSERT_TRUE(res.valid);
    ASSERT_EQ(res.lines.size(), 1u);
    EXPECT_EQ(res.lines[0].text, L" tomorrow");
    EXPECT_EQ(res.fittedText, L" tomorrow");
    EXPECT_DOUBLE_EQ(res.lines[0].x, 101.2);
}

TEST(GhostLayout, StripsRedundantLeadingSpaceWhenDocumentEndsWithSpace) {
    CaretGeometry cg;
    cg.caret = {100.0, 50.0, 2.5, 18.0};
    cg.field = {20.0, 40.0, 400.0, 200.0};
    cg.lineStartX = 28.0;
    cg.lineHeight = 20.0;
    cg.isSingleLine = false;
    cg.atEditableEnd = true;
    cg.textTail = L"meeting "; // Document already ends with space

    // When document already has space, " tomorrow" must have its leading space stripped
    // so no double space is displayed or injected
    GhostLayoutResult res = computeGhostLayout(L" tomorrow", cg, false, mono8Measure);
    ASSERT_TRUE(res.valid);
    ASSERT_EQ(res.lines.size(), 1u);
    EXPECT_EQ(res.lines[0].text, L"tomorrow");
    EXPECT_EQ(res.fittedText, L"tomorrow");
    EXPECT_DOUBLE_EQ(res.lines[0].x, 101.2);
}
