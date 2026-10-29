#include "core/tier1/corrector.h"
#include <cwctype>
#include <algorithm>
#include <cmath>

static std::wstring toLower(const std::wstring& s) {
    std::wstring res = s;
    for (auto& c : res) c = std::towlower(c);
    return res;
}

static bool isAllUpper(const std::wstring& s) {
    if (s.empty()) return false;
    for (auto c : s) {
        if (std::iswalpha(c) && !std::iswupper(c)) return false;
    }
    return true;
}

static bool isCapitalized(const std::wstring& s) {
    if (s.empty()) return false;
    return std::iswupper(s[0]);
}

static std::wstring applyCasing(const std::wstring& target, const std::wstring& original) {
    if (original.empty() || target.empty()) return target;
    if (isAllUpper(original)) {
        std::wstring res = target;
        for (auto& c : res) c = std::towupper(c);
        return res;
    }
    if (isCapitalized(original)) {
        std::wstring res = target;
        res[0] = std::towupper(res[0]);
        return res;
    }
    return target;
}

// True if every character of `small` appears in `big` in order (i.e. `small` is `big` with letters dropped).
static bool isSubsequence(const std::wstring& small, const std::wstring& big) {
    size_t i = 0;
    for (wchar_t c : big) {
        if (i < small.size() && small[i] == c) ++i;
    }
    return i == small.size();
}

// True if `a` and `b` are identical except for one swapped adjacent pair (e.g. "wnat" / "want").
static bool isAdjacentTransposition(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size() || a.size() < 2) return false;
    size_t i = 0;
    while (i < a.size() && a[i] == b[i]) ++i;
    if (i + 1 >= a.size()) return false;
    if (a[i] != b[i + 1] || a[i + 1] != b[i]) return false;
    return a.compare(i + 2, std::wstring::npos, b, i + 2, std::wstring::npos) == 0;
}

// QWERTY neighbours: a substituted letter that sits next to the intended one is a very likely slip.
static bool areKeysAdjacent(wchar_t x, wchar_t y) {
    static const wchar_t* rows[3] = { L"qwertyuiop", L"asdfghjkl", L"zxcvbnm" };
    auto locate = [](wchar_t c, int& row, int& col) {
        for (int r = 0; r < 3; ++r) {
            for (int k = 0; rows[r][k]; ++k) {
                if (rows[r][k] == c) { row = r; col = k; return true; }
            }
        }
        return false;
    };
    int rx, cx, ry, cy;
    if (!locate(x, rx, cx) || !locate(y, ry, cy)) return false;
    if (rx == ry) return std::abs(cx - cy) == 1;
    if (std::abs(rx - ry) != 1) return false;
    // Rows are staggered: a key touches the keys directly above/below and the one diagonal to its upper-left/lower-right.
    int upperCol = (rx < ry) ? cx : cy;
    int lowerCol = (rx < ry) ? cy : cx;
    int off = lowerCol - upperCol;
    return off == 0 || off == -1;
}

// Bonus applied on top of the frequency score for candidates whose relationship to the typed text
// matches how typos really happen. Without this, a very frequent but unrelated word beats the
// intended one (e.g. "wich" -> "with" instead of "which", "wnat" -> "what" instead of "want").
static double typoShapeBonus(const std::wstring& typed, const std::wstring& cand, int distance) {
    if (distance == 1) {
        if (isAdjacentTransposition(typed, cand)) return 2.5;
        if (typed.size() + 1 == cand.size() && isSubsequence(typed, cand)) return 2.8;   // missing letter
        if (typed.size() == cand.size()) {                                                 // substitution
            for (size_t i = 0; i < typed.size(); ++i) {
                if (typed[i] != cand[i]) return areKeysAdjacent(typed[i], cand[i]) ? 1.0 : 0.0;
            }
        }
        if (typed.size() == cand.size() + 1 && isSubsequence(cand, typed)) return 0.8;   // extra letter
        return 0.0;
    }
    if (distance == 2) {
        if (typed.size() + 2 == cand.size() && isSubsequence(typed, cand)) return 1.2;   // two missing letters
        if (typed.size() == cand.size() + 2 && isSubsequence(cand, typed)) return 0.6;   // two extra letters
    }
    return 0.0;
}

static const std::unordered_map<std::wstring, std::wstring> kCommonExpansions = {
    {L"i", L"I"},
    {L"im", L"I'm"},
    {L"ive", L"I've"},
    {L"id", L"I'd"},
    {L"ill", L"I'll"},
    {L"dont", L"don't"},
    {L"cant", L"can't"},
    {L"wont", L"won't"},
    {L"didnt", L"didn't"},
    {L"doesnt", L"doesn't"},
    {L"isnt", L"isn't"},
    {L"arent", L"aren't"},
    {L"wasnt", L"wasn't"},
    {L"werent", L"weren't"},
    {L"hasnt", L"hasn't"},
    {L"havent", L"haven't"},
    {L"hadnt", L"hadn't"},
    {L"couldnt", L"couldn't"},
    {L"shouldnt", L"shouldn't"},
    {L"wouldnt", L"wouldn't"},
    {L"theyre", L"they're"},
    {L"youre", L"you're"},
    {L"weve", L"we've"},
    {L"theyve", L"they've"},
    {L"youve", L"you've"},
    {L"whats", L"what's"},
    {L"thats", L"that's"},
    {L"lets", L"let's"},
    {L"theres", L"there's"},
    {L"heres", L"here's"},
    {L"hows", L"how's"},
    {L"whos", L"who's"},
    {L"wheres", L"where's"},
    {L"couldve", L"could've"},
    {L"shouldve", L"should've"},
    {L"wouldve", L"would've"},
    {L"mustnt", L"mustn't"},
    {L"mightnt", L"mightn't"},
    {L"neednt", L"needn't"},
    {L"itll", L"it'll"},
    {L"yall", L"y'all"},
    {L"youll", L"you'll"},
    {L"theyll", L"they'll"},
    {L"doenst", L"doesn't"},
    {L"knwo", L"know"},
    {L"smothly", L"smoothly"},
    {L"applicaiton", L"application"},
    {L"nessasry", L"necessary"},
    {L"permision", L"permission"},
    {L"troble", L"trouble"},
    {L"imediatly", L"immediately"},
    {L"completly", L"completely"},
    {L"autp", L"auto"},
    {L"autpcrreccts", L"autocorrects"},
    {L"teh", L"the"},
    {L"het", L"the"},
    {L"th", L"the"},
    {L"ths", L"this"},
    {L"intdo", L"into"},
    {L"duto", L"auto"},
    {L"neht", L"next"},
    {L"asw", L"as"},
    {L"wors", L"words"},
    {L"alot", L"a lot"},   // one word -> two words, SymSpell cannot produce this
    {L"fo", L"of"},        // <=2 chars are skipped by the fuzzy path, so handle the common transposition here
    {L"og", L"of"}         // common QWERTY adjacent-key slip (g next to f)
};

CorrectionResult correctWord(const std::wstring& typed, const SymSpellDict& dict, const AppConfig& cfg, bool isSentenceStart, const std::wstring& prevWord) {
    CorrectionResult result;
    result.didCorrect = false;
    result.corrected = typed;

    if (typed.empty() || typed.size() > 24) return result;

    std::wstring lowerTyped = toLower(typed);
    for (const auto& ign : cfg.ignoreWords) {
        if (ign.size() == lowerTyped.size()) {
            bool match = true;
            for (size_t i = 0; i < ign.size(); ++i) {
                if (static_cast<wchar_t>(std::tolower(static_cast<unsigned char>(ign[i]))) != lowerTyped[i]) {
                    match = false;
                    break;
                }
            }
            if (match) return result;
        }
    }

    // Extract clean previous word for bigram context
    std::wstring cleanPrev;
    for (wchar_t c : prevWord) {
        if (std::iswalnum(c) || c == L'\'') {
            cleanPrev.push_back(std::towlower(c));
        }
    }

    // 1. Check contractions and explicit auto-cleanups
    auto expIt = kCommonExpansions.find(lowerTyped);
    if (expIt != kCommonExpansions.end()) {
        if (lowerTyped == L"og" && isAllUpper(typed)) {
            return result; // Preserve all-caps "OG" (acronym/slang)
        }
        std::wstring exp = expIt->second;
        if ((cfg.sentenceCorrectionEnabled && isSentenceStart) || isCapitalized(typed)) {
            if (!exp.empty()) exp[0] = std::towupper(exp[0]);
        }
        if (exp != typed) {
            result.didCorrect = true;
            result.corrected = exp;
            result.editDistance = 1;
            return result;
        }
        return result;
    }

    // 2. Check if already valid dictionary word
    if (dict.hasWord(lowerTyped)) {
        if (cfg.sentenceCorrectionEnabled && isSentenceStart && std::iswlower(typed[0])) {
            result.didCorrect = true;
            result.corrected = typed;
            result.corrected[0] = std::towupper(result.corrected[0]);
            result.editDistance = 0;
            return result;
        }
        result.didCorrect = false;
        result.corrected = typed;
        return result;
    }

    if (typed.size() <= 2) {
        if (cfg.sentenceCorrectionEnabled && isSentenceStart && std::iswlower(typed[0])) {
            result.didCorrect = true;
            result.corrected = typed;
            result.corrected[0] = std::towupper(result.corrected[0]);
            result.editDistance = 0;
            return result;
        }
        return result;
    }

    bool isAllAlpha = true;
    for (wchar_t c : lowerTyped) {
        if (!std::iswalpha(c)) {
            isAllAlpha = false;
            break;
        }
    }

    int effectiveMaxDist = (typed.size() <= 4)
        ? std::min(1, cfg.maxDictionaryEditDistance)
        : cfg.maxDictionaryEditDistance;
    auto suggestions = dict.lookup(lowerTyped, effectiveMaxDist);

    // Rank candidates by log frequency penalizing edit distance
    const Suggestion* best = nullptr;
    double bestScore = -1e9;

    for (const auto& s : suggestions) {
        if (s.distance == 0) {
            if (cfg.sentenceCorrectionEnabled && isSentenceStart && std::iswlower(typed[0])) {
                result.didCorrect = true;
                result.corrected = typed;
                result.corrected[0] = std::towupper(result.corrected[0]);
                result.editDistance = 0;
                return result;
            }
            result.didCorrect = false;
            result.corrected = typed;
            return result;
        }
        if (s.distance > cfg.maxDictionaryEditDistance) continue;
        if (typed.size() <= 4 && s.distance > 1) continue;
        if (typed.size() <= 2 && s.distance > 1) continue;
        if (s.count <= 0) continue;

        double logCount = std::log10(static_cast<double>(s.count));

        // Confidence threshold based on distance:
        // Dist 1 requires logCount >= 4.0 (10,000+ count in 82K dictionary)
        // Dist 2 requires logCount >= 4.8 (60,000+ count in 82K dictionary)
        double minRequiredLogCount = (s.distance == 1) ? 4.0 : 4.8;
        if (logCount < minRequiredLogCount) {
            continue;
        }

        // Compute character overlap bonus using a register bitmask (zero heap allocations)
        int matchingChars = 0;
        uint32_t usedMask = 0;
        const size_t termLen = std::min<size_t>(s.term.size(), 32);
        for (wchar_t tc : lowerTyped) {
            for (size_t k = 0; k < termLen; ++k) {
                if (!(usedMask & (1u << k)) && s.term[k] == tc) {
                    usedMask |= (1u << k);
                    matchingChars++;
                    break;
                }
            }
        }
        double overlapRatio = static_cast<double>(matchingChars) / std::max(lowerTyped.size(), s.term.size());
        if (s.distance > 1 && overlapRatio < 0.60) {
            continue;
        }
        double score = logCount - s.distance * 2.0 + overlapRatio * 1.5 + typoShapeBonus(lowerTyped, s.term, s.distance);

        // Context-aware bigram boost: if this candidate forms a frequent, natural bigram with the previous word
        if (!cleanPrev.empty()) {
            int64_t biFreq = dict.getBigramFrequency(cleanPrev, s.term);
            if (biFreq > 0) {
                score += std::log10(static_cast<double>(biFreq)) * 1.5;
            }
        }

        if (score > bestScore) {
            bestScore = score;
            best = &s;
        }
    }

    // 3. Check for run-together words & compound typos (e.g. "theclass" -> "the class", "twowords" -> "two words", "whenI" -> "when I", "timerighte" -> "time right")
    size_t camelCaseSplit = 0;
    bool hasCamelCase = false;
    for (size_t i = 1; i < typed.size(); ++i) {
        if (std::iswlower(typed[i - 1]) && std::iswupper(typed[i])) {
            hasCamelCase = true;
            camelCaseSplit = i;
            break;
        }
    }

    std::wstring bestSplitW1;
    std::wstring bestSplitW2;
    double bestSplitScore = -1e9;
    int bestSplitDist = 1;
    int64_t bestSplitBiFreq = 0;
    bool bestSplitIsCamel = false;

    if (cfg.splitRunTogetherWords && isAllAlpha && lowerTyped.size() >= 3) {
        for (size_t split = 1; split < lowerTyped.size(); ++split) {
            // Disallow 1-letter fragments unless they are valid English 1-letter words ('a' or 'i')
            if (split == 1 && lowerTyped[0] != L'a' && lowerTyped[0] != L'i') continue;
            if (split == lowerTyped.size() - 1 && lowerTyped[split] != L'a' && lowerTyped[split] != L'i') continue;

            std::wstring w1 = lowerTyped.substr(0, split);
            std::wstring w2 = lowerTyped.substr(split);

            // Handle common slips inside compounds (e.g. "og" -> "of")
            if (w1 == L"og") w1 = L"of";
            if (w2 == L"og") w2 = L"of";

            bool isCamelAtSplit = (hasCamelCase && split == camelCaseSplit);

            // Case A: Both w1 and w2 are exact dictionary words
            if (dict.hasWord(w1) && dict.hasWord(w2)) {
                int64_t c1 = dict.getFrequency(w1);
                int64_t c2 = dict.getFrequency(w2);
                int64_t biFreq = dict.getBigramFrequency(w1, w2);

                // Multi-letter words must meet the 10,000 unigram frequency threshold
                if (w1.size() >= 2 && c1 < 10000) continue;
                if (w2.size() >= 2 && c2 < 10000) continue;

                // Single-letter words ('a' or 'i') require high confidence:
                // either explicit CamelCase at the boundary, or an established bigram >= 100,000 count
                if (w1.size() == 1 && !isCamelAtSplit && biFreq < 100000) continue;
                if (w2.size() == 1 && !isCamelAtSplit && biFreq < 100000) continue;

                double splitScore = std::log10(static_cast<double>(c1)) + std::log10(static_cast<double>(c2));
                if (biFreq > 0) {
                    splitScore += std::log10(static_cast<double>(biFreq)) * 1.5;
                }
                if (isCamelAtSplit) {
                    splitScore += 6.0; // explicit user CamelCase capitalization bonus
                }

                if (splitScore > bestSplitScore) {
                    bestSplitScore = splitScore;
                    bestSplitW1 = w1;
                    bestSplitW2 = w2;
                    bestSplitDist = 1;
                    bestSplitBiFreq = biFreq;
                    bestSplitIsCamel = isCamelAtSplit;
                }
            }
            // Case B: w1 is exact word, w2 has trailing extra letter typo (e.g. "timerighte" -> "time right")
            else if (!best && cfg.maxDictionaryEditDistance >= 2 && w1.size() >= 3 && w2.size() >= 3 && dict.hasWord(w1)) {
                int64_t c1 = dict.getFrequency(w1);
                if (c1 >= 10000) {
                    auto s2_list = dict.lookup(w2, 1);
                    for (const auto& s2 : s2_list) {
                        if (s2.distance == 1 && s2.count >= 50000) {
                            bool isTrailingExtra = (w2.size() > s2.term.size() && w2.substr(0, s2.term.size()) == s2.term);
                            if (isTrailingExtra) {
                                int64_t biFreq = dict.getBigramFrequency(w1, s2.term);
                                double splitScore = std::log10(static_cast<double>(c1)) + std::log10(static_cast<double>(s2.count)) - 1.0;
                                if (biFreq > 0) {
                                    splitScore += std::log10(static_cast<double>(biFreq)) * 1.5;
                                }
                                if (splitScore > bestSplitScore) {
                                    bestSplitScore = splitScore;
                                    bestSplitW1 = w1;
                                    bestSplitW2 = s2.term;
                                    bestSplitDist = 2;
                                    bestSplitBiFreq = biFreq;
                                    bestSplitIsCamel = false;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    bool preferSplit = false;
    if (!bestSplitW1.empty() && bestSplitDist <= cfg.maxDictionaryEditDistance) {
        if (!best) {
            preferSplit = (bestSplitScore >= 10.0);
        } else if (best->distance >= 2) {
            preferSplit = (bestSplitDist == 1 && bestSplitScore >= 12.0);
        } else if (best->distance == 1) {
            // Distance-1 single-word candidate: only split when HIGHLY CONFIDENT
            // 1. Explicit CamelCase boundary typed by user (e.g. "whenI", "twoWords", "thatI"), OR
            // 2. High-frequency bigram (>= 500,000 count) where w1 is at least 2 chars (e.g. "when i", "that i"),
            //    preventing words like "adress" -> "address" from erroneously splitting into "a dress".
            if (bestSplitIsCamel) {
                preferSplit = true;
            } else if (bestSplitBiFreq >= 500000 && bestSplitScore >= 20.0 && bestSplitW1.size() >= 2) {
                preferSplit = true;
            }
        }
    }

    if (preferSplit) {
        result.didCorrect = true;
        result.editDistance = bestSplitDist;

        std::wstring c1 = bestSplitW1;
        std::wstring c2 = bestSplitW2;

        // Clean up word 1
        if (c1 == L"i") {
            c1 = L"I";
        } else if (isAllUpper(typed)) {
            for (auto& c : c1) c = std::towupper(c);
        } else if ((cfg.sentenceCorrectionEnabled && isSentenceStart) || isCapitalized(typed)) {
            if (!c1.empty()) c1[0] = std::towupper(c1[0]);
        }

        // Clean up word 2
        if (c2 == L"i") {
            c2 = L"I";
        } else if (c2 == L"im") {
            c2 = L"I'm";
        } else if (c2 == L"ive") {
            c2 = L"I've";
        } else if (c2 == L"id") {
            c2 = L"I'd";
        } else if (c2 == L"ill") {
            c2 = L"I'll";
        } else if (isAllUpper(typed)) {
            for (auto& c : c2) c = std::towupper(c);
        }

        result.corrected = c1 + L" " + c2;
        return result;
    }

    if (best) {
        result.didCorrect = true;
        result.corrected = applyCasing(best->term, typed);
        if (cfg.sentenceCorrectionEnabled && isSentenceStart && !result.corrected.empty()) {
            result.corrected[0] = std::towupper(result.corrected[0]);
        }
        result.editDistance = best->distance;
        return result;
    }

    if (cfg.sentenceCorrectionEnabled && isSentenceStart && std::iswlower(typed[0])) {
        result.didCorrect = true;
        result.corrected = typed;
        result.corrected[0] = std::towupper(result.corrected[0]);
        result.editDistance = 0;
        return result;
    }

    return result;
}
