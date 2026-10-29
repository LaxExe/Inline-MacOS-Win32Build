#include <gtest/gtest.h>
#include "gui/settings_binding.h"
#include "gui/settings_dialog.h"
#include <QApplication>

TEST(SettingsBinding, AggressivenessSliderMapsToUnitRange) {
    EXPECT_FLOAT_EQ(sliderPositionToAggressiveness(0),   0.0f);
    EXPECT_FLOAT_EQ(sliderPositionToAggressiveness(100), 1.0f);
    EXPECT_FLOAT_EQ(sliderPositionToAggressiveness(50),  0.5f);
}

TEST(SettingsBinding, PredictionLengthEnumRoundTrips) {
    EXPECT_EQ(predictionLengthFromIndex(0), AppConfig::PredictionLength::Conservative);
    EXPECT_EQ(predictionLengthFromIndex(2), AppConfig::PredictionLength::Verbose);
}

TEST(SettingsBinding, DefaultAppConfigHasTogglesEnabled) {
    AppConfig cfg;
    EXPECT_TRUE(cfg.masterEnabled);
    EXPECT_TRUE(cfg.singleKeyUndoEnabled);
    EXPECT_TRUE(cfg.splitRunTogetherWords);
    EXPECT_TRUE(cfg.sentenceCorrectionEnabled);
    EXPECT_TRUE(cfg.launchAtLogin);
}

TEST(SettingsBinding, ToggleSwitchVisuallyReflectsOnState) {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    std::unique_ptr<QApplication> ownedApp;
    if (!app) {
        ownedApp = std::make_unique<QApplication>(argc, argv);
    }

    ToggleSwitch toggle;
    toggle.setChecked(true);
    EXPECT_TRUE(toggle.isChecked());
    // offset must be 1.0 so that paintEvent renders the green ON background and knob to the right
    EXPECT_DOUBLE_EQ(toggle.offset(), 1.0);

    toggle.setChecked(false);
    EXPECT_FALSE(toggle.isChecked());
    EXPECT_DOUBLE_EQ(toggle.offset(), 0.0);
}
