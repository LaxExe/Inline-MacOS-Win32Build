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
    if (index <= 0) return AppConfig::PredictionLength::Conservative;
    if (index >= 2) return AppConfig::PredictionLength::Verbose;
    return AppConfig::PredictionLength::Balanced;
}

int predictionLengthToIndex(AppConfig::PredictionLength length) {
    return static_cast<int>(length);
}
