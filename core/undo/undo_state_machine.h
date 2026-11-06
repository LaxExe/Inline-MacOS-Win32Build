#pragma once

#include <string>

enum class UndoAction {
    NormalDelete,
    RevertCorrection,
    RevertGhostAccept
};

struct UndoActionResult {
    UndoAction type = UndoAction::NormalDelete;
    std::wstring revertedText;
    std::wstring correctedText;
};

struct CorrectResult {
    bool suppressed = false;
};

class UndoStateMachine {
public:
    UndoStateMachine();

    CorrectResult onWordCorrected(const std::wstring& original, const std::wstring& corrected);
    void onGhostAccepted(const std::wstring& acceptedText, const std::wstring& priorWordBuffer = L"");
    UndoActionResult onBackspace();
    void onOtherKey();
    void reset();

private:
    enum class State {
        Idle,
        EligibleForUndo,
        EligibleForGhostUndo,
        Suppressed
    };

    State state_ = State::Idle;
    std::wstring lastOriginal_;
    std::wstring lastCorrected_;
    std::wstring suppressedWord_;
};
