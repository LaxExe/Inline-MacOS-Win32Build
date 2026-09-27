#include "core/undo/undo_state_machine.h"

UndoStateMachine::UndoStateMachine() = default;

CorrectResult UndoStateMachine::onWordCorrected(const std::wstring& original, const std::wstring& corrected) {
    CorrectResult res;
    if (state_ == State::Suppressed && suppressedWord_ == original) {
        res.suppressed = true;
        return res;
    }

    state_ = State::EligibleForUndo;
    lastOriginal_ = original;
    lastCorrected_ = corrected;
    suppressedWord_.clear();
    res.suppressed = false;
    return res;
}

UndoActionResult UndoStateMachine::onBackspace() {
    UndoActionResult action;
    if (state_ == State::EligibleForUndo) {
        action.type = UndoAction::RevertCorrection;
        action.revertedText = lastOriginal_;
        action.correctedText = lastCorrected_;
        state_ = State::Suppressed;
        suppressedWord_ = lastOriginal_;
        return action;
    }

    action.type = UndoAction::NormalDelete;
    return action;
}

void UndoStateMachine::onOtherKey() {
    state_ = State::Idle;
    suppressedWord_.clear();
}
