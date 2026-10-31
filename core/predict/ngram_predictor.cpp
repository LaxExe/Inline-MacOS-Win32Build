#include "core/predict/ngram_predictor.h"
#include "core/predict/suggestion_filter.h"
#include <cwctype>
#include <unordered_map>
#include <chrono>

int maxWordsForLength(AppConfig::PredictionLength length) {
    switch (length) {
        case AppConfig::PredictionLength::SingleWord:   return 1;
        case AppConfig::PredictionLength::Conservative: return 2;
        case AppConfig::PredictionLength::Balanced:     return 3;
        case AppConfig::PredictionLength::Verbose:      return 5;
    }
    return 3;
}

static std::wstring toLowerW(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t c : s) out.push_back(static_cast<wchar_t>(std::towlower(c)));
    return out;
}

static bool isAllUpperW(const std::wstring& s) {
    if (s.size() < 2) return false;
    for (wchar_t c : s) {
        if (std::iswalpha(c) && !std::iswupper(c)) return false;
    }
    return true;
}

static const std::unordered_map<std::wstring, std::wstring>& multiWordPhrases() {
    static const std::unordered_map<std::wstring, std::wstring> kPhrases = {
        // 1-word high-confidence continuations
        {L"thank", L"you so much"},
        {L"thanks", L"for letting me know"},
        {L"looking", L"forward to"},
        {L"forward", L"to hearing from you"},
        {L"please", L"let me know"},
        {L"let", L"me know if"},
        {L"hope", L"you are doing well"},
        {L"would", L"like to"},
        {L"could", L"you please"},
        {L"should", L"be able to"},
        {L"going", L"to be"},
        {L"want", L"to make sure"},
        {L"wanted", L"to follow up"},
        {L"need", L"to be"},
        {L"able", L"to get"},
        {L"working", L"on the"},
        {L"think", L"that we"},
        {L"know", L"if you need"},
        {L"feel", L"free to reach out"},
        {L"talk", L"to you soon"},
        {L"sounds", L"good to me"},
        {L"makes", L"sense to me"},
        {L"happy", L"to help"},
        {L"excited", L"to share"},
        {L"sorry", L"for the delay"},
        {L"apologies", L"for the delay"},
        {L"attached", L"is the"},
        {L"following", L"up on"},
        {L"schedule", L"a quick call"},
        {L"meeting", L"with the team"},
        {L"questions", L"please let me know"},
        {L"because", L"of the"},
        {L"however", L"we can"},
        {L"whether", L"or not"},
        {L"welcome", L"to the"},
        {L"according", L"to the"},
        {L"instead", L"of the"},
        {L"important", L"to make sure"},
        {L"available", L"for a quick"},
        {L"definitely", L"let you know"},
        {L"appreciate", L"your help"},

        // 2-word & 3-word continuations
        {L"thank you", L"so much"},
        {L"thank you for", L"your time"},
        {L"thanks for", L"letting me know"},
        {L"looking forward", L"to hearing from you"},
        {L"looking forward to", L"hearing from you"},
        {L"let me", L"know if you"},
        {L"let me know", L"if you need anything"},
        {L"please let", L"me know if"},
        {L"please let me", L"know if you"},
        {L"i am", L"going to"},
        {L"i will", L"send over the"},
        {L"i have", L"been working on"},
        {L"i would", L"like to"},
        {L"i want", L"to make sure"},
        {L"i wanted", L"to follow up"},
        {L"i wanted to", L"follow up on"},
        {L"i think", L"we should"},
        {L"i need", L"to get"},
        {L"i can", L"help with that"},
        {L"we are", L"going to"},
        {L"we can", L"schedule a"},
        {L"we should", L"be able to"},
        {L"we have", L"been working on"},
        {L"we will", L"keep you posted"},
        {L"you can", L"find the"},
        {L"you are", L"doing well"},
        {L"you have", L"any questions"},
        {L"if you", L"have any questions"},
        {L"if you have", L"any questions"},
        {L"if you need", L"anything else"},
        {L"as soon", L"as possible"},
        {L"as soon as", L"possible"},
        {L"have a", L"great day"},
        {L"hope you", L"are doing well"},
        {L"hope this", L"finds you well"},
        {L"i hope", L"this finds you well"},
        {L"would love", L"to chat"},
        {L"would like", L"to schedule"},
        {L"sounds good", L"to me"},
        {L"makes sense", L"to me"},
        {L"in the", L"meantime"},
        {L"at the", L"same time"},
        {L"on the", L"other hand"},
        {L"by the", L"way"},
        {L"for the", L"first time"},
        {L"to the", L"team"},
        {L"of the", L"project"},
        {L"in order", L"to make"},
        {L"feel free", L"to reach out"},
        {L"feel free to", L"reach out"},
        {L"do you", L"have time"},
        {L"can you", L"please send"},
        {L"could you", L"please share"},
        {L"let us", L"know what you think"},
        {L"this is", L"going to be"},
        {L"it is", L"important to"},
        {L"there is", L"a lot of"},
        {L"not sure", L"if we"},
        {L"just wanted", L"to check in"},
        {L"wanted to", L"check in on"},
        {L"check in", L"on the"},
        {L"follow up", L"on the"}
    };
    return kPhrases;
}

NgramPredictor::NgramPredictor(const SymSpellDict& dict) : dict_(dict) {}

static std::wstring buildNextWordChain(
    const SymSpellDict& dict,
    const std::wstring& prevWord,
    const std::wstring& curWord,
    int wordsAlready,
    int maxWords
) {
    if (wordsAlready >= maxWords) return L"";
    const auto& phrases = multiWordPhrases();

    if (!prevWord.empty()) {
        std::wstring pairKey = prevWord + L" " + curWord;
        auto pit = phrases.find(pairKey);
        if (pit != phrases.end()) {
            return clampSuggestionWords(pit->second, maxWords - wordsAlready);
        }
    }
    auto pit1 = phrases.find(curWord);
    if (pit1 != phrases.end()) {
        return clampSuggestionWords(pit1->second, maxWords - wordsAlready);
    }

    std::wstring chained;
    std::wstring w = curWord;
    int added = wordsAlready;
    while (added < maxWords) {
        auto nexts = dict.getNextWords(w, 4);
        if (nexts.empty() || nexts[0].probability < 0.045) break;
        const std::wstring& nw = nexts[0].nextWord;
        if (nw == w) break;
        if (!chained.empty()) chained += L" ";
        chained += nw;
        added++;

        // Check if (w + " " + nw) has a curated phrase continuation
        if (added < maxWords) {
            auto pit2 = phrases.find(w + L" " + nw);
            if (pit2 != phrases.end()) {
                chained += L" " + pit2->second;
                chained = clampSuggestionWords(chained, maxWords - wordsAlready);
                break;
            }
        }
        w = nw;
    }
    return chained;
}

PredictionResult NgramPredictor::predict(
    const std::vector<std::wstring>& contextWords,
    const std::wstring& partialWord,
    AppConfig::PredictionLength lengthMode,
    bool isSentenceStart
) const {
    auto t0 = std::chrono::steady_clock::now();
    PredictionResult res;
    const int maxWords = maxWordsForLength(lengthMode);

    std::wstring prevWord = contextWords.empty() ? L"" : toLowerW(contextWords.back());
    std::wstring prevTwoWords;
    if (contextWords.size() >= 2) {
        prevTwoWords = toLowerW(contextWords[contextWords.size() - 2]) + L" " + prevWord;
    }
    std::wstring prevThreeWords;
    if (contextWords.size() >= 3) {
        prevThreeWords = toLowerW(contextWords[contextWords.size() - 3]) + L" " + prevTwoWords;
    }

    const auto& phrases = multiWordPhrases();

    // Case 1: Completing a partial word (1+ chars when context exists, 2+ chars at start)
    if (!partialWord.empty()) {
        if (partialWord.size() < 2 && prevWord.empty()) return res;
        std::wstring lowerPart = toLowerW(partialWord);

        // 1a. Check if (prevTwoWords / prevWord) + partialWord matches a curated multi-word phrase
        for (const auto& ctxKey : {prevTwoWords, prevWord}) {
            if (ctxKey.empty()) continue;
            auto pit = phrases.find(ctxKey);
            if (pit != phrases.end()) {
                if (pit->second.size() > lowerPart.size() &&
                    pit->second.compare(0, lowerPart.size(), lowerPart) == 0) {
                    std::wstring rem = pit->second.substr(lowerPart.size());
                    if (isAllUpperW(partialWord)) {
                        for (wchar_t& c : rem) c = static_cast<wchar_t>(std::towupper(c));
                    }
                    rem = clampSuggestionWords(rem, maxWords);
                    if (isSuggestionSafe(rem)) {
                        res.text = rem;
                        res.confidence = 0.88;
                        res.wordCount = 1;
                        for (wchar_t c : res.text) if (c == L' ') res.wordCount++;
                        res.latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - t0).count();
                        return res;
                    }
                }
            }
        }

        // 1b. Look up prefix completions
        auto cands = dict_.completePrefix(partialWord, prevWord, 5);

        // 1c. Check if the user has ALREADY finished typing a complete word (e.g. "thank", "going", "want")
        // whose frequency (or phrase match) dominates any longer single-word extension.
        if (lowerPart.size() >= 3 && maxWords > 1) {
            int64_t exactScore = dict_.getFrequency(lowerPart);
            if (!prevWord.empty()) {
                exactScore += dict_.getBigramFrequency(prevWord, lowerPart) * 600;
            }
            bool hasCuratedPhrase = (phrases.find(lowerPart) != phrases.end()) ||
                                    (!prevWord.empty() && phrases.find(prevWord + L" " + lowerPart) != phrases.end());
            int64_t bestLongerScore = cands.empty() ? 0 : cands[0].count;

            if ((hasCuratedPhrase && exactScore > 0 && exactScore >= bestLongerScore / 2) ||
                (exactScore > 0 && exactScore > bestLongerScore * 3)) {
                std::wstring nextChain = buildNextWordChain(dict_, prevWord, lowerPart, 0, maxWords);
                if (!nextChain.empty()) {
                    std::wstring withSpace = L" " + nextChain;
                    if (isSuggestionSafe(withSpace)) {
                        res.text = withSpace;
                        res.confidence = 0.78;
                        res.wordCount = 1;
                        for (wchar_t c : nextChain) if (c == L' ') res.wordCount++;
                        res.latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - t0).count();
                        return res;
                    }
                }
            }
        }

        if (cands.empty()) return res;
        const auto& best = cands[0];
        if (best.term.size() <= partialWord.size()) return res;

        std::wstring suffix = best.term.substr(partialWord.size());
        if (isAllUpperW(partialWord)) {
            for (wchar_t& c : suffix) c = static_cast<wchar_t>(std::towupper(c));
        }

        double conf = 0.60;
        if (cands.size() >= 2 && cands[1].count > 0) {
            double ratio = static_cast<double>(best.count) / static_cast<double>(best.count + cands[1].count);
            conf = std::max(0.45, ratio);
        }
        if (!prevWord.empty() && dict_.getBigramFrequency(prevWord, best.term) > 0) {
            conf = std::max(conf, 0.75);
        }

        // Chain follow-up words after the completed word if multi-word prediction is enabled
        std::wstring chained = suffix;
        if (maxWords > 1) {
            std::wstring curWord = toLowerW(best.term);
            std::wstring follow = buildNextWordChain(dict_, prevWord, curWord, 0, maxWords);
            if (!follow.empty()) {
                chained += L" " + follow;
                chained = clampSuggestionWords(chained, maxWords + 1);
            }
        }

        if (chained.empty() || !isSuggestionSafe(chained)) return res;

        res.text = chained;
        res.confidence = conf;
        res.wordCount = 1;
        for (wchar_t c : res.text) if (c == L' ') res.wordCount++;
        res.latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - t0).count();
        return res;
    }

    // Case 2: Next-word prediction after a space (partialWord is empty)
    if (prevWord.empty()) return res;

    for (const auto& key : {prevThreeWords, prevTwoWords, prevWord}) {
        if (key.empty()) continue;
        auto it = phrases.find(key);
        if (it != phrases.end()) {
            std::wstring out = clampSuggestionWords(it->second, maxWords);
            if (isSentenceStart && !out.empty()) {
                out[0] = static_cast<wchar_t>(std::towupper(out[0]));
            }
            if (isSuggestionSafe(out)) {
                res.text = out;
                res.confidence = 0.85;
                res.wordCount = 1;
                for (wchar_t c : res.text) if (c == L' ') res.wordCount++;
                res.latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - t0).count();
                return res;
            }
        }
    }

    auto nexts = dict_.getNextWords(prevWord, 4);
    if (nexts.empty() || nexts[0].probability < 0.04) {
        return res;
    }

    std::wstring built = nexts[0].nextWord;
    if (isSentenceStart && !built.empty()) {
        built[0] = static_cast<wchar_t>(std::towupper(built[0]));
    }
    double conf = std::max(0.45, nexts[0].probability);
    std::wstring curWord = nexts[0].nextWord;

    std::wstring follow = buildNextWordChain(dict_, prevWord, curWord, 1, maxWords);
    if (!follow.empty()) {
        built += L" " + follow;
        built = clampSuggestionWords(built, maxWords);
    }

    if (!isSuggestionSafe(built)) return res;

    res.text = built;
    res.confidence = conf;
    res.wordCount = 1;
    for (wchar_t c : res.text) if (c == L' ') res.wordCount++;
    res.latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    return res;
}
