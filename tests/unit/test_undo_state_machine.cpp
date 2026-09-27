#include <gtest/gtest.h>
#include "core/undo/undo_state_machine.h"

TEST(UndoStateMachine, BackspaceImmediatelyAfterCorrectionReverts) {
    UndoStateMachine sm;
    sm.onWordCorrected(/*original=*/L"teh", /*corrected=*/L"the");
    auto action = sm.onBackspace();
    EXPECT_EQ(action.type, UndoAction::RevertCorrection);
    EXPECT_EQ(action.revertedText, L"teh");
    EXPECT_EQ(action.correctedText, L"the");
}
TEST(UndoStateMachine, TypingOtherCharClearsUndoEligibility) {
    UndoStateMachine sm;
    sm.onWordCorrected(L"teh", L"the");
    sm.onOtherKey(); // user typed something else — undo window closes
    auto action = sm.onBackspace();
    EXPECT_EQ(action.type, UndoAction::NormalDelete);
}
TEST(UndoStateMachine, NoLoopOnRepeatedCorrectRevert) {
    UndoStateMachine sm;
    sm.onWordCorrected(L"teh", L"the");
    sm.onBackspace();                 // reverts to "teh", suppress scoped to this occurrence
    auto secondAttempt = sm.onWordCorrected(L"teh", L"the"); // same occurrence, should be suppressed
    EXPECT_TRUE(secondAttempt.suppressed);
}
TEST(UndoStateMachine, SuppressClearsOnManualEdit) {
    UndoStateMachine sm;
    sm.onWordCorrected(L"teh", L"the");
    sm.onBackspace();
    sm.onOtherKey(); // user manually edits the reverted word — suppress must clear
    auto thirdAttempt = sm.onWordCorrected(L"teh", L"the");
    EXPECT_FALSE(thirdAttempt.suppressed);
}
