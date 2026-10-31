#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <filesystem>
#include "core/tier1/symspell_dict.h"
#include "core/predict/ngram_predictor.h"
#include "core/predict/llm_predictor.h"

struct BenchSample {
    std::string context;
    std::string partial;
};

static double percentileMs(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t idx = std::min<size_t>(v.size() - 1, static_cast<size_t>(p * v.size()));
    return v[idx];
}

int main() {
    std::cout << "=== Inline Ghost Text Predictor Benchmark (macOS Apple Silicon) ===\n\n";

    SymSpellDict dict;
    dict.loadDictionary("dictionaries/frequency_dictionary_en_82_765.txt", 0, 1);
    dict.loadBigramDictionary("dictionaries/frequency_bigramdictionary_en_243_342.txt");

    NgramPredictor ngram(dict);

    std::vector<BenchSample> samples = {
        {"Hello, I wanted ", ""},
        {"Hello, I wanted to ", "foll"},
        {"Hello, I wanted to follow ", ""},
        {"Hello, I wanted to follow up ", "on"},
        {"Thank you ", ""},
        {"Please let me ", "kn"},
        {"Looking forward ", ""},
        {"Looking forward to ", "hear"},
        {"Can we schedule a ", "meet"},
        {"Sounds good ", ""},
        {"I hope you are ", "doi"},
        {"Let me know if you ", ""}
    };

    // 1. Benchmark Instant N-gram Layer
    std::vector<double> ngramTimesMs;
    for (int iter = 0; iter < 50; ++iter) {
        for (const auto& s : samples) {
            std::vector<std::wstring> words = {L"wanted", L"to"};
            std::wstring part = LlmPredictor::utf8ToWstring(s.partial);
            auto t0 = std::chrono::steady_clock::now();
            auto res = ngram.predict(words, part, AppConfig::PredictionLength::Balanced, false);
            auto t1 = std::chrono::steady_clock::now();
            (void)res;
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            ngramTimesMs.push_back(ms);
        }
    }
    std::cout << "[L0 Instant N-Gram Predictor]\n"
              << "  p50 latency: " << std::fixed << std::setprecision(4) << percentileMs(ngramTimesMs, 0.50) << " ms\n"
              << "  p95 latency: " << percentileMs(ngramTimesMs, 0.95) << " ms\n\n";

    // 2. Benchmark LLM candidates on 2 CPU threads with KV-cache prefix reuse
    struct ModelCandidate {
        std::string label;
        std::string path;
    };
    std::vector<ModelCandidate> models = {
        {"SmolLM2-135M (Q8_0)", "third_party/models/smollm2-135m.gguf"},
        {"SmolLM2-360M (Q4_K_M)", "third_party/models/smollm2-360m.gguf"}
    };

    for (const auto& m : models) {
        if (!std::filesystem::exists(m.path)) {
            std::cout << "[L1 LLM: " << m.label << "] Skipped (file not present: " << m.path << ")\n\n";
            continue;
        }
        LlmPredictor llm;
        auto loadStart = std::chrono::steady_clock::now();
        if (!llm.loadModel(m.path, 2)) {
            std::cout << "[L1 LLM: " << m.label << "] Failed to load model.\n\n";
            continue;
        }
        double loadMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - loadStart).count();

        // Warmup
        llm.predict("Hello, I wanted ", "", 3, 0.20f);

        std::vector<double> timesMs;
        std::cout << "[L1 LLM: " << m.label << " | 2 CPU threads | mmap]\n"
                  << "  Load time: " << std::setprecision(1) << loadMs << " ms\n";
        for (const auto& s : samples) {
            auto t0 = std::chrono::steady_clock::now();
            auto r = llm.predict(s.context, s.partial, 3, 0.20f);
            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            timesMs.push_back(ms);
            std::cout << "  Prompt: \"" << s.context << s.partial << "\" -> \""
                      << LlmPredictor::wstringToUtf8(r.text) << "\" ("
                      << std::setprecision(1) << ms << " ms)\n";
        }
        std::cout << "  p50 latency: " << std::setprecision(2) << percentileMs(timesMs, 0.50) << " ms\n"
                  << "  p95 latency: " << percentileMs(timesMs, 0.95) << " ms\n\n";
    }

    return 0;
}
