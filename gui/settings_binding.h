#pragma once

#include "core/config/app_config.h"

float sliderPositionToAggressiveness(int position);
int aggressivenessToSliderPosition(float aggressiveness);
AppConfig::PredictionLength predictionLengthFromIndex(int index);
int predictionLengthToIndex(AppConfig::PredictionLength length);

KeyCode acceptKeyFromIndex(int index);
int acceptKeyToIndex(KeyCode key);

int ghostAcceptModeFromIndex(int index);
int ghostAcceptModeToIndex(int mode);

int ghostStyleFromIndex(int index);
int ghostStyleToIndex(int style);
