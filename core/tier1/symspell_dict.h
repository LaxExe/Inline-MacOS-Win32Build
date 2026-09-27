#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <cstdint>

struct Suggestion {
    std::wstring term;
    int distance = 0;
    int64_t count = 0;
};

class SymSpellDict {
public:
    SymSpellDict(int maxEditDistance = 2, int prefixLength = 7);

    bool loadDictionary(const std::string& corpusPath, int termIndex = 0, int countIndex = 1);
    bool loadBigramDictionary(const std::string& corpusPath, int termIndex = 0, int countIndex = 2);
    void mergePersonalDictionary(const std::vector<std::pair<std::wstring, int64_t>>& personalEntries);
    void addWord(const std::wstring& word, int64_t count);

    std::vector<Suggestion> lookup(const std::wstring& input, int maxEditDistance) const;
    std::vector<Suggestion> lookupPrefix(const std::wstring& prefix, size_t maxResults = 5) const;
    std::optional<std::wstring> lookupBigramNext(const std::wstring& prevWord) const;
    bool hasWord(const std::wstring& word) const;
    int64_t getFrequency(const std::wstring& word) const;
    int64_t getBigramFrequency(const std::wstring& w1, const std::wstring& w2) const;

private:
    int maxEditDistance_;
    int prefixLength_;
    int64_t maxCount_ = 0;

    std::unordered_map<std::wstring, int64_t> words_;
    std::unordered_map<std::wstring, std::vector<std::wstring>> deletes_;
    std::unordered_map<std::wstring, int64_t> bigrams_;
    std::unordered_map<std::wstring, std::pair<std::wstring, int64_t>> bigramBestNext_;
    std::unordered_map<std::wstring, std::vector<std::pair<std::wstring, int64_t>>> prefixMap_;

    void createDeletes(const std::wstring& word);
    void getDeletes(const std::wstring& word, int editDistance, std::unordered_set<std::wstring>& result) const;
};

int damerauLevenshteinDistance(const std::wstring& s1, const std::wstring& s2);
