#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <atomic>
#include <mutex>
#include <cstdint>
#include "core/config/app_config.h"
#include "core/ghost/ghost_layout.h"
#include "core/predict/ngram_predictor.h"

class GhostSession {
public:
    GhostSession() = default;

    // Lock-free / fast hook-thread queries
    bool isVisible() const { return visible_.load(std::memory_order_acquire); }
    uint64_t currentGeneration() const { return generation_.load(std::memory_order_acquire); }
    uint64_t bumpGeneration() { return generation_.fetch_add(1, std::memory_order_acq_rel) + 1; }

    // Checks whether a physical keyDown should be swallowed (Tab / Right Arrow accept or post-accept Tab repeat)
    bool shouldSwallowKey(
        int64_t macKeyCode,
        uint64_t macFlags,
        bool isAutorepeat,
        bool isActiveTap,
        KeyCode acceptKey,
        int64_t nowNs
    ) const;

    // Called on the hook thread when shouldSwallowKey returns true for an accept key.
    // Populates outChunkToInject with the accepted text (whole suggestion or 1 word).
    // Returns true if a non-empty chunk was consumed.
    bool consumeAccept(
        int acceptMode,
        int64_t nowNs,
        std::wstring& outChunkToInject,
        std::wstring& outRemainingVisible
    );

    // Called when the user types a normal character or space.
    // If the character matches the first character of the visible ghost text (type-through),
    // consumes that character and returns true with outRemaining populated.
    // Otherwise hides ghost text, bumps generation, and returns false.
    bool onCharacterTyped(wchar_t ch, std::wstring& outRemaining);

    // Immediately hides any visible ghost text and invalidates in-flight predictions
    void hide();

    // Records a rejected suggestion (e.g. after Backspace undo of a ghost accept)
    void markRejected(const std::wstring& contextKey, const std::wstring& suggestion);
    bool isRejected(const std::wstring& contextKey, const std::wstring& suggestion) const;
    void clearRejected();

    // Called by the worker thread when a layout + prediction is ready for `gen`.
    // Applies the Section 7.4 merge rule (instant shows first; LLM only shows if hidden or extends prefix).
    // Returns true if the overlay should be updated with `outTextToRender`.
    bool applyCandidatePrediction(
        uint64_t gen,
        const std::wstring& contextKey,
        const PredictionResult& pred,
        std::wstring& outTextToRender
    );

    std::wstring currentVisibleText() const;

private:
    mutable std::mutex mutex_;
    std::atomic<bool> visible_{false};
    std::atomic<uint64_t> generation_{1};
    std::atomic<int64_t> lastAcceptTimeNs_{0};
    std::wstring visibleText_;
    std::unordered_set<std::wstring> rejected_;
};
