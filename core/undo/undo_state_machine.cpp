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

void UndoStateMachine::onGhostAccepted(const std::wstring& acceptedText, const std::wstring& priorWordBuffer) {
    if (acceptedText.empty()) return;
    state_ = State::EligibleForGhostUndo;
    lastOriginal_ = priorWordBuffer;
    lastCorrected_ = acceptedText;
    suppressedWord_.clear();
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
    if (state_ == State::EligibleForGhostUndo) {
        action.type = UndoAction::RevertGhostAccept;
        action.revertedText = lastOriginal_;
        action.correctedText = lastCorrected_;
        state_ = State::Idle;
        return action;
    }

    action.type = UndoAction::NormalDelete;
    return action;
}

void UndoStateMachine::onOtherKey() {
    state_ = State::Idle;
    suppressedWord_.clear();
}

void UndoStateMachine::reset() {
    state_ = State::Idle;
    lastOriginal_.clear();
    lastCorrected_.clear();
    suppressedWord_.clear();
}
