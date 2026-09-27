#pragma once

#include "core/config/app_config.h"

float sliderPositionToAggressiveness(int position);
int aggressivenessToSliderPosition(float aggressiveness);
AppConfig::PredictionLength predictionLengthFromIndex(int index);
int predictionLengthToIndex(AppConfig::PredictionLength length);
