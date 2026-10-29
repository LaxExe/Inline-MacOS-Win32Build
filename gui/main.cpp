#include <QApplication>
#include <QMessageBox>
#include <fstream>
#include <filesystem>
#include "gui/main_tray_app.h"
#include "gui/permission_dialog.h"
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
#include "platform/mac/permissions.h"
#else
#include "platform/win/permissions.h"
#endif
#include "core/config/config_json.h"
#include "core/reliability/privacy_logger.h"

static void loadInitialConfig() {
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
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    PrivacyLogger::instance().init("Inline");
    loadInitialConfig();

    // If permissions are missing, show the onboarding & permission explanation dialog
    if (!PermissionDialog::areAllPermissionsGranted()) {
        PermissionDialog permDialog;
        permDialog.exec();
    }


    MainTrayApp trayApp;
    trayApp.initEngine();

    return app.exec();
}
