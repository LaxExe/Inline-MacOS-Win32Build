#pragma once

#include <QDialog>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include "core/config/app_config.h"

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

    void loadFromConfig();
    void saveToConfig();
    void updateStatusInfo(bool hookRunning, size_t dictWords);

public slots:
    void appendLog(const QString& msg);
    void refreshPermissions();
    void openWizard();

private:
    QTabWidget* tabWidget_ = nullptr;

    // Diagnostics & Permissions
    QLabel* axStatusLabel_ = nullptr;
    QPushButton* axFixBtn_ = nullptr;
    QLabel* imStatusLabel_ = nullptr;
    QPushButton* imFixBtn_ = nullptr;
    QLabel* dictStatusLabel_ = nullptr;
    QLabel* hookStatusLabel_ = nullptr;
    QLineEdit* testInputEdit_ = nullptr;
    QPlainTextEdit* liveLogEdit_ = nullptr;
    QTimer* refreshTimer_ = nullptr;

    // Config settings
    QCheckBox* masterEnabledCheck_ = nullptr;
    QCheckBox* launchAtLoginCheck_ = nullptr;
    QSlider* aggressivenessSlider_ = nullptr;
    QLabel* aggressivenessLabel_ = nullptr;
    QSpinBox* maxEditDistanceSpin_ = nullptr;
    QPlainTextEdit* ignoreWordsEdit_ = nullptr;
    QCheckBox* sentenceCorrectionCheck_ = nullptr;
    QSlider* sentenceIntensitySlider_ = nullptr;
    QPlainTextEdit* disabledAppsEdit_ = nullptr;

    bool hookRunning_ = false;
    size_t dictWords_ = 0;

    void setupUi();
    void applyTheme();
    QWidget* createGeneralTab();
    QWidget* createDiagnosticsTab();
    QWidget* createPlaygroundTab();
};
