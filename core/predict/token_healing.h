#pragma once

#include <string>

struct HealedPrompt {
    std::string contextWithoutPartial;
    std::string requiredTokenPrefix;
    bool hadLeadingSpace = false;
};

// Splits context + partial word so BPE tokenization does not fragment a half-typed word.
// If partialWord is non-empty, strips it from the prompt and requires the first generated
// token(s) to begin with (" " + partialWord) or partialWord.
HealedPrompt splitForTokenHealing(const std::string& contextBeforePartial, const std::string& partialWord);

// Checks whether a candidate generated string is compatible with the required prefix
// (either candidate starts with requiredPrefix, or requiredPrefix starts with candidate).
bool matchesTokenHealingPrefix(const std::string& generatedSoFar, const std::string& requiredPrefix);
