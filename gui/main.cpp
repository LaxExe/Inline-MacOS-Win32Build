#include <QApplication>
#include <QMessageBox>
#include <QLockFile>
#include <QDir>
#include <fstream>
#include <filesystem>
#include <iostream>
#include "gui/main_tray_app.h"
#include "gui/permission_dialog.h"
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
#include "platform/mac/permissions.h"
#else
#include "platform/win/permissions.h"
#endif
#include "core/config/config_json.h"
#include "core/reliability/privacy_logger.h"

static bool loadInitialConfig() {
    std::string path;
#ifdef _WIN32
    const char* appData = getenv("APPDATA");
    path = (appData ? appData : ".") + std::string("/Inline/config.json");
#else
    const char* home = getenv("HOME");
    path = (home ? home : ".") + std::string("/Library/Application Support/Inline/config.json");
#endif
    if (std::filesystem::exists(path)) {
        std::ifstream in(path);
        if (in.is_open()) {
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            bool ok = false;
            AppConfig loaded = deserializeConfig(content, ok);
            if (ok) {
                std::lock_guard<std::mutex> lock(configMutex);
                sharedConfig = loaded;
            }
        }
        return true;
    }
    return false;
}

int main(int argc, char *argv[]) {
    QLockFile lockFile(QDir::tempPath() + "/com.inline.mac.lock");
    lockFile.setStaleLockTime(0);
    if (!lockFile.tryLock()) {
        std::ofstream trigger("/tmp/com.inline.mac.show");
        if (trigger.is_open()) {
            trigger << "1";
            trigger.close();
        }
        std::cerr << "Inline is already running. Signaling active instance to show window." << std::endl;
        return 0;
    }

#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    disableAppNap();
#endif

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    PrivacyLogger::instance().init("Inline");
    bool hasExistingConfig = loadInitialConfig();

    MainTrayApp trayApp;
    trayApp.initEngine();

    QTimer::singleShot(0, [&trayApp, hasExistingConfig]() {
        // On first launch or when permissions are missing, present the 4-slide Walkthrough & Setup guide first,
        // then immediately open the main Settings window on screen.
        if (!hasExistingConfig || !PermissionDialog::areAllPermissionsGranted()) {
            trayApp.openWalkthrough();
        }
        trayApp.openPreferences();
    });

    return app.exec();
}
