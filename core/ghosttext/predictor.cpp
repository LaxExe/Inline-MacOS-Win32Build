#include "core/ghosttext/predictor.h"

GhostPredictor::GhostPredictor(const SymSpellDict& dict) : dict_(dict) {}

std::optional<std::wstring> GhostPredictor::predictNext(const ContextWindow& context, const CurrentWordBuffer& currentWord, const AppConfig& cfg) {
    if (!cfg.ghostTextEnabled) return std::nullopt;

    // Do NOT correct or complete in between characters of a word
    if (!currentWord.chars.empty()) {
        return std::nullopt;
    }

    // Predict the entire NEXT word based on the preceding context word
    if (!context.words.empty()) {
        const auto& lastWord = context.words.back();
        auto nextWord = dict_.lookupBigramNext(lastWord);
        if (nextWord.has_value() && !nextWord->empty()) {
            return nextWord;
        }
    }

    return std::nullopt;
}
