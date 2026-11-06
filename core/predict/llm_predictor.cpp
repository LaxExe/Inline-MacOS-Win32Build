#include "core/predict/llm_predictor.h"
#include "core/predict/token_healing.h"
#include "core/predict/suggestion_filter.h"
#include <chrono>
#include <cmath>
#include <algorithm>
#include <filesystem>

#if defined(INLINE_ENABLE_LLAMA)
#include "llama.h"
#endif

std::string LlmPredictor::wstringToUtf8(const std::wstring& ws) {
    std::string out;
    out.reserve(ws.size());
    for (wchar_t wc : ws) {
        uint32_t cp = static_cast<uint32_t>(wc);
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

std::wstring LlmPredictor::utf8ToWstring(const std::string& s) {
    std::wstring ws;
    ws.reserve(s.size());
    size_t i = 0;
    const size_t len = s.size();
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            ws.push_back(static_cast<wchar_t>(c));
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < len) {
            wchar_t wc = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
            ws.push_back(wc);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < len) {
            wchar_t wc = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
            ws.push_back(wc);
            i += 3;
        } else {
            i += 4;
        }
    }
    return ws;
}

#if defined(INLINE_ENABLE_LLAMA)
static void silentLlamaLog(enum ggml_log_level level, const char* text, void* user_data) {
    (void)level;
    (void)text;
    (void)user_data;
}
#endif

LlmPredictor::LlmPredictor() {
#if defined(INLINE_ENABLE_LLAMA)
    static std::once_flag initFlag;
    std::call_once(initFlag, []() {
        llama_log_set(silentLlamaLog, nullptr);
        llama_backend_init();
    });
#endif
}

LlmPredictor::~LlmPredictor() {
    unload();
}

bool LlmPredictor::loadModel(const std::string& ggufPath, int numThreads) {
    std::lock_guard<std::mutex> lock(mutex_);
#if defined(INLINE_ENABLE_LLAMA)
    if (ctx_) {
        llama_free(ctx_);
        ctx_ = nullptr;
    }
    if (model_) {
        llama_model_free(model_);
        model_ = nullptr;
    }
    vocab_ = nullptr;
    cachedTokens_.clear();
    numThreads_ = std::max(1, numThreads);

    if (ggufPath.empty() || !std::filesystem::exists(ggufPath)) {
        return false;
    }

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = 0; // CPU-only so GPU stays completely idle
    mparams.load_mode = LLAMA_LOAD_MODE_MMAP;

    model_ = llama_model_load_from_file(ggufPath.c_str(), mparams);
    if (!model_) {
        return false;
    }

    vocab_ = llama_model_get_vocab(model_);

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx = 128;
    cparams.n_batch = 64;
    cparams.n_ubatch = 64;
    cparams.n_threads = numThreads_;
    cparams.n_threads_batch = numThreads_;
    cparams.offload_kqv = false;
    cparams.no_perf = true;

    ctx_ = llama_init_from_model(model_, cparams);
    if (!ctx_) {
        llama_model_free(model_);
        model_ = nullptr;
        vocab_ = nullptr;
        return false;
    }

    modelName_ = std::filesystem::path(ggufPath).filename().string();
    return true;
#else
    (void)ggufPath;
    (void)numThreads;
    return false;
#endif
}

void LlmPredictor::unload() {
    std::lock_guard<std::mutex> lock(mutex_);
#if defined(INLINE_ENABLE_LLAMA)
    if (ctx_) {
        llama_free(ctx_);
        ctx_ = nullptr;
    }
    if (model_) {
        llama_model_free(model_);
        model_ = nullptr;
    }
    vocab_ = nullptr;
    cachedTokens_.clear();
#endif
}

bool LlmPredictor::isLoaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return model_ != nullptr && ctx_ != nullptr;
}

PredictionResult LlmPredictor::predict(
    const std::string& contextBeforePartialUTF8,
    const std::string& partialWordUTF8,
    int maxWords,
    float minFirstWordProb
) {
    PredictionResult result;
    result.fromLLM = true;

#if defined(INLINE_ENABLE_LLAMA)
    std::lock_guard<std::mutex> lock(mutex_);
    if (!model_ || !ctx_ || !vocab_ || maxWords <= 0) return result;

    auto t0 = std::chrono::steady_clock::now();

    HealedPrompt healed = splitForTokenHealing(contextBeforePartialUTF8, partialWordUTF8);
    if (healed.contextWithoutPartial.empty() && healed.requiredTokenPrefix.empty()) {
        return result;
    }

    std::string prompt = healed.contextWithoutPartial.empty() ? " " : healed.contextWithoutPartial;
    std::vector<llama_token> tokens(prompt.size() + 8);
    int32_t nTokens = llama_tokenize(
        vocab_,
        prompt.c_str(),
        static_cast<int32_t>(prompt.size()),
        tokens.data(),
        static_cast<int32_t>(tokens.size()),
        true,
        false
    );
    if (nTokens <= 0) return result;
    tokens.resize(nTokens);

    // Keep at most last 80 context tokens so KV cache has headroom for generation
    constexpr size_t kMaxPromptTokens = 80;
    if (tokens.size() > kMaxPromptTokens) {
        tokens.erase(tokens.begin(), tokens.end() - kMaxPromptTokens);
    }

    // Compute shared prefix with cachedTokens_ for incremental KV-cache reuse
    size_t commonPrefix = 0;
    while (commonPrefix < cachedTokens_.size() &&
           commonPrefix < tokens.size() &&
           cachedTokens_[commonPrefix] == tokens[commonPrefix]) {
        ++commonPrefix;
    }

    // Always decode at least the last prompt token so we have fresh logits
    if (commonPrefix == tokens.size() && commonPrefix > 0) {
        --commonPrefix;
    }

    llama_memory_t mem = llama_get_memory(ctx_);
    if (commonPrefix == 0) {
        llama_memory_clear(mem, false);
    } else {
        llama_memory_seq_rm(mem, 0, static_cast<llama_pos>(commonPrefix), -1);
    }
    cachedTokens_.assign(tokens.begin(), tokens.begin() + commonPrefix);

    int32_t nEval = static_cast<int32_t>(tokens.size() - commonPrefix);
    llama_batch batch = llama_batch_init(std::max<int32_t>(nEval, 16), 0, 1);
    batch.n_tokens = nEval;
    for (int32_t i = 0; i < nEval; ++i) {
        batch.token[i] = tokens[commonPrefix + i];
        batch.pos[i] = static_cast<llama_pos>(commonPrefix + i);
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0] = 0;
        batch.logits[i] = (i == nEval - 1) ? 1 : 0;
    }

    if (llama_decode(ctx_, batch) != 0) {
        llama_batch_free(batch);
        cachedTokens_.clear();
        return result;
    }
    cachedTokens_ = tokens;

    const int32_t nVocab = llama_vocab_n_tokens(vocab_);
    std::string generated;
    int wordsCompleted = 0;
    double firstTokenProb = 0.0;
    const int maxSteps = std::min(10, maxWords * 3 + 2);

    for (int step = 0; step < maxSteps; ++step) {
        const float* logits = llama_get_logits_ith(ctx_, -1);
        if (!logits) break;

        const bool needPrefixMatch = (generated.size() < healed.requiredTokenPrefix.size());

        // Find max logit for numerical stability + pick best matching token
        float maxLogit = -1e30f;
        llama_token bestToken = -1;
        float bestLogit = -1e30f;

        for (int32_t tid = 0; tid < nVocab; ++tid) {
            float l = logits[tid];
            if (l > maxLogit) maxLogit = l;
            if (!needPrefixMatch) {
                if (l > bestLogit && !llama_vocab_is_control(vocab_, tid)) {
                    bestLogit = l;
                    bestToken = tid;
                }
            }
        }

        if (needPrefixMatch) {
            // Scan top logits that satisfy token-healing prefix constraint
            char pieceBuf[64];
            for (int32_t tid = 0; tid < nVocab; ++tid) {
                float l = logits[tid];
                if (l <= bestLogit || l < maxLogit - 12.0f) continue;
                if (llama_vocab_is_eog(vocab_, tid) || llama_vocab_is_control(vocab_, tid)) continue;
                int32_t plen = llama_token_to_piece(vocab_, tid, pieceBuf, sizeof(pieceBuf), 0, false);
                if (plen <= 0) continue;
                std::string candidate = generated + std::string(pieceBuf, plen);
                if (matchesTokenHealingPrefix(candidate, healed.requiredTokenPrefix)) {
                    bestLogit = l;
                    bestToken = tid;
                }
            }
        }

        if (bestToken < 0 || llama_vocab_is_eog(vocab_, bestToken)) break;

        // Compute probability of bestToken via softmax denominator
        double sumExp = 0.0;
        for (int32_t tid = 0; tid < nVocab; ++tid) {
            float diff = logits[tid] - maxLogit;
            if (diff > -20.0f) {
                sumExp += std::exp(static_cast<double>(diff));
            }
        }
        double prob = (sumExp > 0.0) ? (std::exp(static_cast<double>(bestLogit - maxLogit)) / sumExp) : 0.0;

        char pieceBuf[64];
        int32_t plen = llama_token_to_piece(vocab_, bestToken, pieceBuf, sizeof(pieceBuf), 0, false);
        if (plen <= 0) break;
        std::string piece(pieceBuf, plen);

        // Stop before newline or control characters
        size_t nlPos = piece.find_first_of("\r\n\t");
        if (nlPos != std::string::npos) {
            piece = piece.substr(0, nlPos);
            generated += piece;
            break;
        }

        // Check if this token extends beyond the healed prefix
        bool crossesPrefix = (generated.size() + piece.size() > healed.requiredTokenPrefix.size());
        if (crossesPrefix) {
            if (firstTokenProb == 0.0) {
                firstTokenProb = prob;
                if (prob < static_cast<double>(minFirstWordProb)) {
                    break;
                }
            } else if (!piece.empty() && piece[0] == ' ' && prob < 0.05) {
                break;
            }
        }

        generated += piece;

        // Count completed words in the suffix beyond requiredTokenPrefix
        if (generated.size() > healed.requiredTokenPrefix.size()) {
            std::string suffix = generated.substr(healed.requiredTokenPrefix.size());
            int spaceTransitions = 0;
            bool inW = false;
            for (char c : suffix) {
                if (c != ' ' && !inW) {
                    inW = true;
                    spaceTransitions++;
                } else if (c == ' ') {
                    inW = false;
                }
            }
            wordsCompleted = spaceTransitions;
            if (wordsCompleted > maxWords) {
                // Trim back to the last space before the overflow word
                size_t lastSp = suffix.rfind(' ');
                if (lastSp != std::string::npos) {
                    generated = healed.requiredTokenPrefix + suffix.substr(0, lastSp);
                }
                break;
            }
        }

        if (!piece.empty() && (piece.back() == '.' || piece.back() == '!' || piece.back() == '?')) {
            break;
        }

        // Advance KV cache by 1 token
        batch.n_tokens = 1;
        batch.token[0] = bestToken;
        batch.pos[0] = static_cast<llama_pos>(cachedTokens_.size());
        batch.n_seq_id[0] = 1;
        batch.seq_id[0][0] = 0;
        batch.logits[0] = 1;
        if (llama_decode(ctx_, batch) != 0) break;
        cachedTokens_.push_back(bestToken);
    }

    llama_batch_free(batch);

    if (generated.size() <= healed.requiredTokenPrefix.size()) {
        return result;
    }

    std::string suffixUTF8 = generated.substr(healed.requiredTokenPrefix.size());
    // If context ended with a space and partialWord was empty, strip any leading space from suffixUTF8
    if (partialWordUTF8.empty() && !contextBeforePartialUTF8.empty() &&
        contextBeforePartialUTF8.back() == ' ' && !suffixUTF8.empty() && suffixUTF8.front() == ' ') {
        suffixUTF8.erase(suffixUTF8.begin());
    }

    std::wstring suffixW = clampSuggestionWords(utf8ToWstring(suffixUTF8), maxWords);
    std::wstring ctxW = utf8ToWstring(contextBeforePartialUTF8);
    if (suffixW.size() < 2 || !isSuggestionSafe(suffixW, ctxW)) {
        return result;
    }

    result.text = std::move(suffixW);
    result.confidence = (firstTokenProb > 0.0) ? firstTokenProb : 0.45;
    result.wordCount = 1;
    for (wchar_t c : result.text) if (c == L' ') result.wordCount++;
    result.latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
#else
    (void)contextBeforePartialUTF8;
    (void)partialWordUTF8;
    (void)maxWords;
    (void)minFirstWordProb;
#endif
    return result;
}
