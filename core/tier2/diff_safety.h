#pragma once

#include <vector>
#include <string>
#include <cstddef>

struct WordDiff {
    size_t index = 0;
    std::wstring original;
    std::wstring replacement;
};

bool exceedsChangeCeiling(const std::vector<WordDiff>& diff, size_t totalWords, float ceilingFraction);
