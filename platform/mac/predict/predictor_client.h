#pragma once

#include <string>
#include <deque>
#include <mutex>
#include <atomic>
#include <cstdint>
#include "core/predict/ngram_predictor.h"

class PredictorClient {
public:
    PredictorClient();
    ~PredictorClient();

    // Synchronous call from the dedicated ghost worker thread (never called on hook thread)
    PredictionResult predict(
        uint64_t generation,
        const std::wstring& contextBeforePartial,
        const std::wstring& partialWord,
        int maxWords
    );

    void shutdownHelper();

    bool isModelAvailable() const { return modelAvailable_.load(std::memory_order_relaxed); }
    bool isHelperRunning() const { return childPid_.load(std::memory_order_relaxed) > 0; }
    bool isCircuitTripped() const { return circuitTripped_.load(std::memory_order_relaxed); }
    int64_t lastLatencyMs() const { return lastLatencyMs_.load(std::memory_order_relaxed); }
    std::string modelName() const { return modelName_; }

private:
    mutable std::mutex mutex_;
    std::string helperPath_;
    std::string modelPath_;
    std::string modelName_ = "SmolLM2-135M (Q8_0)";

    std::atomic<bool> modelAvailable_{false};
    std::atomic<bool> circuitTripped_{false};
    std::atomic<int> childPid_{0};
    std::atomic<int64_t> lastLatencyMs_{15};

    int toChildFd_ = -1;
    int fromChildFd_ = -1;
    std::deque<int64_t> recentCrashTimestampsMs_;

    void resolvePaths();
    bool ensureHelperStartedLocked();
    void closeChildLocked(bool recordCrash);
};
