#include <gtest/gtest.h>
#include "core/config/config_json.h"

TEST(ConfigPersistence, RoundTripsAllFields) {
    AppConfig cfg;
    cfg.correctionAggressiveness = 0.75f;
    cfg.ghostTextEnabled = false;
    cfg.smartPredictionsEnabled = false;
    cfg.ghostAcceptMode = 1;
    cfg.ghostStyle = 2;
    cfg.onboardingVersion = 2;
    cfg.ignoreWords = {"kubernetes", "laxExe"};
    cfg.ghostDisabledApps = {"com.apple.Terminal", "com.microsoft.VSCode"};
    std::string json = serializeConfig(cfg);
    bool ok = false;
    AppConfig roundTripped = deserializeConfig(json, ok);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(roundTripped.correctionAggressiveness, 0.75f);
    EXPECT_FALSE(roundTripped.ghostTextEnabled);
    EXPECT_FALSE(roundTripped.smartPredictionsEnabled);
    EXPECT_EQ(roundTripped.ghostAcceptMode, 1);
    EXPECT_EQ(roundTripped.ghostStyle, 2);
    EXPECT_EQ(roundTripped.onboardingVersion, 2);
    EXPECT_EQ(roundTripped.ignoreWords, cfg.ignoreWords);
    EXPECT_EQ(roundTripped.ghostDisabledApps, cfg.ghostDisabledApps);
}
TEST(ConfigPersistence, CorruptJsonFallsBackToDefaultsWithoutCrash) {
    bool ok = false;
    AppConfig cfg = deserializeConfig("{not valid json!!", ok);
    EXPECT_FALSE(ok);
    EXPECT_EQ(cfg.correctionAggressiveness, 0.5f); // default, not garbage
}
TEST(ConfigPersistence, MissingFieldsFallBackToDefaults) {
    bool ok = false;
    AppConfig cfg = deserializeConfig(R"({"masterEnabled": false})", ok);
    EXPECT_TRUE(ok);
    EXPECT_FALSE(cfg.masterEnabled);
    EXPECT_EQ(cfg.predictionLength, AppConfig::PredictionLength::Balanced); // untouched default
}
