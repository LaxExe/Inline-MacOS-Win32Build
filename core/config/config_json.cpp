#include "core/config/config_json.h"
#include <sstream>
#include <iomanip>
#include <regex>
#include <iostream>

std::mutex configMutex;
AppConfig sharedConfig;

static std::string escapeJsonString(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\b') o << "\\b";
        else if (c == '\f') o << "\\f";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else o << c;
    }
    return o.str();
}

std::string serializeConfig(const AppConfig& cfg) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"masterEnabled\": " << (cfg.masterEnabled ? "true" : "false") << ",\n";
    ss << "  \"launchAtLogin\": " << (cfg.launchAtLogin ? "true" : "false") << ",\n";
    ss << "  \"toggleHotkey\": " << static_cast<uint32_t>(cfg.toggleHotkey) << ",\n";
    ss << "  \"correctionAggressiveness\": " << cfg.correctionAggressiveness << ",\n";
    ss << "  \"maxDictionaryEditDistance\": " << cfg.maxDictionaryEditDistance << ",\n";
    ss << "  \"ignoreWords\": [";
    for (size_t i = 0; i < cfg.ignoreWords.size(); ++i) {
        ss << "\"" << escapeJsonString(cfg.ignoreWords[i]) << "\"";
        if (i + 1 < cfg.ignoreWords.size()) ss << ", ";
    }
    ss << "],\n";
    ss << "  \"sentenceCorrectionEnabled\": " << (cfg.sentenceCorrectionEnabled ? "true" : "false") << ",\n";
    ss << "  \"sentenceCorrectionIntensity\": " << cfg.sentenceCorrectionIntensity << ",\n";
    ss << "  \"ghostTextEnabled\": " << (cfg.ghostTextEnabled ? "true" : "false") << ",\n";
    ss << "  \"acceptKey\": " << static_cast<uint32_t>(cfg.acceptKey) << ",\n";
    ss << "  \"predictionLength\": " << static_cast<int>(cfg.predictionLength) << ",\n";
    ss << "  \"disabledApps\": [";
    for (size_t i = 0; i < cfg.disabledApps.size(); ++i) {
        ss << "\"" << escapeJsonString(cfg.disabledApps[i]) << "\"";
        if (i + 1 < cfg.disabledApps.size()) ss << ", ";
    }
    ss << "]\n";
    ss << "}";
    return ss.str();
}

AppConfig deserializeConfig(const std::string& jsonStr, bool& outOk) {
    AppConfig cfg;
    outOk = false;

    // Check basic json object validity
    size_t start = jsonStr.find('{');
    size_t end = jsonStr.rfind('}');
    if (start == std::string::npos || end == std::string::npos || start >= end) {
        return cfg;
    }

    outOk = true;

    auto parseBool = [&](const std::string& key, bool& val) {
        std::regex re("\"" + key + "\"\\s*:\\s*(true|false)");
        std::smatch m;
        if (std::regex_search(jsonStr, m, re)) {
            val = (m[1].str() == "true");
        }
    };

    auto parseFloat = [&](const std::string& key, float& val) {
        std::regex re("\"" + key + "\"\\s*:\\s*([0-9]+(?:\\.[0-9]+)?)");
        std::smatch m;
        if (std::regex_search(jsonStr, m, re)) {
            try { val = std::stof(m[1].str()); } catch (...) {}
        }
    };

    auto parseInt = [&](const std::string& key, int& val) {
        std::regex re("\"" + key + "\"\\s*:\\s*([0-9]+)");
        std::smatch m;
        if (std::regex_search(jsonStr, m, re)) {
            try { val = std::stoi(m[1].str()); } catch (...) {}
        }
    };

    auto parseStringArray = [&](const std::string& key, std::vector<std::string>& list) {
        std::regex re("\"" + key + "\"\\s*:\\s*\\[([^\\]]*)\\]");
        std::smatch m;
        if (std::regex_search(jsonStr, m, re)) {
            list.clear();
            std::string content = m[1].str();
            std::regex elemRe("\"([^\"]*)\"");
            auto begin = std::sregex_iterator(content.begin(), content.end(), elemRe);
            auto endIt = std::sregex_iterator();
            for (std::sregex_iterator i = begin; i != endIt; ++i) {
                list.push_back((*i)[1].str());
            }
        }
    };

    parseBool("masterEnabled", cfg.masterEnabled);
    parseBool("launchAtLogin", cfg.launchAtLogin);
    int hotkeyInt = 0;
    parseInt("toggleHotkey", hotkeyInt);
    if (hotkeyInt > 0) cfg.toggleHotkey = static_cast<KeyCode>(hotkeyInt);

    parseFloat("correctionAggressiveness", cfg.correctionAggressiveness);
    parseInt("maxDictionaryEditDistance", cfg.maxDictionaryEditDistance);
    parseStringArray("ignoreWords", cfg.ignoreWords);

    parseBool("sentenceCorrectionEnabled", cfg.sentenceCorrectionEnabled);
    parseFloat("sentenceCorrectionIntensity", cfg.sentenceCorrectionIntensity);
    parseBool("ghostTextEnabled", cfg.ghostTextEnabled);

    int acceptKeyInt = static_cast<int>(KeyCode::Tab);
    parseInt("acceptKey", acceptKeyInt);
    cfg.acceptKey = static_cast<KeyCode>(acceptKeyInt);

    int predLenInt = static_cast<int>(AppConfig::PredictionLength::Balanced);
    parseInt("predictionLength", predLenInt);
    if (predLenInt >= 0 && predLenInt <= 2) {
        cfg.predictionLength = static_cast<AppConfig::PredictionLength>(predLenInt);
    }

    parseStringArray("disabledApps", cfg.disabledApps);

    return cfg;
}
