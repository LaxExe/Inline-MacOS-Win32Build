#include <gtest/gtest.h>
#include "core/config/config_json.h"

TEST(ConfigPersistence, RoundTripsAllFields) {
    AppConfig cfg;
    cfg.correctionAggressiveness = 0.75f;
    cfg.ghostTextEnabled = false;
    cfg.smartPredictionsEnabled = false;
    cfg.ghostAcceptMode = 1;
    cfg.ghostStyle = 2;
    cfg.onboardingVersion = 3;
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
    EXPECT_EQ(roundTripped.onboardingVersion, 3);
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
    EXPECT_FALSE(cfg.ghostTextEnabled);
    EXPECT_EQ(cfg.predictionLength, AppConfig::PredictionLength::Balanced); // untouched default
}
TEST(ConfigPersistence, LegacyUpgradeDisablesGhostTextOnceUntilUserEnables) {
    bool ok = false;
    AppConfig legacy = deserializeConfig(R"({"ghostTextEnabled": true, "onboardingVersion": 2})", ok);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(legacy.ghostTextEnabled);
    EXPECT_TRUE(migrateConfigIfNeeded(legacy));
    EXPECT_FALSE(legacy.ghostTextEnabled);
    EXPECT_EQ(legacy.onboardingVersion, 3);

    // Once migrated to v3, explicitly enabling Ghost Text must be preserved across restarts
    legacy.ghostTextEnabled = true;
    std::string persisted = serializeConfig(legacy);
    AppConfig reloaded = deserializeConfig(persisted, ok);
    ASSERT_TRUE(ok);
    EXPECT_FALSE(migrateConfigIfNeeded(reloaded));
    EXPECT_TRUE(reloaded.ghostTextEnabled);
    EXPECT_EQ(reloaded.onboardingVersion, 3);
}
