#include <gtest/gtest.h>
#include "gui/settings_binding.h"
#include "gui/settings_dialog.h"
#include "gui/permission_dialog.h"
#include <QApplication>

TEST(SettingsBinding, AggressivenessSliderMapsToUnitRange) {
    EXPECT_FLOAT_EQ(sliderPositionToAggressiveness(0),   0.0f);
    EXPECT_FLOAT_EQ(sliderPositionToAggressiveness(100), 1.0f);
    EXPECT_FLOAT_EQ(sliderPositionToAggressiveness(50),  0.5f);
}

TEST(SettingsBinding, PredictionLengthEnumRoundTrips) {
    EXPECT_EQ(predictionLengthFromIndex(0), AppConfig::PredictionLength::Conservative);
    EXPECT_EQ(predictionLengthFromIndex(1), AppConfig::PredictionLength::Balanced);
    EXPECT_EQ(predictionLengthFromIndex(2), AppConfig::PredictionLength::Verbose);
    EXPECT_EQ(predictionLengthFromIndex(3), AppConfig::PredictionLength::SingleWord);
    EXPECT_EQ(predictionLengthToIndex(AppConfig::PredictionLength::SingleWord), 3);
}

TEST(SettingsBinding, DefaultAppConfigHasTogglesEnabled) {
    AppConfig cfg;
    EXPECT_TRUE(cfg.masterEnabled);
    EXPECT_TRUE(cfg.singleKeyUndoEnabled);
    EXPECT_TRUE(cfg.splitRunTogetherWords);
    EXPECT_TRUE(cfg.sentenceCorrectionEnabled);
    EXPECT_TRUE(cfg.launchAtLogin);
    EXPECT_FALSE(cfg.ghostTextEnabled); // Ghost Text is disabled by default (beta feature under testing)
    EXPECT_EQ(cfg.onboardingVersion, 3);
    EXPECT_TRUE(cfg.smartPredictionsEnabled);
    EXPECT_EQ(cfg.ghostAcceptMode, 0);
    EXPECT_EQ(cfg.ghostStyle, 1);
    EXPECT_FALSE(cfg.ghostDisabledApps.empty());
}

TEST(SettingsBinding, GhostTextCombosRoundTrip) {
    EXPECT_EQ(acceptKeyToIndex(acceptKeyFromIndex(0)), 0);
    EXPECT_EQ(acceptKeyToIndex(acceptKeyFromIndex(1)), 1);
    EXPECT_EQ(ghostAcceptModeToIndex(ghostAcceptModeFromIndex(0)), 0);
    EXPECT_EQ(ghostAcceptModeToIndex(ghostAcceptModeFromIndex(1)), 1);
    EXPECT_EQ(ghostStyleToIndex(ghostStyleFromIndex(0)), 0);
    EXPECT_EQ(ghostStyleToIndex(ghostStyleFromIndex(2)), 2);
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

TEST(SettingsBinding, PermissionDialogHasThreeCleanSlides) {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    std::unique_ptr<QApplication> ownedApp;
    if (!app) {
        ownedApp = std::make_unique<QApplication>(argc, argv);
    }

    EXPECT_EQ(PermissionDialog::kSlideCount, 3);
    PermissionDialog dlg(nullptr, 0);
    dlg.resize(700, 520);

    for (int step = 0; step < 3; ++step) {
        dlg.setCurrentSlide(step);
        QPixmap pixmap(dlg.size());
        dlg.render(&pixmap);
        EXPECT_FALSE(pixmap.isNull());

        std::string path = "/Users/lakshman/.gemini/antigravity/brain/2d6cc60c-ccaf-47cd-ae08-05516b88a5b5/slide_" + std::to_string(step) + ".png";
        pixmap.save(QString::fromStdString(path), "PNG");
    }

    // Also verify Dark Mode
    dlg.toggleTheme();
    for (int step = 0; step < 3; ++step) {
        dlg.setCurrentSlide(step);
        QPixmap pixmap(dlg.size());
        dlg.render(&pixmap);
        EXPECT_FALSE(pixmap.isNull());

        std::string path = "/Users/lakshman/.gemini/antigravity/brain/2d6cc60c-ccaf-47cd-ae08-05516b88a5b5/slide_" + std::to_string(step) + "_dark.png";
        pixmap.save(QString::fromStdString(path), "PNG");
    }
}
