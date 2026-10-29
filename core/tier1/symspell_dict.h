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
    bool loadBigramDictionary(const std::string& corpusPath);
    void mergePersonalDictionary(const std::vector<std::pair<std::wstring, int64_t>>& personalEntries);
    void addWord(const std::wstring& word, int64_t count);

    std::vector<Suggestion> lookup(const std::wstring& input, int maxEditDistance) const;
    bool hasWord(const std::wstring& word) const;
    int64_t getFrequency(const std::wstring& word) const;
    int64_t getBigramFrequency(const std::wstring& word1, const std::wstring& word2) const;

    size_t wordCount() const { return words_.size(); }
    size_t bigramCount() const { return bigrams_.size(); }

private:
    int maxEditDistance_;
    int prefixLength_;

    std::vector<std::pair<std::wstring, int64_t>> wordList_;
    std::unordered_map<std::wstring, uint32_t> words_;
    std::unordered_map<std::wstring, std::vector<uint32_t>> deletes_;
    std::unordered_map<std::string, int64_t> bigrams_;

    mutable std::vector<uint32_t> seenStamp_;
    mutable uint32_t currentStamp_ = 0;

    void createDeletes(uint32_t wordId);
    void getDeletes(const std::wstring& word, int editDistance, std::unordered_set<std::wstring>& result) const;
};

int damerauLevenshteinDistance(const std::wstring& s1, const std::wstring& s2);
