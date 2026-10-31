#include "core/predict/suggestion_filter.h"
#include <cwctype>
#include <unordered_set>
#include <algorithm>

static const std::unordered_set<std::wstring>& blockedTerms() {
    static const std::unordered_set<std::wstring> kBlocked = {
        L"fuck", L"fucking", L"fucked", L"fucker", L"shit", L"shitty",
        L"bitch", L"bastard", L"asshole", L"dick", L"pussy", L"cunt",
        L"slut", L"whore", L"retard", L"retarded", L"nigger", L"nigga",
        L"faggot", L"fag", L"kike", L"spic", L"chink", L"kill", L"suicide"
    };
    return kBlocked;
}

static std::vector<std::wstring> splitLowerAlphaTokens(const std::wstring& s) {
    std::vector<std::wstring> tokens;
    std::wstring cur;
    for (wchar_t c : s) {
        if (std::iswalpha(c)) {
            cur.push_back(static_cast<wchar_t>(std::towlower(c)));
        } else if (!cur.empty()) {
            tokens.push_back(std::move(cur));
            cur.clear();
        }
    }
    if (!cur.empty()) tokens.push_back(std::move(cur));
    return tokens;
}

bool isSuggestionSafe(const std::wstring& suggestion, const std::wstring& recentContext) {
    if (suggestion.empty()) return false;

    bool hasAlpha = false;
    int consecutiveDigits = 0;
    for (wchar_t c : suggestion) {
        if (c == L'@') return false; // Email pattern
        if (std::iswalpha(c)) hasAlpha = true;
        if (std::iswdigit(c)) {
            if (++consecutiveDigits >= 5) return false; // Phone / OTP / card pattern
        } else {
            consecutiveDigits = 0;
        }
    }
    if (!hasAlpha) return false;

    std::wstring lowerSug;
    lowerSug.reserve(suggestion.size());
    for (wchar_t c : suggestion) lowerSug.push_back(static_cast<wchar_t>(std::towlower(c)));
    if (lowerSug.find(L"http://") != std::wstring::npos ||
        lowerSug.find(L"https://") != std::wstring::npos ||
        lowerSug.find(L"www.") != std::wstring::npos) {
        return false;
    }

    auto sugTokens = splitLowerAlphaTokens(suggestion);
    if (sugTokens.empty()) return false;

    auto ctxTokens = splitLowerAlphaTokens(recentContext);
    std::unordered_set<std::wstring> ctxSet(ctxTokens.begin(), ctxTokens.end());
    const auto& blocked = blockedTerms();

    for (const auto& tok : sugTokens) {
        if (blocked.count(tok) && !ctxSet.count(tok)) {
            return false;
        }
    }
    return true;
}

std::wstring clampSuggestionWords(const std::wstring& suggestion, int maxWords) {
    if (suggestion.empty() || maxWords <= 0) return L"";

    std::wstring out;
    out.reserve(suggestion.size());
    int words = 0;
    bool inWord = false;

    for (size_t i = 0; i < suggestion.size(); ++i) {
        wchar_t c = suggestion[i];
        if (c == L'\n' || c == L'\r' || c == L'\t') break;

        bool isSpace = (c == L' ');
        if (!isSpace && !inWord) {
            if (words >= maxWords) {
                // Trim trailing space if we stopped before a new word
                while (!out.empty() && out.back() == L' ') out.pop_back();
                break;
            }
            inWord = true;
            words++;
        } else if (isSpace && inWord) {
            inWord = false;
        }

        out.push_back(c);

        if (c == L'.' || c == L'!' || c == L'?') {
            break;
        }
    }

    while (!out.empty() && (out.back() == L' ' || out.back() == L',' || out.back() == L';' || out.back() == L':')) {
        out.pop_back();
    }
    return out;
}

std::wstring extractAcceptChunk(const std::wstring& suggestion, int acceptMode, std::wstring& outRemaining) {
    outRemaining.clear();
    if (suggestion.empty()) return L"";
    if (acceptMode != 1) {
        return suggestion;
    }

    // One-word mode: include any leading spaces plus the first contiguous non-space word
    size_t i = 0;
    while (i < suggestion.size() && suggestion[i] == L' ') ++i;
    while (i < suggestion.size() && suggestion[i] != L' ') ++i;

    std::wstring chunk = suggestion.substr(0, i);
    outRemaining = suggestion.substr(i);
    return chunk;
}
