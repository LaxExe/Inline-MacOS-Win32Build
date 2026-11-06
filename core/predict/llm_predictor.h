#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>
#include "core/predict/ngram_predictor.h"

struct llama_model;
struct llama_context;
struct llama_vocab;

class LlmPredictor {
public:
    LlmPredictor();
    ~LlmPredictor();

    bool loadModel(const std::string& ggufPath, int numThreads = 2);
    void unload();
    bool isLoaded() const;
    std::string modelName() const { return modelName_; }

    PredictionResult predict(
        const std::string& contextBeforePartialUTF8,
        const std::string& partialWordUTF8,
        int maxWords = 3,
        float minFirstWordProb = 0.06f
    );

    static std::string wstringToUtf8(const std::wstring& ws);
    static std::wstring utf8ToWstring(const std::string& s);

private:
    mutable std::mutex mutex_;
    llama_model* model_ = nullptr;
    llama_context* ctx_ = nullptr;
    const llama_vocab* vocab_ = nullptr;
    int numThreads_ = 2;
    std::string modelName_;
    std::vector<int32_t> cachedTokens_;
};
