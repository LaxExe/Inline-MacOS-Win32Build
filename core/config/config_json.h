#pragma once

#include "core/config/app_config.h"
#include <string>

std::string serializeConfig(const AppConfig& cfg);
AppConfig deserializeConfig(const std::string& jsonStr, bool& outOk);

/// Returns the platform-appropriate path to Inline's persisted config.json, creating parent directories.
std::string getDefaultConfigPath();

/// Writes cfg to path as JSON. Returns true on success.
bool saveConfigToFile(const AppConfig& cfg, const std::string& path = getDefaultConfigPath());

/// Migrates legacy configs (onboardingVersion < 3) so Ghost Text is off by default until explicitly enabled.
/// Returns true if cfg was modified.
bool migrateConfigIfNeeded(AppConfig& cfg);
