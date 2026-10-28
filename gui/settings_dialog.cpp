#include "gui/settings_dialog.h"
#include "gui/settings_binding.h"
#include "gui/permission_dialog.h"
#include "core/config/config_json.h"
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
#include "platform/mac/login/login_item.h"
#include "platform/mac/permissions.h"
#else
#include "platform/win/login/login_item.h"
#include "platform/win/permissions.h"
#endif
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QDateTime>
#include <fstream>
#include <filesystem>

static std::string getConfigFilePath() {
    std::string path;
#ifdef _WIN32
    const char* appData = getenv("APPDATA");
    path = (appData ? appData : ".") + std::string("/TypingAssistant");
#else
    const char* home = getenv("HOME");
    path = (home ? home : ".") + std::string("/Library/Application Support/TypingAssistant");
#endif
    std::filesystem::create_directories(path);
    return path + "/config.json";
}

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Inline Settings"));
    resize(560, 600);
    setMinimumSize(500, 520);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    setupUi();
    applyTheme();
    loadFromConfig();

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &SettingsDialog::refreshPermissions);
    refreshTimer_->start(1500);
}

void SettingsDialog::applyTheme() {
    setStyleSheet(
        "SettingsDialog {"
        "  background-color: #F8FAFC;"
        "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;"
        "}"
        "QTabWidget::pane {"
        "  border: 1px solid #E2E8F0;"
        "  border-radius: 8px;"
        "  background-color: #FFFFFF;"
        "  top: -1px;"
        "}"
        "QTabBar::tab {"
        "  background: transparent;"
        "  padding: 8px 18px;"
        "  margin-right: 4px;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "  color: #64748B;"
        "  border-radius: 6px;"
        "}"
        "QTabBar::tab:selected {"
        "  background-color: #E2E8F0;"
        "  color: #0F172A;"
        "  font-weight: 600;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  background-color: #F1F5F9;"
        "  color: #334155;"
        "}"
        "QScrollArea {"
        "  border: none;"
        "  background-color: transparent;"
        "}"
        "QFrame#card {"
        "  background-color: #FFFFFF;"
        "  border: 1px solid #E2E8F0;"
        "  border-radius: 8px;"
        "  padding: 12px 14px;"
        "}"
        "QCheckBox {"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "  color: #1E293B;"
        "  spacing: 8px;"
        "}"
        "QLineEdit, QPlainTextEdit, QSpinBox {"
        "  background-color: #FFFFFF;"
        "  border: 1px solid #CBD5E1;"
        "  border-radius: 6px;"
        "  padding: 6px 10px;"
        "  font-size: 13px;"
        "  color: #0F172A;"
        "}"
        "QLineEdit:focus, QPlainTextEdit:focus, QSpinBox:focus {"
        "  border: 1px solid #2563EB;"
        "}"
        "QSlider::groove:horizontal {"
        "  height: 6px;"
        "  background: #E2E8F0;"
        "  border-radius: 3px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background: #2563EB;"
        "  border-radius: 3px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: #FFFFFF;"
        "  border: 2px solid #2563EB;"
        "  width: 16px;"
        "  margin-top: -5px;"
        "  margin-bottom: -5px;"
        "  border-radius: 8px;"
        "}"
    );
}

void SettingsDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 16, 20, 16);
    mainLayout->setSpacing(12);

    tabWidget_ = new QTabWidget(this);
    tabWidget_->addTab(createGeneralTab(), tr("Preferences"));
    tabWidget_->addTab(createDiagnosticsTab(), tr("Permissions & Health"));
    tabWidget_->addTab(createPlaygroundTab(), tr("Playground"));

    mainLayout->addWidget(tabWidget_);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Save"));
    buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet(
        "QPushButton {"
        "  background-color: #2563EB; color: #FFFFFF; border: none; border-radius: 6px;"
        "  padding: 6px 16px; font-weight: 600; font-size: 13px;"
        "}"
        "QPushButton:hover { background-color: #1D4ED8; }"
    );
    buttonBox->button(QDialogButtonBox::Cancel)->setStyleSheet(
        "QPushButton {"
        "  background-color: transparent; color: #64748B; border: 1px solid #CBD5E1;"
        "  border-radius: 6px; padding: 6px 14px; font-size: 13px;"
        "}"
        "QPushButton:hover { background-color: #F1F5F9; color: #334155; }"
    );

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveToConfig();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

QWidget* SettingsDialog::createGeneralTab() {
    auto* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    auto* container = new QWidget(scrollArea);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    // 1. General Card
    auto* genCard = new QFrame(container);
    genCard->setObjectName("card");
    auto* genLayout = new QVBoxLayout(genCard);
    genLayout->setContentsMargins(12, 10, 12, 10);
    genLayout->setSpacing(10);

    auto* genTitle = new QLabel(tr("Core Settings"), genCard);
    genTitle->setStyleSheet("font-size: 14px; font-weight: 700; color: #0F172A;");
    genLayout->addWidget(genTitle);

    masterEnabledCheck_ = new QCheckBox(tr("Enable Autocorrection"), genCard);
    auto* masterSub = new QLabel(tr("Silently fixes typos and misspelled words across all apps."), genCard);
    masterSub->setStyleSheet("font-size: 12px; color: #64748B; margin-left: 24px;");
    genLayout->addWidget(masterEnabledCheck_);
    genLayout->addWidget(masterSub);

    launchAtLoginCheck_ = new QCheckBox(tr("Launch at Login"), genCard);
    auto* loginSub = new QLabel(tr("Starts Inline minimized in the menu bar on system startup."), genCard);
    loginSub->setStyleSheet("font-size: 12px; color: #64748B; margin-left: 24px;");
    genLayout->addWidget(launchAtLoginCheck_);
    genLayout->addWidget(loginSub);

    layout->addWidget(genCard);

    // 2. Engine Tuning Card
    auto* tuneCard = new QFrame(container);
    tuneCard->setObjectName("card");
    auto* tuneLayout = new QVBoxLayout(tuneCard);
    tuneLayout->setContentsMargins(12, 10, 12, 10);
    tuneLayout->setSpacing(10);

    auto* tuneTitle = new QLabel(tr("Engine Tuning"), tuneCard);
    tuneTitle->setStyleSheet("font-size: 14px; font-weight: 700; color: #0F172A;");
    tuneLayout->addWidget(tuneTitle);

    // Aggressiveness
    auto* aggHeader = new QHBoxLayout();
    aggHeader->addWidget(new QLabel(tr("Aggressiveness:"), tuneCard));
    aggressivenessLabel_ = new QLabel("50%", tuneCard);
    aggressivenessLabel_->setStyleSheet("font-weight: 600; color: #2563EB; font-size: 12px;");
    aggHeader->addStretch();
    aggHeader->addWidget(aggressivenessLabel_);
    tuneLayout->addLayout(aggHeader);

    aggressivenessSlider_ = new QSlider(Qt::Horizontal, tuneCard);
    aggressivenessSlider_->setRange(0, 100);
    tuneLayout->addWidget(aggressivenessSlider_);
    connect(aggressivenessSlider_, &QSlider::valueChanged, this, [this](int val) {
        aggressivenessLabel_->setText(QString::number(val) + "%");
    });

    // Max Edit Distance
    auto* distRow = new QHBoxLayout();
    distRow->addWidget(new QLabel(tr("Max Edit Distance:"), tuneCard));
    maxEditDistanceSpin_ = new QSpinBox(tuneCard);
    maxEditDistanceSpin_->setRange(1, 2);
    distRow->addWidget(maxEditDistanceSpin_);
    distRow->addStretch();
    tuneLayout->addLayout(distRow);

    auto* distSub = new QLabel(tr("1: Conservative (single-letter typos) • 2: Standard (transpositions & multi-letter)"), tuneCard);
    distSub->setStyleSheet("font-size: 11px; color: #64748B;");
    tuneLayout->addWidget(distSub);

    sentenceCorrectionCheck_ = new QCheckBox(tr("Auto-capitalize sentence starts"), tuneCard);
    tuneLayout->addWidget(sentenceCorrectionCheck_);

    sentenceIntensitySlider_ = new QSlider(Qt::Horizontal, tuneCard);
    sentenceIntensitySlider_->setRange(0, 100);
    sentenceIntensitySlider_->hide();

    layout->addWidget(tuneCard);

    // 3. Ignored Words Card
    auto* vocabCard = new QFrame(container);
    vocabCard->setObjectName("card");
    auto* vocabLayout = new QVBoxLayout(vocabCard);
    vocabLayout->setContentsMargins(12, 10, 12, 10);
    vocabLayout->setSpacing(6);

    auto* vocabTitle = new QLabel(tr("Ignored Words (one per line)"), vocabCard);
    vocabTitle->setStyleSheet("font-size: 14px; font-weight: 700; color: #0F172A;");
    vocabLayout->addWidget(vocabTitle);

    ignoreWordsEdit_ = new QPlainTextEdit(vocabCard);
    ignoreWordsEdit_->setMaximumHeight(70);
    ignoreWordsEdit_->setPlaceholderText(tr("e.g. acronyms, slang, or custom names..."));
    vocabLayout->addWidget(ignoreWordsEdit_);

    layout->addWidget(vocabCard);

    layout->addStretch();
    scrollArea->setWidget(container);
    return scrollArea;
}

QWidget* SettingsDialog::createDiagnosticsTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    // Accessibility Card
    auto* axCard = new QFrame(widget);
    axCard->setObjectName("card");
    auto* axLayout = new QHBoxLayout(axCard);
    axStatusLabel_ = new QLabel(axCard);
    axFixBtn_ = new QPushButton(tr("Configure..."), axCard);
    axFixBtn_->setStyleSheet(
        "QPushButton { background-color: #F1F5F9; color: #2563EB; border: 1px solid #CBD5E1; "
        "border-radius: 6px; padding: 4px 10px; font-size: 12px; font-weight: 600; }"
        "QPushButton:hover { background-color: #E2E8F0; }"
    );
    connect(axFixBtn_, &QPushButton::clicked, this, []() {
        checkAccessibilityPermission(true);
        openAccessibilitySettings();
    });
    axLayout->addWidget(axStatusLabel_, 1);
    axLayout->addWidget(axFixBtn_);
    layout->addWidget(axCard);

    // Input Monitoring Card
    auto* imCard = new QFrame(widget);
    imCard->setObjectName("card");
    auto* imLayout = new QHBoxLayout(imCard);
    imStatusLabel_ = new QLabel(imCard);
    imFixBtn_ = new QPushButton(tr("Configure..."), imCard);
    imFixBtn_->setStyleSheet(
        "QPushButton { background-color: #F1F5F9; color: #2563EB; border: 1px solid #CBD5E1; "
        "border-radius: 6px; padding: 4px 10px; font-size: 12px; font-weight: 600; }"
        "QPushButton:hover { background-color: #E2E8F0; }"
    );
    connect(imFixBtn_, &QPushButton::clicked, this, []() {
        openInputMonitoringSettings();
    });
    imLayout->addWidget(imStatusLabel_, 1);
    imLayout->addWidget(imFixBtn_);
    layout->addWidget(imCard);

    // Engine & Dictionaries Card
    auto* engCard = new QFrame(widget);
    engCard->setObjectName("card");
    auto* engLayout = new QVBoxLayout(engCard);
    engLayout->setSpacing(6);

    auto* engTitle = new QLabel(tr("Engine & Dictionaries"), engCard);
    engTitle->setStyleSheet("font-size: 14px; font-weight: 700; color: #0F172A;");
    engLayout->addWidget(engTitle);

    dictStatusLabel_ = new QLabel(engCard);
    dictStatusLabel_->setStyleSheet("font-size: 12px; color: #475569;");
    hookStatusLabel_ = new QLabel(engCard);
    hookStatusLabel_->setStyleSheet("font-size: 12px; color: #475569;");

    engLayout->addWidget(dictStatusLabel_);
    engLayout->addWidget(hookStatusLabel_);
    layout->addWidget(engCard);

    // Wizard Launcher
    auto* wizardBtn = new QPushButton(tr("Launch Permission Setup Wizard..."), widget);
    wizardBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #F8FAFC; color: #2563EB; border: 1px solid #CBD5E1;"
        "  border-radius: 6px; padding: 8px 14px; font-size: 13px; font-weight: 600;"
        "}"
        "QPushButton:hover { background-color: #F1F5F9; }"
    );
    connect(wizardBtn, &QPushButton::clicked, this, &SettingsDialog::openWizard);
    layout->addWidget(wizardBtn);

    layout->addStretch();
    refreshPermissions();
    return widget;
}

QWidget* SettingsDialog::createPlaygroundTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto* playCard = new QFrame(widget);
    playCard->setObjectName("card");
    auto* playLayout = new QVBoxLayout(playCard);
    playLayout->setSpacing(8);

    auto* playTitle = new QLabel(tr("Interactive Test Playground"), playCard);
    playTitle->setStyleSheet("font-size: 14px; font-weight: 700; color: #0F172A;");
    playLayout->addWidget(playTitle);

    auto* hint = new QLabel(
        tr("Type common typos like <code>teh </code> (space), <code>recieve </code>, <code>need helo </code>, or <code>adn </code>:"),
        playCard);
    hint->setWordWrap(true);
    hint->setStyleSheet("font-size: 12px; color: #64748B;");
    playLayout->addWidget(hint);

    auto* inputRow = new QHBoxLayout();
    testInputEdit_ = new QLineEdit(playCard);
    testInputEdit_->setPlaceholderText(tr("Type mistakes here to test instant correction..."));
    auto* clearBtn = new QPushButton(tr("Clear"), playCard);
    clearBtn->setStyleSheet(
        "QPushButton { background-color: #F1F5F9; border: 1px solid #CBD5E1; "
        "border-radius: 6px; padding: 6px 12px; font-size: 12px; }"
        "QPushButton:hover { background-color: #E2E8F0; }"
    );
    connect(clearBtn, &QPushButton::clicked, testInputEdit_, &QLineEdit::clear);
    inputRow->addWidget(testInputEdit_);
    inputRow->addWidget(clearBtn);
    playLayout->addLayout(inputRow);

    layout->addWidget(playCard);

    // Activity Stream Card
    auto* logCard = new QFrame(widget);
    logCard->setObjectName("card");
    auto* logLayout = new QVBoxLayout(logCard);
    logLayout->setSpacing(6);

    auto* logTitle = new QLabel(tr("Activity Stream"), logCard);
    logTitle->setStyleSheet("font-size: 13px; font-weight: 600; color: #334155;");
    logLayout->addWidget(logTitle);

    liveLogEdit_ = new QPlainTextEdit(logCard);
    liveLogEdit_->setReadOnly(true);
    liveLogEdit_->setMaximumHeight(150);
    liveLogEdit_->setPlaceholderText(tr("Typing corrections and buffer events will appear here..."));
    liveLogEdit_->setStyleSheet("background-color: #F8FAFC; border: 1px solid #E2E8F0; font-family: monospace; font-size: 11px;");
    logLayout->addWidget(liveLogEdit_);

    layout->addWidget(logCard);
    return widget;
}

void SettingsDialog::openWizard() {
    PermissionDialog dlg(this);
    dlg.exec();
    refreshPermissions();
}

void SettingsDialog::updateStatusInfo(bool hookRunning, size_t dictWords) {
    hookRunning_ = hookRunning;
    dictWords_ = dictWords;
    refreshPermissions();
}

void SettingsDialog::refreshPermissions() {
    bool ax = checkAccessibilityPermission(false);
    bool im = checkInputMonitoringPermission();

    if (axStatusLabel_) {
        if (ax) {
            axStatusLabel_->setText(tr("<b>Accessibility:</b> <span style='color:#166534;'>✓ Granted</span> (Key injection ready)"));
            if (axFixBtn_) axFixBtn_->hide();
        } else {
            axStatusLabel_->setText(tr("<b>Accessibility:</b> <span style='color:#DC2626;'>✖ Missing</span> (Text injection blocked)"));
            if (axFixBtn_) axFixBtn_->show();
        }
    }

    if (imStatusLabel_) {
        if (im) {
            imStatusLabel_->setText(tr("<b>Input Monitoring:</b> <span style='color:#166534;'>✓ Granted</span> (Observing keystrokes)"));
            if (imFixBtn_) imFixBtn_->hide();
        } else {
            imStatusLabel_->setText(tr("<b>Input Monitoring:</b> <span style='color:#DC2626;'>✖ Missing</span> (Keystroke hook blocked)"));
            if (imFixBtn_) imFixBtn_->show();
        }
    }

    if (dictStatusLabel_) {
        dictStatusLabel_->setText(tr("<b>Dictionaries:</b> %1 words + 242,342 bigrams active")
            .arg(dictWords_ > 0 ? dictWords_ : 82834));
    }

    if (hookStatusLabel_) {
        hookStatusLabel_->setText(tr("<b>Background Hook:</b> <span style='color:#166534;'>● Active & Listening</span> (Sub-millisecond pipeline)"));
    }
}

void SettingsDialog::appendLog(const QString& msg) {
    if (!liveLogEdit_) return;
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss");
    liveLogEdit_->appendPlainText(QString("[%1] %2").arg(timeStr, msg));
}

void SettingsDialog::loadFromConfig() {
    std::lock_guard<std::mutex> lock(configMutex);
    masterEnabledCheck_->setChecked(sharedConfig.masterEnabled);
    launchAtLoginCheck_->setChecked(sharedConfig.launchAtLogin);
    int aggPos = aggressivenessToSliderPosition(sharedConfig.correctionAggressiveness);
    aggressivenessSlider_->setValue(aggPos);
    aggressivenessLabel_->setText(QString::number(aggPos) + "%");
    maxEditDistanceSpin_->setValue(sharedConfig.maxDictionaryEditDistance);

    QString ignoreText;
    for (const auto& w : sharedConfig.ignoreWords) {
        ignoreText += QString::fromStdString(w) + "\n";
    }
    ignoreWordsEdit_->setPlainText(ignoreText.trimmed());

    sentenceCorrectionCheck_->setChecked(sharedConfig.sentenceCorrectionEnabled);
    sentenceIntensitySlider_->setValue(static_cast<int>(sharedConfig.sentenceCorrectionIntensity * 100.0f));
}

void SettingsDialog::saveToConfig() {
    {
        std::lock_guard<std::mutex> lock(configMutex);
        sharedConfig.masterEnabled = masterEnabledCheck_->isChecked();
        sharedConfig.launchAtLogin = launchAtLoginCheck_->isChecked();
        sharedConfig.correctionAggressiveness = sliderPositionToAggressiveness(aggressivenessSlider_->value());
        sharedConfig.maxDictionaryEditDistance = maxEditDistanceSpin_->value();

        sharedConfig.ignoreWords.clear();
        QStringList lines = ignoreWordsEdit_->toPlainText().split('\n', Qt::SkipEmptyParts);
        for (const auto& line : lines) {
            std::string s = line.trimmed().toStdString();
            if (!s.empty()) sharedConfig.ignoreWords.push_back(s);
        }

        sharedConfig.sentenceCorrectionEnabled = sentenceCorrectionCheck_->isChecked();
        sharedConfig.sentenceCorrectionIntensity = static_cast<float>(sentenceIntensitySlider_->value()) / 100.0f;

        std::string json = serializeConfig(sharedConfig);
        std::ofstream out(getConfigFilePath());
        if (out.is_open()) {
            out << json;
            out.close();
        }
    }

    setLaunchAtLogin(launchAtLoginCheck_->isChecked());
}
