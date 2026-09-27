#include <gtest/gtest.h>
#include "gui/settings_binding.h"

TEST(SettingsBinding, AggressivenessSliderMapsToUnitRange) {
    EXPECT_FLOAT_EQ(sliderPositionToAggressiveness(0),   0.0f);
    EXPECT_FLOAT_EQ(sliderPositionToAggressiveness(100), 1.0f);
    EXPECT_FLOAT_EQ(sliderPositionToAggressiveness(50),  0.5f);
}
TEST(SettingsBinding, PredictionLengthEnumRoundTrips) {
    EXPECT_EQ(predictionLengthFromIndex(0), AppConfig::PredictionLength::Conservative);
    EXPECT_EQ(predictionLengthFromIndex(2), AppConfig::PredictionLength::Verbose);
}
