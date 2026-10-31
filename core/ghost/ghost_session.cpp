#include "core/ghost/ghost_session.h"
#include "core/predict/suggestion_filter.h"
#include <cwctype>

static bool isMatchingAcceptKeyCode(int64_t macKeyCode, KeyCode acceptKey) {
    if (acceptKey == KeyCode::RightArrow) {
        return macKeyCode == 124; // kVK_RightArrow
    }
    return macKeyCode == 48; // kVK_Tab
}

bool GhostSession::shouldSwallowKey(
    int64_t macKeyCode,
    uint64_t macFlags,
    bool isAutorepeat,
    bool isActiveTap,
    KeyCode acceptKey,
    int64_t nowNs
) const {
    if (!isActiveTap) return false;

    // Suppress held-down Tab autorepeat for 500 ms right after accepting a suggestion
    constexpr int64_t kAutorepeatGuardNs = 500LL * 1000000LL;
    int64_t lastAccept = lastAcceptTimeNs_.load(std::memory_order_relaxed);
    if (isAutorepeat && isMatchingAcceptKeyCode(macKeyCode, acceptKey) &&
        lastAccept > 0 && (nowNs - lastAccept) <= kAutorepeatGuardNs) {
        return true;
    }

    if (!visible_.load(std::memory_order_acquire)) return false;
    if (isAutorepeat) return false;

    // Check modifier flags: Shift (1<<17), Ctrl (1<<18), Option (1<<19), Cmd (1<<20)
    constexpr uint64_t kModMask = (1ULL << 17) | (1ULL << 18) | (1ULL << 19) | (1ULL << 20);
    if ((macFlags & kModMask) != 0) return false;

    return isMatchingAcceptKeyCode(macKeyCode, acceptKey);
}

bool GhostSession::consumeAccept(
    int acceptMode,
    int64_t nowNs,
    std::wstring& outChunkToInject,
    std::wstring& outRemainingVisible
) {
    std::lock_guard<std::mutex> lock(mutex_);
    outChunkToInject.clear();
    outRemainingVisible.clear();

    if (!visible_.load(std::memory_order_relaxed) || visibleText_.empty()) {
        visible_.store(false, std::memory_order_release);
        return false;
    }

    lastAcceptTimeNs_.store(nowNs, std::memory_order_relaxed);
    generation_.fetch_add(1, std::memory_order_acq_rel);

    std::wstring rem;
    outChunkToInject = extractAcceptChunk(visibleText_, acceptMode, rem);
    if (rem.empty()) {
        visibleText_.clear();
        visible_.store(false, std::memory_order_release);
    } else {
        visibleText_ = rem;
        outRemainingVisible = rem;
        visible_.store(true, std::memory_order_release);
    }
    return !outChunkToInject.empty();
}

bool GhostSession::onCharacterTyped(wchar_t ch, std::wstring& outRemaining) {
    std::lock_guard<std::mutex> lock(mutex_);
    outRemaining.clear();

    if (visible_.load(std::memory_order_relaxed) && !visibleText_.empty()) {
        wchar_t expected = visibleText_.front();
        if (std::towlower(expected) == std::towlower(ch)) {
            visibleText_.erase(visibleText_.begin());
            if (visibleText_.empty()) {
                visible_.store(false, std::memory_order_release);
                generation_.fetch_add(1, std::memory_order_acq_rel);
                return false;
            }
            outRemaining = visibleText_;
            return true;
        }
    }

    visibleText_.clear();
    visible_.store(false, std::memory_order_release);
    generation_.fetch_add(1, std::memory_order_acq_rel);
    return false;
}

void GhostSession::hide() {
    std::lock_guard<std::mutex> lock(mutex_);
    visibleText_.clear();
    visible_.store(false, std::memory_order_release);
    generation_.fetch_add(1, std::memory_order_acq_rel);
}

void GhostSession::markRejected(const std::wstring& contextKey, const std::wstring& suggestion) {
    if (suggestion.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (rejected_.size() >= 256) rejected_.clear();
    rejected_.insert(contextKey + L"||" + suggestion);
}

bool GhostSession::isRejected(const std::wstring& contextKey, const std::wstring& suggestion) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rejected_.count(contextKey + L"||" + suggestion) > 0;
}

void GhostSession::clearRejected() {
    std::lock_guard<std::mutex> lock(mutex_);
    rejected_.clear();
}

bool GhostSession::applyCandidatePrediction(
    uint64_t gen,
    const std::wstring& contextKey,
    const PredictionResult& pred,
    std::wstring& outTextToRender
) {
    outTextToRender.clear();
    if (pred.text.empty()) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    if (gen != generation_.load(std::memory_order_relaxed)) {
        return false; // Stale generation
    }
    if (rejected_.count(contextKey + L"||" + pred.text) > 0) {
        return false;
    }

    if (!visible_.load(std::memory_order_relaxed) || visibleText_.empty()) {
        visibleText_ = pred.text;
        visible_.store(true, std::memory_order_release);
        outTextToRender = visibleText_;
        return true;
    }

    // Something is already visible for this generation (e.g. instant N-gram layer).
    // Only upgrade if the new prediction extends the currently visible text prefix (no jarring swap).
    if (pred.text.size() > visibleText_.size() &&
        pred.text.compare(0, visibleText_.size(), visibleText_) == 0) {
        visibleText_ = pred.text;
        outTextToRender = visibleText_;
        return true;
    }

    return false;
}

std::wstring GhostSession::currentVisibleText() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return visibleText_;
}
