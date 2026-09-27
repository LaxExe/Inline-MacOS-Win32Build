#include "core/tier1/symspell_dict.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static inline std::wstring utf8ToWstringFast(const char* str, size_t len) {
    std::wstring result;
    result.reserve(len);
    size_t i = 0;
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c < 0x80) {
            result.push_back(static_cast<wchar_t>(c));
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < len) {
                wchar_t wc = ((c & 0x1F) << 6) | (static_cast<unsigned char>(str[i + 1]) & 0x3F);
                result.push_back(wc);
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < len) {
                wchar_t wc = ((c & 0x0F) << 12) | ((static_cast<unsigned char>(str[i + 1]) & 0x3F) << 6) | (static_cast<unsigned char>(str[i + 2]) & 0x3F);
                result.push_back(wc);
            }
            i += 3;
        } else {
            i += 1;
        }
    }
    return result;
}

int damerauLevenshteinDistance(const std::wstring& s1, const std::wstring& s2) {
    size_t len1 = s1.size();
    size_t len2 = s2.size();
    if (len1 == 0) return static_cast<int>(len2);
    if (len2 == 0) return static_cast<int>(len1);

    int d[32][32];
    if (len1 >= 31 || len2 >= 31) {
        std::vector<std::vector<int>> dyn(len1 + 1, std::vector<int>(len2 + 1, 0));
        for (size_t i = 0; i <= len1; ++i) dyn[i][0] = static_cast<int>(i);
        for (size_t j = 0; j <= len2; ++j) dyn[0][j] = static_cast<int>(j);
        for (size_t i = 1; i <= len1; ++i) {
            for (size_t j = 1; j <= len2; ++j) {
                int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
                dyn[i][j] = std::min({ dyn[i - 1][j] + 1, dyn[i][j - 1] + 1, dyn[i - 1][j - 1] + cost });
                if (i > 1 && j > 1 && s1[i - 1] == s2[j - 2] && s1[i - 2] == s2[j - 1]) {
                    dyn[i][j] = std::min(dyn[i][j], dyn[i - 2][j - 2] + 1);
                }
            }
        }
        return dyn[len1][len2];
    }

    for (size_t i = 0; i <= len1; ++i) d[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= len2; ++j) d[0][j] = static_cast<int>(j);

    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            d[i][j] = std::min({
                d[i - 1][j] + 1,
                d[i][j - 1] + 1,
                d[i - 1][j - 1] + cost
            });
            if (i > 1 && j > 1 && s1[i - 1] == s2[j - 2] && s1[i - 2] == s2[j - 1]) {
                d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);
            }
        }
    }
    return d[len1][len2];
}

SymSpellDict::SymSpellDict(int maxEditDistance, int prefixLength)
    : maxEditDistance_(maxEditDistance), prefixLength_(prefixLength) {
    words_.reserve(90000);
    deletes_.reserve(350000);
    prefixMap_.reserve(100000);
}

void SymSpellDict::createDeletes(const std::wstring& word) {
    std::wstring prefix = word.substr(0, std::min<size_t>(word.size(), prefixLength_));
    std::unordered_set<std::wstring> delSet;
    delSet.insert(prefix);
    getDeletes(prefix, maxEditDistance_, delSet);
    for (const auto& del : delSet) {
        deletes_[del].push_back(word);
    }
}

void SymSpellDict::getDeletes(const std::wstring& word, int editDistance, std::unordered_set<std::wstring>& result) const {
    if (editDistance < 1 || word.empty()) return;

    for (size_t i = 0; i < word.size(); ++i) {
        std::wstring del = word.substr(0, i) + word.substr(i + 1);
        if (result.insert(del).second) {
            if (editDistance > 1) {
                getDeletes(del, editDistance - 1, result);
            }
        }
    }
}

void SymSpellDict::addWord(const std::wstring& word, int64_t count) {
    if (word.empty()) return;
    auto it = words_.find(word);
    if (it != words_.end()) {
        it->second = std::max(it->second, count);
    } else {
        words_[word] = count;
        if (count > maxCount_) maxCount_ = count;
        createDeletes(word);

        // Index prefixes for instant ghost text autocomplete
        for (size_t len = 2; len <= std::min<size_t>(word.size(), 8); ++len) {
            std::wstring pref = word.substr(0, len);
            auto& list = prefixMap_[pref];
            if (list.size() < 10) {
                list.push_back({word, count});
                std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
                    return a.second > b.second;
                });
            } else if (count > list.back().second) {
                list.back() = {word, count};
                std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
                    return a.second > b.second;
                });
            }
        }
    }
}

bool SymSpellDict::hasWord(const std::wstring& word) const {
    return words_.find(word) != words_.end();
}

int64_t SymSpellDict::getFrequency(const std::wstring& word) const {
    auto it = words_.find(word);
    if (it != words_.end()) return it->second;
    return 0;
}

int64_t SymSpellDict::getBigramFrequency(const std::wstring& w1, const std::wstring& w2) const {
    std::wstring phrase = w1 + L" " + w2;
    auto it = bigrams_.find(phrase);
    if (it != bigrams_.end()) return it->second;
    return 0;
}

static std::string resolveFilePath(const std::string& path) {
    if (path.empty()) return path;
    if (std::filesystem::exists(path)) {
        return path;
    }

    std::vector<std::string> candidates;
    candidates.push_back("./" + path);
    candidates.push_back("../" + path);
    candidates.push_back("../../" + path);
    candidates.push_back("../../../" + path);
    candidates.push_back("../Resources/" + path);
    candidates.push_back("Resources/" + path);

    std::string filename = std::filesystem::path(path).filename().string();
    candidates.push_back("dictionaries/" + filename);
    candidates.push_back("../dictionaries/" + filename);
    candidates.push_back("../../dictionaries/" + filename);

#ifdef __APPLE__
    char execPath[PATH_MAX] = {0};
    uint32_t size = sizeof(execPath);
    if (_NSGetExecutablePath(execPath, &size) == 0) {
        std::error_code ec;
        std::filesystem::path binPath = std::filesystem::canonical(execPath, ec);
        if (ec) {
            binPath = std::filesystem::path(execPath);
        }
        std::filesystem::path binDir = binPath.parent_path();

        // 1. App bundle Resources (e.g. TypingAssistant.app/Contents/Resources/...)
        candidates.push_back((binDir.parent_path() / "Resources" / path).string());
        candidates.push_back((binDir.parent_path() / "Resources" / "dictionaries" / filename).string());

        // 2. Relative to MacOS directory or build output directory
        candidates.push_back((binDir / path).string());
        candidates.push_back((binDir / ".." / path).string());
        candidates.push_back((binDir / ".." / ".." / path).string());
        candidates.push_back((binDir / ".." / ".." / ".." / path).string());
        candidates.push_back((binDir / ".." / ".." / ".." / "dictionaries" / filename).string());
        candidates.push_back((binDir / "dictionaries" / filename).string());
    }

    candidates.push_back("/Applications/TypingAssistant.app/Contents/Resources/" + path);
    candidates.push_back("/Applications/TypingAssistant.app/Contents/Resources/dictionaries/" + filename);
#endif

    for (const auto& c : candidates) {
        if (!c.empty() && std::filesystem::exists(c)) {
            return c;
        }
    }
    return path;
}

bool SymSpellDict::loadDictionary(const std::string& corpusPath, int termIndex, int countIndex) {
    std::string resolved = resolveFilePath(corpusPath);
    std::ifstream file(resolved);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        const char* p = line.c_str();
        if (line.size() >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
            p += 3;
        }
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) continue;

        const char* wordStart = p;
        while (*p && *p != ' ' && *p != '\t') ++p;
        size_t wordLen = p - wordStart;

        while (*p == ' ' || *p == '\t') ++p;
        int64_t count = 1;
        if (*p) {
            char* endp = nullptr;
            count = std::strtoll(p, &endp, 10);
            if (count <= 0) count = 1;
        }

        std::wstring term = utf8ToWstringFast(wordStart, wordLen);
        addWord(term, count);
    }
    return true;
}

bool SymSpellDict::loadBigramDictionary(const std::string& corpusPath, int termIndex, int countIndex) {
    std::string resolved = resolveFilePath(corpusPath);
    std::ifstream file(resolved);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        const char* p = line.c_str();
        if (line.size() >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
            p += 3;
        }
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) continue;

        const char* p1 = p;
        while (*p && *p != ' ' && *p != '\t') ++p;
        size_t len1 = p - p1;

        while (*p == ' ' || *p == '\t') ++p;
        const char* p2 = p;
        while (*p && *p != ' ' && *p != '\t') ++p;
        size_t len2 = p - p2;

        while (*p == ' ' || *p == '\t') ++p;
        int64_t count = 1;
        if (*p) {
            char* endp = nullptr;
            count = std::strtoll(p, &endp, 10);
            if (count <= 0) count = 1;
        }

        std::wstring w1 = utf8ToWstringFast(p1, len1);
        std::wstring w2 = utf8ToWstringFast(p2, len2);
        std::wstring term = w1 + L" " + w2;
        bigrams_[term] = count;

        std::wstring lowerW1 = w1;
        for (auto& c : lowerW1) c = std::towlower(c);
        auto it = bigramBestNext_.find(lowerW1);
        if (it == bigramBestNext_.end() || count > it->second.second) {
            bigramBestNext_[lowerW1] = {w2, count};
        }
    }
    return true;
}

void SymSpellDict::mergePersonalDictionary(const std::vector<std::pair<std::wstring, int64_t>>& personalEntries) {
    for (const auto& entry : personalEntries) {
        addWord(entry.first, entry.second);
    }
}

std::vector<Suggestion> SymSpellDict::lookup(const std::wstring& input, int maxEditDistance) const {
    std::vector<Suggestion> suggestions;
    if (input.empty()) return suggestions;

    // 1. Exact match
    auto exactIt = words_.find(input);
    if (exactIt != words_.end()) {
        suggestions.push_back({input, 0, exactIt->second});
    }

    if (maxEditDistance == 0) return suggestions;

    std::unordered_set<std::wstring> consideredCandidates;
    if (exactIt != words_.end()) {
        consideredCandidates.insert(input);
    }

    std::wstring prefix = input.substr(0, std::min<size_t>(input.size(), prefixLength_));
    std::unordered_set<std::wstring> inputDeletes;
    inputDeletes.insert(prefix);
    getDeletes(prefix, maxEditDistance, inputDeletes);

    for (const auto& del : inputDeletes) {
        auto delIt = deletes_.find(del);
        if (delIt != deletes_.end()) {
            for (const auto& cand : delIt->second) {
                if (consideredCandidates.insert(cand).second) {
                    if (std::abs(static_cast<int>(cand.size()) - static_cast<int>(input.size())) <= maxEditDistance) {
                        int dist = damerauLevenshteinDistance(input, cand);
                        if (dist <= maxEditDistance) {
                            suggestions.push_back({cand, dist, getFrequency(cand)});
                        }
                    }
                }
            }
        }
    }

    std::sort(suggestions.begin(), suggestions.end(), [](const Suggestion& a, const Suggestion& b) {
        if (a.distance != b.distance) return a.distance < b.distance;
        return a.count > b.count;
    });

    return suggestions;
}

std::vector<Suggestion> SymSpellDict::lookupPrefix(const std::wstring& prefix, size_t maxResults) const {
    std::vector<Suggestion> result;
    if (prefix.size() < 2) return result;

    std::wstring lowerPref = prefix;
    for (auto& c : lowerPref) c = std::towlower(c);

    auto it = prefixMap_.find(lowerPref);
    if (it != prefixMap_.end()) {
        for (const auto& item : it->second) {
            result.push_back({item.first, 0, item.second});
            if (result.size() >= maxResults) break;
        }
    }
    return result;
}

std::optional<std::wstring> SymSpellDict::lookupBigramNext(const std::wstring& prevWord) const {
    if (prevWord.empty()) return std::nullopt;

    std::wstring lowerPrev = prevWord;
    for (auto& c : lowerPrev) c = std::towlower(c);

    auto it = bigramBestNext_.find(lowerPrev);
    if (it != bigramBestNext_.end() && !it->second.first.empty()) {
        return it->second.first;
    }

    // High-frequency natural language phrase fallbacks
    static const std::unordered_map<std::wstring, std::wstring> kCommonPhrases = {
        {L"thank", L"you"},
        {L"thanks", L"for"},
        {L"how", L"are"},
        {L"good", L"morning"},
        {L"nice", L"to"},
        {L"feel", L"free"},
        {L"take", L"care"},
        {L"welcome", L"to"},
        {L"see", L"you"},
        {L"let", L"me"},
        {L"please", L"let"},
        {L"i'm", L"going"},
        {L"i", L"think"},
        {L"you", L"can"},
        {L"we", L"can"},
        {L"they", L"are"},
        {L"it", L"is"},
        {L"this", L"is"},
        {L"that", L"is"},
        {L"what", L"is"},
        {L"where", L"is"},
        {L"have", L"a"},
        {L"look", L"forward"},
        {L"best", L"regards"}
    };

    auto phraseIt = kCommonPhrases.find(lowerPrev);
    if (phraseIt != kCommonPhrases.end()) {
        return phraseIt->second;
    }

    return std::nullopt;
}
