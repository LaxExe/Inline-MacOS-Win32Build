#pragma once

#include "core/config/app_config.h"
#include <string>

std::string serializeConfig(const AppConfig& cfg);
AppConfig deserializeConfig(const std::string& jsonStr, bool& outOk);
