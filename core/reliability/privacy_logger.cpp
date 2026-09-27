#include "core/reliability/privacy_logger.h"
#include <chrono>
#include <filesystem>
#include <iostream>

PrivacyLogger& PrivacyLogger::instance() {
    static PrivacyLogger inst;
    return inst;
}

PrivacyLogger::~PrivacyLogger() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (safeLogFile_.is_open()) safeLogFile_.close();
    if (diagLogFile_.is_open()) diagLogFile_.close();
}

void PrivacyLogger::init(const std::string& appName) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* home = getenv("HOME");
    std::string homeDir = home ? home : ".";
    logDir_ = homeDir + "/Library/Logs/" + appName;
    std::filesystem::create_directories(logDir_);

    safeLogFile_.open(logDir_ + "/app.log", std::ios::out | std::ios::app);
}

void PrivacyLogger::logEvent(const std::string& eventType, int64_t elapsedNs, int errorCode) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    if (safeLogFile_.is_open()) {
        safeLogFile_ << ms << " [EVENT] " << eventType << " elapsed_ns=" << elapsedNs << " code=" << errorCode << "\n";
        safeLogFile_.flush();
    }
}

void PrivacyLogger::logDiagnosticText(const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!diagnosticMode_) return;
    if (!diagLogFile_.is_open() && !logDir_.empty()) {
        diagLogFile_.open(logDir_ + "/diagnostic.log", std::ios::out | std::ios::app);
    }
    if (diagLogFile_.is_open()) {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        diagLogFile_ << ms << " [DIAG] " << text << "\n";
        diagLogFile_.flush();
    }
}

void PrivacyLogger::setDiagnosticMode(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    diagnosticMode_ = enabled;
}

bool PrivacyLogger::isDiagnosticMode() const {
    return diagnosticMode_;
}
