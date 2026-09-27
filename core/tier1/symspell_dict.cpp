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
    wordList_.reserve(85000);
    words_.reserve(85000);
    deletes_.reserve(300000);
}

void SymSpellDict::createDeletes(uint32_t wordId) {
    const std::wstring& word = wordList_[wordId].first;
    std::wstring prefix = word.substr(0, std::min<size_t>(word.size(), prefixLength_));
    std::unordered_set<std::wstring> delSet;
    delSet.insert(prefix);
    getDeletes(prefix, maxEditDistance_, delSet);
    for (const auto& del : delSet) {
        deletes_[del].push_back(wordId);
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
        suggestions.push_back({input, 0, wordList_[exactIt->second].second});
    }

    if (maxEditDistance == 0) return suggestions;

    std::unordered_set<uint32_t> consideredIds;
    if (exactIt != words_.end()) {
        consideredIds.insert(exactIt->second);
    }

    std::wstring prefix = input.substr(0, std::min<size_t>(input.size(), prefixLength_));
    std::unordered_set<std::wstring> inputDeletes;
    inputDeletes.insert(prefix);
    getDeletes(prefix, maxEditDistance, inputDeletes);

    for (const auto& del : inputDeletes) {
        auto delIt = deletes_.find(del);
        if (delIt != deletes_.end()) {
            for (uint32_t candId : delIt->second) {
                if (consideredIds.insert(candId).second) {
                    const auto& candEntry = wordList_[candId];
                    const std::wstring& cand = candEntry.first;
                    if (std::abs(static_cast<int>(cand.size()) - static_cast<int>(input.size())) <= maxEditDistance) {
                        int dist = damerauLevenshteinDistance(input, cand);
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
