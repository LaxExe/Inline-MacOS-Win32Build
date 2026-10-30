#pragma once

#include <QDialog>
#include <QAbstractButton>
#include <QPropertyAnimation>
#include <QStackedWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include "core/config/app_config.h"

enum class IconType {
    Gear,
    Grid,
    Display,
    Book,
    Shield,
    Terminal,
    Lock,
    Laptop,
    Globe,
    Bolt,
    Undo,
    Message,
    TextCase,
    WordSplit,
    Tag,
    Gauge,
    Ruler,
    MenuBar,
    Accessibility,
    Keyboard,
    Activity,
    Database
};

// Modern iOS/macOS Pill-Style Toggle Switch
class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal offset READ offset WRITE setOffset)
public:
    explicit ToggleSwitch(QWidget* parent = nullptr);
    QSize sizeHint() const override { return QSize(44, 24); }

    qreal offset() const { return offset_; }
    void setOffset(qreal o) { offset_ = o; update(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void nextCheckState() override;
    void checkStateSet() override;

private:
    qreal offset_ = 0.0;
    QPropertyAnimation* anim_ = nullptr;
};

// Modern Theme Selection Preview Box (System / Light / Dark)
class ThemePreviewCard : public QWidget {
    Q_OBJECT
public:
    enum class ThemeMode { System, Light, Dark };
    explicit ThemePreviewCard(ThemeMode mode, const QString& label, QWidget* parent = nullptr);

    bool isSelected() const { return selected_; }
    void setSelected(bool s);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    ThemeMode mode_;
    QString label_;
    bool selected_ = false;
};

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

    void loadFromConfig();
    void saveToConfig();
    void updateStatusInfo(bool hookRunning, size_t dictWords);
    bool isDarkTheme() const { return isDarkTheme_; }

public slots:
    void appendLog(const QString& msg);
    void refreshPermissions();
    void openWizard();

private slots:
    void onSidebarItemChanged(int index);
    void onSearchFilterChanged(const QString& query);
    void onThemeSelected(ThemePreviewCard::ThemeMode mode);
    void onOpenAccessibilitySettings();
    void onOpenInputMonitoringSettings();

private:
    void applyTheme(ThemePreviewCard::ThemeMode mode = ThemePreviewCard::ThemeMode::System);
    ThemePreviewCard::ThemeMode currentTheme_ = ThemePreviewCard::ThemeMode::System;
    bool isDarkTheme_ = false;
    bool isLoadingConfig_ = false;
    // Navigation & Layout
    QLineEdit* searchEdit_ = nullptr;
    QListWidget* sidebarList_ = nullptr;
    QStackedWidget* pageStack_ = nullptr;

    // Page 0: General Controls
    ToggleSwitch* launchAtLoginToggle_ = nullptr;
    QComboBox* languageCombo_ = nullptr;
    ToggleSwitch* masterEnabledToggle_ = nullptr;
    ToggleSwitch* singleKeyUndoToggle_ = nullptr;

    // Page 1: Features
    ToggleSwitch* sentenceCorrectionToggle_ = nullptr;
    ToggleSwitch* runTogetherToggle_ = nullptr;
    QComboBox* sensitivityCombo_ = nullptr;
    QComboBox* editDistanceCombo_ = nullptr;

    // Page 2: Appearance
    ThemePreviewCard* systemThemeCard_ = nullptr;
    ThemePreviewCard* lightThemeCard_ = nullptr;
    ThemePreviewCard* darkThemeCard_ = nullptr;

    // Page 3: Vocabulary & Slang
    QPlainTextEdit* ignoreWordsEdit_ = nullptr;

    // Page 4: Excluded Applications
    QPlainTextEdit* disabledAppsEdit_ = nullptr;

    // Page 5: Playground
    QLineEdit* testInputEdit_ = nullptr;
    QPlainTextEdit* liveLogEdit_ = nullptr;

    // Page 6: Permissions & Health
    QLabel* axStatusBadge_ = nullptr;
    QPushButton* axActionBtn_ = nullptr;
    QLabel* imStatusBadge_ = nullptr;
    QPushButton* imActionBtn_ = nullptr;
    QLabel* engineStatusBadge_ = nullptr;
    QLabel* dictCountBadge_ = nullptr;

    QTimer* refreshTimer_ = nullptr;
    bool hookRunning_ = false;
    size_t dictWords_ = 0;

    void setupUi();
    void applyTheme();
    void buildSidebar();

    QWidget* createGeneralPage();
    QWidget* createFeaturesPage();
    QWidget* createAppearancePage();
    QWidget* createVocabularyPage();
    QWidget* createExcludedAppsPage();
    QWidget* createPlaygroundPage();
    QWidget* createPermissionsPage();

    QWidget* createSettingCard();
    QWidget* createSettingRow(IconType icon, const QString& title, const QString& subtitle, QWidget* controlWidget, bool isLast = false);

    static QIcon renderVectorIcon(IconType type, const QColor& color, int size = 18);
    static QWidget* createIconBadge(IconType type);
};
