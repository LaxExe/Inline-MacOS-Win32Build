#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "core/config/keycode.h"

struct AppConfig {
    bool masterEnabled = true;
    bool launchAtLogin = false;
    KeyCode toggleHotkey = KeyCode::None;
    float correctionAggressiveness = 0.5f;
    int   maxDictionaryEditDistance = 2;
    std::vector<std::string> ignoreWords;
    bool  sentenceCorrectionEnabled = false;
    float sentenceCorrectionIntensity = 0.5f;
    bool  ghostTextEnabled = false;
    KeyCode acceptKey = KeyCode::Tab;
    enum class PredictionLength { Conservative, Balanced, Verbose } predictionLength = PredictionLength::Balanced;
    std::vector<std::string> disabledApps;   // on mac: bundle identifiers, e.g. "com.apple.Terminal"
};

extern std::mutex configMutex;
extern AppConfig sharedConfig;
