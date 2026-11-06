#pragma once

#include <string>
#include <vector>
#include "core/tier1/symspell_dict.h"
#include "core/config/app_config.h"

struct PredictionResult {
    std::wstring text;       // exact suffix to render after caret & insert on Tab
    double confidence = 0.0; // [0.0, 1.0]
    int wordCount = 0;
    bool fromLLM = false;
    int64_t latencyUs = 0;
};

int maxWordsForLength(AppConfig::PredictionLength length);

class NgramPredictor {
public:
    explicit NgramPredictor(const SymSpellDict& dict);

    PredictionResult predict(
        const std::vector<std::wstring>& contextWords,
        const std::wstring& partialWord,
        AppConfig::PredictionLength lengthMode = AppConfig::PredictionLength::Balanced,
        bool isSentenceStart = false
    ) const;

private:
    const SymSpellDict& dict_;
};
