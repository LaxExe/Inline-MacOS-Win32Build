#include "core/predict/token_healing.h"
#include <cctype>

HealedPrompt splitForTokenHealing(const std::string& contextBeforePartial, const std::string& partialWord) {
    HealedPrompt out;
    if (partialWord.empty()) {
        out.contextWithoutPartial = contextBeforePartial;
        if (!out.contextWithoutPartial.empty() && out.contextWithoutPartial.back() == ' ') {
            out.contextWithoutPartial.pop_back();
            out.requiredTokenPrefix = " ";
            out.hadLeadingSpace = true;
        } else {
            out.requiredTokenPrefix.clear();
            out.hadLeadingSpace = false;
        }
        return out;
    }

    out.contextWithoutPartial = contextBeforePartial;
    // Strip trailing space from context so BPE tokens with leading space (" word") match naturally
    if (!out.contextWithoutPartial.empty() && out.contextWithoutPartial.back() == ' ') {
        out.contextWithoutPartial.pop_back();
        out.requiredTokenPrefix = " " + partialWord;
        out.hadLeadingSpace = true;
    } else {
        out.requiredTokenPrefix = partialWord;
        out.hadLeadingSpace = false;
    }
    return out;
}

bool matchesTokenHealingPrefix(const std::string& generatedSoFar, const std::string& requiredPrefix) {
    if (requiredPrefix.empty()) return true;
    size_t n = std::min(generatedSoFar.size(), requiredPrefix.size());
    for (size_t i = 0; i < n; ++i) {
        char a = static_cast<char>(std::tolower(static_cast<unsigned char>(generatedSoFar[i])));
        char b = static_cast<char>(std::tolower(static_cast<unsigned char>(requiredPrefix[i])));
        if (a != b) return false;
    }
    // If requiredPrefix is just " " and generatedSoFar has at least 2 chars, require the 2nd char to be alphabetic
    if (requiredPrefix == " " && generatedSoFar.size() >= 2) {
        unsigned char second = static_cast<unsigned char>(generatedSoFar[1]);
        if (!std::isalpha(second)) return false;
    }
    return true;
}
