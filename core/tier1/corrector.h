#pragma once

#include "core/tier1/symspell_dict.h"
#include "core/config/app_config.h"
#include <string>

struct CorrectionResult {
    bool didCorrect = false;
    std::wstring corrected;
    int editDistance = 0;
};

CorrectionResult correctWord(const std::wstring& typed, const SymSpellDict& dict, const AppConfig& cfg, bool isSentenceStart = false);
