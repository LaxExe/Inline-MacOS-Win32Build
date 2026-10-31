#include "gui/settings_binding.h"
#include <algorithm>

float sliderPositionToAggressiveness(int position) {
    int clamped = std::clamp(position, 0, 100);
    return static_cast<float>(clamped) / 100.0f;
}

int aggressivenessToSliderPosition(float aggressiveness) {
    float clamped = std::clamp(aggressiveness, 0.0f, 1.0f);
    return static_cast<int>(clamped * 100.0f + 0.5f);
}

AppConfig::PredictionLength predictionLengthFromIndex(int index) {
    if (index == 0) return AppConfig::PredictionLength::Conservative;
    if (index == 1) return AppConfig::PredictionLength::Balanced;
    if (index == 2) return AppConfig::PredictionLength::Verbose;
    if (index == 3) return AppConfig::PredictionLength::SingleWord;
    return AppConfig::PredictionLength::Balanced;
}

int predictionLengthToIndex(AppConfig::PredictionLength length) {
    switch (length) {
        case AppConfig::PredictionLength::Conservative: return 0;
        case AppConfig::PredictionLength::Balanced:     return 1;
        case AppConfig::PredictionLength::Verbose:      return 2;
        case AppConfig::PredictionLength::SingleWord:   return 3;
    }
    return 1;
}

KeyCode acceptKeyFromIndex(int index) {
    return (index == 1) ? KeyCode::RightArrow : KeyCode::Tab;
}

int acceptKeyToIndex(KeyCode key) {
    return (key == KeyCode::RightArrow) ? 1 : 0;
}

int ghostAcceptModeFromIndex(int index) {
    return (index == 1) ? 1 : 0;
}

int ghostAcceptModeToIndex(int mode) {
    return (mode == 1) ? 1 : 0;
}

int ghostStyleFromIndex(int index) {
    return std::clamp(index, 0, 2);
}

int ghostStyleToIndex(int style) {
    return std::clamp(style, 0, 2);
}
