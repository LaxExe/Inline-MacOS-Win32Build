#include "core/tier1/symspell_dict.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static inline int min3(int a, int b, int c) {
    return std::min(std::min(a, b), c);
}

int damerauLevenshteinDistance(const std::wstring& s1, const std::wstring& s2) {
    size_t len1 = s1.size();
    size_t len2 = s2.size();

    if (len1 == 0) return static_cast<int>(len2);
    if (len2 == 0) return static_cast<int>(len1);

    if (s1 == s2) return 0;

    int d[32][32];
    if (len1 >= 32 || len2 >= 32) {
        std::vector<std::vector<int>> dyn(len1 + 1, std::vector<int>(len2 + 1));
        for (size_t i = 0; i <= len1; ++i) dyn[i][0] = static_cast<int>(i);
        for (size_t j = 0; j <= len2; ++j) dyn[0][j] = static_cast<int>(j);

        for (size_t i = 1; i <= len1; ++i) {
            for (size_t j = 1; j <= len2; ++j) {
                int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
                dyn[i][j] = min3(dyn[i - 1][j] + 1, dyn[i][j - 1] + 1, dyn[i - 1][j - 1] + cost);
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
            d[i][j] = min3(
                d[i - 1][j] + 1,
                d[i][j - 1] + 1,
                d[i - 1][j - 1] + cost
            );
            if (i > 1 && j > 1 && s1[i - 1] == s2[j - 2] && s1[i - 2] == s2[j - 1]) {
                d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);
            }
        }
    }
    return d[len1][len2];
}

static int damerauLevenshteinBounded(const std::wstring& s1, const std::wstring& s2, int maxDist) {
    size_t len1 = s1.size();
    size_t len2 = s2.size();
    if (std::abs(static_cast<int>(len1) - static_cast<int>(len2)) > maxDist) {
        return maxDist + 1;
    }

    // Strip common prefix
    size_t start = 0;
    while (start < len1 && start < len2 && s1[start] == s2[start]) {
        ++start;
    }

    // Strip common suffix
    while (len1 > start && len2 > start && s1[len1 - 1] == s2[len2 - 1]) {
        --len1;
        --len2;
    }

    const size_t rem1 = len1 - start;
    const size_t rem2 = len2 - start;

    if (rem1 == 0) return static_cast<int>(rem2);
    if (rem2 == 0) return static_cast<int>(rem1);
    if (rem1 == 1 && rem2 == 1) return 1; // single substitution
    if (rem1 == 2 && rem2 == 2 && s1[start] == s2[start + 1] && s1[start + 1] == s2[start]) return 1; // adjacent transposition

    if (rem1 >= 32 || rem2 >= 32) {
        return damerauLevenshteinDistance(s1.substr(start, rem1), s2.substr(start, rem2));
    }

    const wchar_t* p1 = s1.data() + start;
    const wchar_t* p2 = s2.data() + start;

    int d[32][32];
    for (size_t i = 0; i <= rem1; ++i) d[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= rem2; ++j) d[0][j] = static_cast<int>(j);

    for (size_t i = 1; i <= rem1; ++i) {
        int rowMin = maxDist + 1;
        const wchar_t c1 = p1[i - 1];
        for (size_t j = 1; j <= rem2; ++j) {
            const wchar_t c2 = p2[j - 1];
            int cost = (c1 == c2) ? 0 : 1;
            int cell = min3(
                d[i - 1][j] + 1,
                d[i][j - 1] + 1,
                d[i - 1][j - 1] + cost
            );
            if (i > 1 && j > 1 && c1 == p2[j - 2] && p1[i - 2] == c2) {
                cell = std::min(cell, d[i - 2][j - 2] + 1);
            }
            d[i][j] = cell;
            if (cell < rowMin) rowMin = cell;
        }
        if (rowMin > maxDist) {
            return maxDist + 1;
        }
    }
    return d[rem1][rem2];
}

SymSpellDict::SymSpellDict(int maxEditDistance, int prefixLength)
    : maxEditDistance_(maxEditDistance), prefixLength_(prefixLength) {
    wordList_.reserve(85000);
    words_.reserve(85000);
    deletes_.reserve(300000);
    seenStamp_.reserve(85000);
}

void SymSpellDict::createDeletes(uint32_t wordId) {
    const std::wstring& word = wordList_[wordId].first;
    std::wstring prefix = word.substr(0, std::min<size_t>(word.size(), prefixLength_));
    if (maxEditDistance_ <= 2) {
        deletes_[prefix].push_back(wordId);
        const size_t n = prefix.size();
        for (size_t i = 0; i < n; ++i) {
            if (i > 0 && prefix[i] == prefix[i - 1]) continue;
            std::wstring del1;
            del1.reserve(n - 1);
            del1.append(prefix, 0, i);
            del1.append(prefix, i + 1, n - i - 1);
            deletes_[del1].push_back(wordId);

            if (maxEditDistance_ >= 2 && !del1.empty()) {
                const size_t m = del1.size();
                for (size_t j = i; j < m; ++j) {
                    if (j > i && del1[j] == del1[j - 1]) continue;
                    std::wstring del2;
                    del2.reserve(m - 1);
                    del2.append(del1, 0, j);
                    del2.append(del1, j + 1, m - j - 1);
                    deletes_[del2].push_back(wordId);
                }
            }
        }
    } else {
        std::unordered_set<std::wstring> delSet;
        delSet.insert(prefix);
        getDeletes(prefix, maxEditDistance_, delSet);
        for (const auto& del : delSet) {
            deletes_[del].push_back(wordId);
        }
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
        if (count > wordList_[it->second].second) {
            wordList_[it->second].second = count;
        }
        return;
    }

    uint32_t wordId = static_cast<uint32_t>(wordList_.size());
    wordList_.push_back({word, count});
    seenStamp_.push_back(0);
    words_[word] = wordId;
    createDeletes(wordId);
}

bool SymSpellDict::hasWord(const std::wstring& word) const {
    return words_.find(word) != words_.end();
}

int64_t SymSpellDict::getFrequency(const std::wstring& word) const {
    auto it = words_.find(word);
    if (it != words_.end()) return wordList_[it->second].second;
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

        candidates.push_back((binDir.parent_path() / "Resources" / path).string());
        candidates.push_back((binDir.parent_path() / "Resources" / "dictionaries" / filename).string());
        candidates.push_back((binDir / "dictionaries" / filename).string());
        candidates.push_back((binDir / "Resources" / "dictionaries" / filename).string());
    }
#endif

    for (const auto& c : candidates) {
        if (!c.empty() && std::filesystem::exists(c)) {
            return c;
        }
    }
    return path;
}

static inline std::wstring utf8ToWstringFast(const char* s, size_t len) {
    std::wstring ws;
    ws.reserve(len);
    size_t i = 0;
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            ws.push_back(static_cast<wchar_t>(c));
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < len) {
                wchar_t wc = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
                ws.push_back(wc);
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < len) {
                wchar_t wc = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
                ws.push_back(wc);
            }
            i += 3;
        } else {
            i += 4;
        }
    }
    return ws;
}

bool SymSpellDict::loadDictionary(const std::string& corpusPath, int termIndex, int countIndex) {
    (void)termIndex;
    (void)countIndex;

    std::string resolved = resolveFilePath(corpusPath);
    std::ifstream file(resolved, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[SymSpellDict] Failed to open dictionary: " << corpusPath << " (resolved: " << resolved << ")\n";
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        const char* p = line.c_str();
        if (line.size() >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
            p += 3;
        }
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) continue;

        const char* termStart = p;
        while (*p && *p != ' ' && *p != '\t') ++p;
        size_t termLen = p - termStart;

        while (*p == ' ' || *p == '\t') ++p;
        int64_t count = 1;
        if (*p) {
            char* endp = nullptr;
            count = std::strtoll(p, &endp, 10);
            if (count <= 0) count = 1;
        }

        std::wstring wterm = utf8ToWstringFast(termStart, termLen);
        addWord(wterm, count);
    }

    return true;
}

bool SymSpellDict::loadBigramDictionary(const std::string& corpusPath) {
    std::string resolved = resolveFilePath(corpusPath);
    std::ifstream file(resolved, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[SymSpellDict] Failed to open bigram dictionary: " << corpusPath << " (resolved: " << resolved << ")\n";
        return false;
    }

    bigrams_.reserve(250000);

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        const char* p = line.c_str();
        if (line.size() >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
            p += 3;
        }
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) continue;

        size_t lastSpace = line.rfind(' ');
        if (lastSpace != std::string::npos && lastSpace > static_cast<size_t>(p - line.c_str())) {
            std::string phrase(p, lastSpace - static_cast<size_t>(p - line.c_str()));
            for (auto& c : phrase) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            int64_t count = std::strtoll(line.c_str() + lastSpace + 1, nullptr, 10);
            if (count <= 0) count = 1;
            bigrams_[phrase] = count;
        }
    }

    return true;
}

int64_t SymSpellDict::getBigramFrequency(const std::wstring& word1, const std::wstring& word2) const {
    if (bigrams_.empty() || word1.empty() || word2.empty()) return 0;
    std::string key;
    key.reserve(word1.size() + word2.size() + 1);
    for (wchar_t c : word1) {
        if (c < 128) key.push_back(static_cast<char>(std::towlower(c)));
    }
    key.push_back(' ');
    for (wchar_t c : word2) {
        if (c < 128) key.push_back(static_cast<char>(std::towlower(c)));
    }
    auto it = bigrams_.find(key);
    if (it != bigrams_.end()) return it->second;
    return 0;
}

void SymSpellDict::mergePersonalDictionary(const std::vector<std::pair<std::wstring, int64_t>>& personalEntries) {
    for (const auto& entry : personalEntries) {
        addWord(entry.first, entry.second);
    }
}

std::vector<Suggestion> SymSpellDict::lookup(const std::wstring& input, int maxEditDistance) const {
    std::vector<Suggestion> suggestions;
    if (input.empty()) return suggestions;
    suggestions.reserve(32);

    // 1. Exact match
    auto exactIt = words_.find(input);
    if (exactIt != words_.end()) {
        suggestions.push_back({input, 0, wordList_[exactIt->second].second});
    }

    if (maxEditDistance <= 0) return suggestions;

    // Pre-allocated stamp array avoids per-lookup std::unordered_set<uint32_t> heap allocations
    if (seenStamp_.size() < wordList_.size()) {
        seenStamp_.resize(wordList_.size(), 0);
    }
    if (++currentStamp_ == 0) {
        std::fill(seenStamp_.begin(), seenStamp_.end(), 0);
        currentStamp_ = 1;
    }
    const uint32_t stamp = currentStamp_;
    if (exactIt != words_.end()) {
        seenStamp_[exactIt->second] = stamp;
    }

    std::wstring prefix = input.substr(0, std::min<size_t>(input.size(), prefixLength_));
    thread_local std::vector<std::wstring> inputDeletes;
    inputDeletes.clear();
    inputDeletes.reserve(32);
    inputDeletes.push_back(prefix);

    if (maxEditDistance <= 2) {
        const size_t n = prefix.size();
        for (size_t i = 0; i < n; ++i) {
            if (i > 0 && prefix[i] == prefix[i - 1]) continue;
            std::wstring del1;
            del1.reserve(n - 1);
            del1.append(prefix, 0, i);
            del1.append(prefix, i + 1, n - i - 1);
            inputDeletes.push_back(del1);

            if (maxEditDistance >= 2 && !del1.empty()) {
                const size_t m = del1.size();
                for (size_t j = i; j < m; ++j) {
                    if (j > i && del1[j] == del1[j - 1]) continue;
                    std::wstring del2;
                    del2.reserve(m - 1);
                    del2.append(del1, 0, j);
                    del2.append(del1, j + 1, m - j - 1);
                    inputDeletes.push_back(del2);
                }
            }
        }
    } else {
        std::unordered_set<std::wstring> fallbackDeletes;
        fallbackDeletes.insert(prefix);
        getDeletes(prefix, maxEditDistance, fallbackDeletes);
        inputDeletes.assign(fallbackDeletes.begin(), fallbackDeletes.end());
    }

    for (const auto& del : inputDeletes) {
        auto delIt = deletes_.find(del);
        if (delIt != deletes_.end()) {
            for (uint32_t candId : delIt->second) {
                if (seenStamp_[candId] != stamp) {
                    seenStamp_[candId] = stamp;
                    const auto& candEntry = wordList_[candId];
                    const std::wstring& cand = candEntry.first;
                    if (std::abs(static_cast<int>(cand.size()) - static_cast<int>(input.size())) <= maxEditDistance) {
                        int dist = damerauLevenshteinBounded(input, cand, maxEditDistance);
                        if (dist <= maxEditDistance) {
                            suggestions.push_back({cand, dist, candEntry.second});
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
