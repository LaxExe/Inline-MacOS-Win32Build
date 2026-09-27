#include "core/tier2/diff_safety.h"

bool exceedsChangeCeiling(const std::vector<WordDiff>& diff, size_t totalWords, float ceilingFraction) {
    if (totalWords == 0) return false;
    float fraction = static_cast<float>(diff.size()) / static_cast<float>(totalWords);
    // Use an epsilon to avoid floating point precision issues on exact boundary comparison
    constexpr float kEps = 1e-6f;
    return fraction > (ceilingFraction + kEps);
}
