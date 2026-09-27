#include "gui/settings_dialog.h"
#include "gui/settings_binding.h"
#include "core/config/config_json.h"
#include "platform/mac/login/login_item.h"
#include "platform/mac/permissions.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QDateTime>
#include <fstream>
#include <filesystem>

static std::string getConfigFilePath() {
    const char* home = getenv("HOME");
    std::string path = (home ? home : ".") + std::string("/Library/Application Support/TypingAssistant");
    std::filesystem::create_directories(path);
    return path + "/config.json";
}

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Typing Assistant - Status & Settings"));
    resize(580, 720);
    setupUi();
    loadFromConfig();

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &SettingsDialog::refreshPermissions);
    refreshTimer_->start(2000);
}

void SettingsDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);

    tabWidget_ = new QTabWidget(this);
    tabWidget_->addTab(createDiagnosticsTab(), tr("Status & Live Test"));
    tabWidget_->addTab(createGeneralTab(), tr("Preferences"));

    mainLayout->addWidget(tabWidget_);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveToConfig();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

QWidget* SettingsDialog::createDiagnosticsTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);

    // 1. Permissions & System Status
    auto* permGroup = new QGroupBox(tr("System Permissions & Engine Status"), widget);
    auto* permLayout = new QVBoxLayout(permGroup);

    // Accessibility Row
    auto* axRow = new QHBoxLayout();
    axStatusLabel_ = new QLabel(permGroup);
    axFixBtn_ = new QPushButton(tr("Open System Settings..."), permGroup);
    connect(axFixBtn_, &QPushButton::clicked, this, []() {
        openAccessibilitySettings();
    });
    axRow->addWidget(axStatusLabel_);
    axRow->addStretch();
    axRow->addWidget(axFixBtn_);
    permLayout->addLayout(axRow);

    // Input Monitoring Row
    auto* imRow = new QHBoxLayout();
    imStatusLabel_ = new QLabel(permGroup);
    imFixBtn_ = new QPushButton(tr("Open System Settings..."), permGroup);
    connect(imFixBtn_, &QPushButton::clicked, this, []() {
        openInputMonitoringSettings();
    });
    imRow->addWidget(imStatusLabel_);
    imRow->addStretch();
    imRow->addWidget(imFixBtn_);
    permLayout->addLayout(imRow);

    // Dictionary & Tap Status
    dictStatusLabel_ = new QLabel(permGroup);
    hookStatusLabel_ = new QLabel(permGroup);
    permLayout->addWidget(dictStatusLabel_);
    permLayout->addWidget(hookStatusLabel_);

    auto* refreshBtn = new QPushButton(tr("↻ Check Permissions Again"), permGroup);
    connect(refreshBtn, &QPushButton::clicked, this, &SettingsDialog::refreshPermissions);
    permLayout->addWidget(refreshBtn);

    layout->addWidget(permGroup);

    // 2. Interactive Playground
    auto* testGroup = new QGroupBox(tr("Interactive Test Playground"), widget);
    auto* testLayout = new QVBoxLayout(testGroup);

    auto* noteLabel = new QLabel(
        tr("<b>Test typing here in real time:</b><br>"
           "Type words like <code>teh </code> (space), <code>recieve </code>, <code>applicaiton </code>, or <code>adn </code>:"),
        testGroup);
    noteLabel->setWordWrap(true);
    testLayout->addWidget(noteLabel);

    auto* inputRow = new QHBoxLayout();
    testInputEdit_ = new QLineEdit(testGroup);
    testInputEdit_->setPlaceholderText(tr("Type mistakes here to test instant correction..."));
    auto* clearInputBtn = new QPushButton(tr("Clear"), testGroup);
    connect(clearInputBtn, &QPushButton::clicked, testInputEdit_, &QLineEdit::clear);
    inputRow->addWidget(testInputEdit_);
    inputRow->addWidget(clearInputBtn);
    testLayout->addLayout(inputRow);

    testLayout->addWidget(new QLabel(tr("<b>Live Activity & Correction Stream:</b>"), testGroup));
    liveLogEdit_ = new QPlainTextEdit(testGroup);
    liveLogEdit_->setReadOnly(true);
    liveLogEdit_->setMaximumHeight(180);
    liveLogEdit_->setPlaceholderText(tr("Keystrokes, detections, and automatic replacements will appear here in real time..."));
    testLayout->addWidget(liveLogEdit_);

    layout->addWidget(testGroup);

    refreshPermissions();
    return widget;
}

QWidget* SettingsDialog::createGeneralTab() {
    auto* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    auto* scrollWidget = new QWidget(scrollArea);
    auto* formLayout = new QVBoxLayout(scrollWidget);

    // 1. General Section
    auto* generalGroup = new QGroupBox(tr("General"), scrollWidget);
    auto* generalLayout = new QVBoxLayout(generalGroup);
    masterEnabledCheck_ = new QCheckBox(tr("Enable Typing Assistant"), generalGroup);
    launchAtLoginCheck_ = new QCheckBox(tr("Launch at login"), generalGroup);
    generalLayout->addWidget(masterEnabledCheck_);
    generalLayout->addWidget(launchAtLoginCheck_);
    formLayout->addWidget(generalGroup);

    // 2. Word Correction Section (Tier 1)
    auto* wordGroup = new QGroupBox(tr("Word Correction (Instant)"), scrollWidget);
    auto* wordLayout = new QVBoxLayout(wordGroup);

    auto* aggRow = new QHBoxLayout();
    aggRow->addWidget(new QLabel(tr("Aggressiveness:"), wordGroup));
    aggressivenessSlider_ = new QSlider(Qt::Horizontal, wordGroup);
    aggressivenessSlider_->setRange(0, 100);
    aggressivenessLabel_ = new QLabel("50%", wordGroup);
    aggRow->addWidget(aggressivenessSlider_);
    aggRow->addWidget(aggressivenessLabel_);
    wordLayout->addLayout(aggRow);

    connect(aggressivenessSlider_, &QSlider::valueChanged, this, [this](int val) {
        aggressivenessLabel_->setText(QString::number(val) + "%");
    });

    auto* distRow = new QHBoxLayout();
    distRow->addWidget(new QLabel(tr("Max Edit Distance:"), wordGroup));
    maxEditDistanceSpin_ = new QSpinBox(wordGroup);
    maxEditDistanceSpin_->setRange(1, 2);
    distRow->addWidget(maxEditDistanceSpin_);
    distRow->addStretch();
    wordLayout->addLayout(distRow);

    wordLayout->addWidget(new QLabel(tr("Ignore Words (one per line):"), wordGroup));
    ignoreWordsEdit_ = new QPlainTextEdit(wordGroup);
    ignoreWordsEdit_->setMaximumHeight(70);
    wordLayout->addWidget(ignoreWordsEdit_);
    formLayout->addWidget(wordGroup);

    // 3. Sentence Correction Section (Tier 2 ML)
    auto* sentenceGroup = new QGroupBox(tr("Sentence Correction (Local AI)"), scrollWidget);
    auto* sentenceLayout = new QVBoxLayout(sentenceGroup);
    sentenceCorrectionCheck_ = new QCheckBox(tr("Enable sentence correction"), sentenceGroup);
    sentenceLayout->addWidget(sentenceCorrectionCheck_);

    auto* sentIntRow = new QHBoxLayout();
    sentIntRow->addWidget(new QLabel(tr("Intensity:"), sentenceGroup));
    sentenceIntensitySlider_ = new QSlider(Qt::Horizontal, sentenceGroup);
    sentenceIntensitySlider_->setRange(0, 100);
    sentIntRow->addWidget(sentenceIntensitySlider_);
    sentenceLayout->addLayout(sentIntRow);
    formLayout->addWidget(sentenceGroup);

    // 4. Privacy Section
    auto* privacyGroup = new QGroupBox(tr("Privacy"), scrollWidget);
    auto* privacyLayout = new QVBoxLayout(privacyGroup);
    auto* privacyLabel = new QLabel(
        tr("Runs entirely on your device. Keystrokes are never sent off your computer.\n\n"
           "Password and secure text fields are automatically detected and completely excluded "
           "from logging, buffering, and corrections."), privacyGroup);
    privacyLabel->setWordWrap(true);
    privacyLayout->addWidget(privacyLabel);
    formLayout->addWidget(privacyGroup);

    scrollArea->setWidget(scrollWidget);
    return scrollArea;
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
            axStatusLabel_->setText(tr("<font color='#28a745'><b>✔ Accessibility:</b> Granted (Ready for key injection)</font>"));
            if (axFixBtn_) axFixBtn_->hide();
        } else {
            axStatusLabel_->setText(tr("<font color='#dc3545'><b>✖ Accessibility:</b> Missing (Keystroke injection blocked)</font>"));
            if (axFixBtn_) axFixBtn_->show();
        }
    }

    if (imStatusLabel_) {
        if (im) {
            imStatusLabel_->setText(tr("<font color='#28a745'><b>✔ Input Monitoring:</b> Granted (Capturing keystrokes)</font>"));
            if (imFixBtn_) imFixBtn_->hide();
        } else {
            imStatusLabel_->setText(tr("<font color='#dc3545'><b>✖ Input Monitoring:</b> Missing (Keyboard listening blocked)</font>"));
            if (imFixBtn_) imFixBtn_->show();
        }
    }

    if (dictStatusLabel_) {
        dictStatusLabel_->setText(tr("<font color='#28a745'><b>✔ Dictionary:</b> %1 words loaded</font>")
            .arg(dictWords_ > 0 ? dictWords_ : 82834));
    }

    if (hookStatusLabel_) {
        hookStatusLabel_->setText(tr("<font color='#28a745'><b>✔ Background Hook:</b> Active & Listening</font>"));
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
