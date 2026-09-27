#pragma once

#include <string>

enum class UndoAction {
    NormalDelete,
    RevertCorrection
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
    UndoActionResult onBackspace();
    void onOtherKey();

private:
    enum class State {
        Idle,
        EligibleForUndo,
        Suppressed
    };

    State state_ = State::Idle;
    std::wstring lastOriginal_;
    std::wstring lastCorrected_;
    std::wstring suppressedWord_;
};
