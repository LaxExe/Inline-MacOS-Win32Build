#include <QApplication>
#include <QMessageBox>
#include <fstream>
#include <filesystem>
#include "gui/main_tray_app.h"
#include "platform/mac/permissions.h"
#include "core/config/config_json.h"
#include "core/reliability/privacy_logger.h"

static void loadInitialConfig() {
    const char* home = getenv("HOME");
    std::string path = (home ? home : ".") + std::string("/Library/Application Support/TypingAssistant/config.json");
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

    PrivacyLogger::instance().init("TypingAssistant");
    loadInitialConfig();

    // Check permissions
    bool inputMon = checkInputMonitoringPermission();
    bool ax = checkAccessibilityPermission(true);

    auto action = decidePermissionAction(inputMon, ax, true);
    if (action == PermissionAction::PromptInputMonitoring || action == PermissionAction::DirectToInputMonitoringSettings) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Permissions Required");
        msgBox.setText("Typing Assistant requires Input Monitoring permission to observe keystrokes.");
        msgBox.setInformativeText("Please enable Typing Assistant in System Settings -> Privacy & Security -> Input Monitoring.");
        QPushButton* openBtn = msgBox.addButton("Open System Settings", QMessageBox::ActionRole);
        msgBox.addButton("Cancel", QMessageBox::RejectRole);
        msgBox.exec();

        if (msgBox.clickedButton() == openBtn) {
            openInputMonitoringSettings();
        }
    } else if (action == PermissionAction::PromptAccessibility || action == PermissionAction::DirectToAccessibilitySettings) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Accessibility Permission Required");
        msgBox.setText("Typing Assistant requires Accessibility permission to insert text corrections.");
        msgBox.setInformativeText("Please enable Typing Assistant in System Settings -> Privacy & Security -> Accessibility.");
        QPushButton* openBtn = msgBox.addButton("Open System Settings", QMessageBox::ActionRole);
        msgBox.addButton("Cancel", QMessageBox::RejectRole);
        msgBox.exec();

        if (msgBox.clickedButton() == openBtn) {
            openAccessibilitySettings();
        }
    }

    MainTrayApp trayApp;
    trayApp.initEngine();

    return app.exec();
}
