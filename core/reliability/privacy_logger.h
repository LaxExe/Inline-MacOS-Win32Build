#pragma once

#include <string>
#include <fstream>
#include <mutex>

class PrivacyLogger {
public:
    static PrivacyLogger& instance();

    void init(const std::string& appName);
    void logEvent(const std::string& eventType, int64_t elapsedNs, int errorCode = 0);
    void logDiagnosticText(const std::string& text);
    void setDiagnosticMode(bool enabled);
    bool isDiagnosticMode() const;

private:
    PrivacyLogger() = default;
    ~PrivacyLogger();

    std::mutex mutex_;
    std::ofstream safeLogFile_;
    std::ofstream diagLogFile_;
    bool diagnosticMode_ = false;
    std::string logDir_;
};
