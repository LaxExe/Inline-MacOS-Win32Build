#import <Cocoa/Cocoa.h>
#include <iostream>
#include <chrono>
#include <thread>
#include "core/tier1/corrector.h"
#include "core/tier1/symspell_dict.h"
#include "core/ghosttext/predictor.h"
#include "core/undo/undo_state_machine.h"
#include "core/config/app_config.h"
#include "core/buffers/buffer_gate.h"
#include "core/buffers/word_boundary.h"

static std::string toNarrow(const std::wstring& ws) {
    std::string s(ws.begin(), ws.end());
    return s;
}

int main(int argc, char* argv[]) {
    @autoreleasepool {
        std::cout << "========================================================\n";
        std::cout << "  REAL-TIME TYPING ASSISTANT — FULL END-TO-END QA TEST  \n";
        std::cout << "========================================================\n\n";

        // 1. Load Dictionaries
        std::cout << "[1/6] Loading frequency dictionaries...\n";
        SymSpellDict dict;
        bool d1 = dict.loadDictionary("dictionaries/frequency_dictionary_en_82_765.txt", 0, 1);
        bool d2 = dict.loadBigramDictionary("dictionaries/frequency_bigramdictionary_en_243_342.txt", 0, 2);

        if (d1 && d2) {
            std::cout << "      [PASS] Dictionaries loaded (82k unigrams + 243k bigrams)\n";
        } else {
            std::cerr << "      [FAIL] Failed to load dictionaries!\n";
            return 1;
        }

        // 2. Test Tier-1 Autocorrect on Worked Examples
        std::cout << "\n[2/6] Testing Tier-1 Autocorrect engine...\n";
        AppConfig cfg;
        cfg.maxDictionaryEditDistance = 2;

        struct TestCase {
            std::wstring input;
            std::wstring expected;
            bool shouldCorrect;
        };

        std::vector<TestCase> cases = {
            {L"teh", L"the", true},
            {L"this9", L"this", true},
            {L"aliek", L"like", true},
            {L"applicaiton", L"application", true},
            {L"ht", L"ht", false},         // Ambiguous short word -> suppressed
            {L"ty0wop", L"ty0wop", false}   // Distance > 2 -> uncorrected
        };

        for (const auto& tc : cases) {
            auto res = correctWord(tc.input, dict, cfg);
            if (res.didCorrect == tc.shouldCorrect && (!tc.shouldCorrect || res.corrected == tc.expected)) {
                std::cout << "      [PASS] \"" << toNarrow(tc.input) << "\" -> " 
                          << (res.didCorrect ? ("\"" + toNarrow(res.corrected) + "\" (Corrected)") : "(Left uncorrected)") << "\n";
            } else {
                std::cout << "      [FAIL] \"" << toNarrow(tc.input) << "\" expected " << toNarrow(tc.expected) 
                          << " but got " << toNarrow(res.corrected) << "\n";
                return 1;
            }
        }

        // 3. Test Ghost Text Predictions
        std::cout << "\n[3/6] Testing Ghost-Text Predictions...\n";
        GhostPredictor predictor(dict);
        ContextWindow ctx;
        CurrentWordBuffer word;

        // Test prefix autocomplete
        word.chars = L"prog";
        auto pred1 = predictor.predictNext(ctx, word, cfg);
        if (pred1.has_value() && (*pred1 == L"ram" || *pred1 == L"ramming" || *pred1 == L"ress")) {
            std::cout << "      [PASS] Prefix \"prog\" -> Predicted completion: \"" << toNarrow(*pred1) << "\" (full: \"" << toNarrow(word.chars + *pred1) << "\")\n";
        } else {
            std::cout << "      [FAIL] Prefix \"prog\" prediction missing\n";
            return 1;
        }

        word.chars = L"appl";
        auto pred2 = predictor.predictNext(ctx, word, cfg);
        if (pred2.has_value() && (*pred2 == L"ication" || *pred2 == L"e")) {
            std::cout << "      [PASS] Prefix \"appl\" -> Predicted completion: \"" << toNarrow(*pred2) << "\"\n";
        } else {
            std::cout << "      [FAIL] Prefix \"appl\" prediction missing\n";
            return 1;
        }

        // 4. Test Undo-on-Backspace State Machine
        std::cout << "\n[4/6] Testing Undo-on-Backspace State Machine...\n";
        UndoStateMachine undoSM;
        undoSM.onWordCorrected(L"teh", L"the");
        auto undoAction = undoSM.onBackspace();
        if (undoAction.type == UndoAction::RevertCorrection && undoAction.revertedText == L"teh") {
            std::cout << "      [PASS] Immediate backspace after \"the\" reverts to \"teh\"\n";
        } else {
            std::cout << "      [FAIL] Undo on backspace did not revert!\n";
            return 1;
        }

        // 5. Test Secure Field Immunity (Zero Leakage)
        std::cout << "\n[5/6] Testing Secure-Field Immunity...\n";
        CurrentWordBuffer secBuf;
        bool wrote = appendCharIfAllowed(secBuf, L'P', /*secureFieldActive=*/true);
        if (!wrote && secBuf.chars.empty()) {
            std::cout << "      [PASS] Zero characters written when secure field is active\n";
        } else {
            std::cout << "      [FAIL] Password character leaked into buffer!\n";
            return 1;
        }

        // 6. Test Word Boundary Segmentation
        std::cout << "\n[6/6] Testing Word Boundary Segmentation...\n";
        if (isWordBoundary(L' ') && isWordBoundary(L'.') && isWordBoundary(L'\n') &&
            !isWordBoundary(L'a') && !isWordBoundary(L'\'')) {
            std::cout << "      [PASS] Delimiters correctly identified (\"don't\" preserved as one word)\n";
        } else {
            std::cout << "      [FAIL] Word boundary check failed\n";
            return 1;
        }

        std::cout << "\n========================================================\n";
        std::cout << "  ALL END-TO-END QA TESTS PASSED CLEANLY (100% OK)      \n";
        std::cout << "========================================================\n";
    }
    return 0;
}
