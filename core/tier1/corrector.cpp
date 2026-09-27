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
    {L"wors", L"words"}
};

CorrectionResult correctWord(const std::wstring& typed, const SymSpellDict& dict, const AppConfig& cfg, bool isSentenceStart) {
    CorrectionResult result;
    result.didCorrect = false;
    result.corrected = typed;

    if (typed.empty()) return result;

    std::wstring lowerTyped = toLower(typed);
    for (const auto& ign : cfg.ignoreWords) {
        std::wstring wign(ign.begin(), ign.end());
        if (toLower(wign) == lowerTyped) {
            return result;
        }
    }

    // 1. Check contractions and explicit auto-cleanups
    auto expIt = kCommonExpansions.find(lowerTyped);
    if (expIt != kCommonExpansions.end()) {
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

    auto suggestions = dict.lookup(lowerTyped, cfg.maxDictionaryEditDistance);

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
        
        // Compute character overlap bonus
        int matchingChars = 0;
        std::wstring sTerm = s.term;
        for (wchar_t tc : lowerTyped) {
            auto pos = sTerm.find(tc);
            if (pos != std::wstring::npos) {
                matchingChars++;
                sTerm.erase(pos, 1);
            }
        }
        double overlapRatio = static_cast<double>(matchingChars) / std::max(lowerTyped.size(), s.term.size());
        if (s.distance > 1 && overlapRatio < 0.60) {
            continue;
        }
        double score = logCount - s.distance * 2.0 + overlapRatio * 1.5;

        // Confidence threshold based on distance:
        // Dist 1 requires logCount >= 4.0 (10,000+ count in 82K dictionary)
        // Dist 2 requires logCount >= 4.8 (60,000+ count in 82K dictionary)
        double minRequiredLogCount = (s.distance == 1) ? 4.0 : 4.8;
        if (logCount < minRequiredLogCount) {
            continue;
        }

        if (score > bestScore) {
            bestScore = score;
            best = &s;
        }
    }

    // 3. Check for run-together words & compound typos (e.g. "theclass" -> "the class", "timerighte" -> "time right", "ofevening" -> "of evening")
    std::wstring bestSplitWord;
    double bestSplitScore = -1e9;
    int bestSplitDist = 1;

    if (isAllAlpha && lowerTyped.size() >= 5) {
        for (size_t split = 2; split <= lowerTyped.size() - 2; ++split) {
            std::wstring w1 = lowerTyped.substr(0, split);
            std::wstring w2 = lowerTyped.substr(split);

            // Case A: Both w1 and w2 are exact dictionary words
            if (dict.hasWord(w1) && dict.hasWord(w2)) {
                int64_t c1 = dict.getFrequency(w1);
                int64_t c2 = dict.getFrequency(w2);
                if (c1 >= 10000 && c2 >= 10000) {
                    int64_t bg = dict.getBigramFrequency(w1, w2);
                    double splitScore = std::log10(static_cast<double>(c1)) + std::log10(static_cast<double>(c2)) + (bg > 0 ? std::log10(static_cast<double>(bg)) * 0.8 : 0.0);
                    if (splitScore > bestSplitScore) {
                        bestSplitScore = splitScore;
                        bestSplitWord = w1 + L" " + w2;
                        bestSplitDist = 1;
                    }
                }
            }
            // Case B: w1 is exact word, w2 has distance 1 typo (e.g. "time" + "righte" -> "time right")
            else if (w1.size() >= 3 && w2.size() >= 3 && dict.hasWord(w1)) {
                int64_t c1 = dict.getFrequency(w1);
                if (c1 >= 10000) {
                    auto s2_list = dict.lookup(w2, 1);
                    for (const auto& s2 : s2_list) {
                        if (s2.distance == 1 && s2.count >= 50000) {
                            int64_t bg = dict.getBigramFrequency(w1, s2.term);
                            bool isTrailingExtra = (w2.size() > s2.term.size() && w2.rfind(s2.term, 0) == 0);
                            if (bg > 0 || isTrailingExtra) {
                                double bgBonus = (bg > 0) ? std::log10(static_cast<double>(bg)) * 1.5 : 0.0;
                                double trailingBonus = isTrailingExtra ? 2.0 : 0.0;
                                double splitScore = std::log10(static_cast<double>(c1)) + std::log10(static_cast<double>(s2.count)) - 2.0 + trailingBonus + bgBonus;
                                if (splitScore > bestSplitScore) {
                                    bestSplitScore = splitScore;
                                    bestSplitWord = w1 + L" " + s2.term;
                                    bestSplitDist = 2;
                                }
                            }
                        }
                    }
                }
            }
            // Case C: w1 has distance 1 typo, w2 is exact word (e.g. "wors" + "we" -> "words we")
            else if (w1.size() >= 3 && w2.size() >= 2 && dict.hasWord(w2)) {
                int64_t c2 = dict.getFrequency(w2);
                if (c2 >= 10000) {
                    auto s1_list = dict.lookup(w1, 1);
                    for (const auto& s1 : s1_list) {
                        if (s1.distance == 1 && s1.count >= 50000) {
                            int64_t bg = dict.getBigramFrequency(s1.term, w2);
                            bool isTrailingExtra = (w1.size() > s1.term.size() && w1.rfind(s1.term, 0) == 0);
                            if (bg > 0 || isTrailingExtra) {
                                double bgBonus = (bg > 0) ? std::log10(static_cast<double>(bg)) * 1.5 : 0.0;
                                double trailingBonus = isTrailingExtra ? 2.0 : 0.0;
                                double splitScore = std::log10(static_cast<double>(s1.count)) + std::log10(static_cast<double>(c2)) - 2.0 + trailingBonus + bgBonus;
                                if (splitScore > bestSplitScore) {
                                    bestSplitScore = splitScore;
                                    bestSplitWord = s1.term + L" " + w2;
                                    bestSplitDist = 2;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (best && (bestSplitWord.empty() || bestScore >= bestSplitScore - 5.0)) {
        result.didCorrect = true;
        result.corrected = applyCasing(best->term, typed);
        if (cfg.sentenceCorrectionEnabled && isSentenceStart && !result.corrected.empty()) {
            result.corrected[0] = std::towupper(result.corrected[0]);
        }
        result.editDistance = best->distance;
        return result;
    }

    if (!bestSplitWord.empty() && bestSplitScore >= 10.0) {
        result.didCorrect = true;
        size_t sp = bestSplitWord.find(L' ');
        std::wstring p1 = bestSplitWord.substr(0, sp);
        std::wstring p2 = (sp != std::wstring::npos) ? bestSplitWord.substr(sp) : L"";
        std::wstring c1 = isCapitalized(typed) ? applyCasing(p1, typed.substr(0, std::min(typed.size(), p1.size()))) : p1;
        result.corrected = c1 + p2;
        if (cfg.sentenceCorrectionEnabled && isSentenceStart && !result.corrected.empty()) {
            result.corrected[0] = std::towupper(result.corrected[0]);
        }
        result.editDistance = bestSplitDist;
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
