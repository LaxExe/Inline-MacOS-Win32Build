#include <gtest/gtest.h>
#include "core/config/config_json.h"

TEST(ConfigPersistence, RoundTripsAllFields) {
    AppConfig cfg;
    cfg.correctionAggressiveness = 0.75f;
    cfg.ghostTextEnabled = false;
    cfg.ignoreWords = {"kubernetes", "laxExe"};
    std::string json = serializeConfig(cfg);
    bool ok = false;
    AppConfig roundTripped = deserializeConfig(json, ok);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(roundTripped.correctionAggressiveness, 0.75f);
    EXPECT_FALSE(roundTripped.ghostTextEnabled);
    EXPECT_EQ(roundTripped.ignoreWords, cfg.ignoreWords);
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
