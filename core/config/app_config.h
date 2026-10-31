#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "core/config/keycode.h"

inline std::vector<std::string> defaultGhostDisabledApps() {
    return {
        "com.apple.Terminal",
        "com.googlecode.iterm2",
        "dev.warp.Warp-Stable",
        "com.mitchellh.ghostty"
    };
}

struct AppConfig {
    bool masterEnabled = true;
    bool launchAtLogin = true;
    bool singleKeyUndoEnabled = true;
    bool splitRunTogetherWords = true;
    int  appTheme = 0; // 0 = System, 1 = Light, 2 = Dark
    KeyCode toggleHotkey = KeyCode::None;
    float correctionAggressiveness = 0.5f;
    int   maxDictionaryEditDistance = 2;
    std::vector<std::string> ignoreWords;
    bool  sentenceCorrectionEnabled = true;
    float sentenceCorrectionIntensity = 0.5f;
    bool  ghostTextEnabled = true;
    KeyCode acceptKey = KeyCode::Tab;
    enum class PredictionLength {
        Conservative = 0,
        Balanced = 1,
        Verbose = 2,
        SingleWord = 3
    } predictionLength = PredictionLength::Balanced;
    bool  smartPredictionsEnabled = true;
    int   ghostAcceptMode = 0; // 0 = Whole suggestion, 1 = One word
    int   ghostStyle = 1;      // 0 = Subtle, 1 = Standard, 2 = High contrast
    std::vector<std::string> disabledApps;      // on mac: bundle identifiers, e.g. "com.apple.Terminal"
    std::vector<std::string> ghostDisabledApps = defaultGhostDisabledApps();
    int   onboardingVersion = 2;
};

extern std::mutex configMutex;
extern AppConfig sharedConfig;
