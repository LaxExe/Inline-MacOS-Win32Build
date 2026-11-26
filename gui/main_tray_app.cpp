#include "gui/main_tray_app.h"
#include "gui/permission_dialog.h"
#include "gui/tray_state.h"
#include "core/buffers/buffer_gate.h"
#include "core/buffers/word_boundary.h"
#include "core/tier1/corrector.h"
#include "core/ghost/ghost_layout.h"
#include "core/config/config_json.h"
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
#include "platform/mac/hotkey/global_hotkey.h"
#include "platform/mac/permissions.h"
#include <Carbon/Carbon.h>
#include <pthread/qos.h>
#else
#include "platform/win/hotkey/global_hotkey.h"
#endif
#include <QApplication>
#include <QStyleHints>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QFontMetricsF>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <unistd.h>
#include <iostream>

static QIcon createStatusIcon(bool active, bool isDark = false) {
    // 36x36 px @ 2x = 18x18 pt Retina display tray icon
    QPixmap pixmap(36, 36);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(2.0);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // On dark menu bar/black screen: vertical line is white (#FFFFFF).
    // On light menu bar/light screen: vertical line is black (#18181B).
    QColor barColor = isDark ? QColor(255, 255, 255) : QColor(24, 24, 27);
    QColor dotColor = active ? QColor(34, 197, 94) : QColor(245, 158, 11);

    painter.setPen(Qt::NoPen);

    // Vertical line (I) - sleek 1.4pt width (reduced from 2.2pt)
    painter.setBrush(barColor);
    painter.drawRoundedRect(QRectF(5.5, 3.0, 1.4, 12.0), 0.5, 0.5);

    // Precision status dot next to it near bottom
    painter.setBrush(dotColor);
    painter.drawRoundedRect(QRectF(8.8, 9.8, 3.0, 3.0), 0.6, 0.6);

    return QIcon(pixmap);
}


MainTrayApp::MainTrayApp(QObject* parent) : QObject(parent) {
    currentWord_.chars.reserve(64);
    lastKeyTimeNs_.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_relaxed);
    idleClearTimer_ = new QTimer(this);
    idleClearTimer_->setInterval(1000); // 1s interval for idle check and background contrast check
    connect(idleClearTimer_, &QTimer::timeout, this, &MainTrayApp::onIdleCheck);
    idleClearTimer_->start();

    setupTray();
}

MainTrayApp::~MainTrayApp() {
    ghostWorkerRunning_.store(false, std::memory_order_relaxed);
    ghostWorkerCv_.notify_all();
    llmWorkerCv_.notify_all();
    if (ghostWorkerThread_.joinable()) {
        ghostWorkerThread_.join();
    }
    if (llmWorkerThread_.joinable()) {
        llmWorkerThread_.join();
    }
    dismissGhost();
    clearAllBuffers();
    hook_.stop();
    injector_.stop();
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    predictorClient_.shutdownHelper();
#endif
}

void MainTrayApp::dismissGhost() {
    ghostSession_.hide();
    lastRenderedCaretX_.store(0.0, std::memory_order_relaxed);
    lastRenderedCaretY_.store(0.0, std::memory_order_relaxed);
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    ghostOverlay_.hide();
#endif
}

void MainTrayApp::clearAllBuffers() {
    dismissGhost();
    ghostSession_.clearRejected();
    clearRequested_.store(true, std::memory_order_release);
    hasBufferedData_.store(false, std::memory_order_relaxed);
    hook_.clearQueue();
    injector_.clearQueue();

    if (settingsDialog_ && settingsDialog_->isVisible()) {
        QString msg = QString("[BUFFER_PURGE] Keystroke buffers, history, and queues flushed.");
        QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
    }
}

void MainTrayApp::onIdleCheck() {
    std::string showTriggerPath = "/tmp/com.inline.mac.show";
    if (std::filesystem::exists(showTriggerPath)) {
        std::error_code ec;
        std::filesystem::remove(showTriggerPath, ec);
        openPreferences();
    }

    std::string walkTriggerPath = "/tmp/com.inline.mac.walkthrough";
    if (std::filesystem::exists(walkTriggerPath)) {
        std::error_code ec;
        std::filesystem::remove(walkTriggerPath, ec);
        openWalkthrough();
    }

    bool isDark = false;
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    isDark = isSystemInDarkMode();
    if (isLiveEventTapAttached() && !hook_.isActiveTap() && checkAccessibilityPermission(false)) {
        hook_.stop();
        hook_.start();
    }
#else
    isDark = QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
#endif
    if (isDark != lastKnownDarkMode_) {
        lastKnownDarkMode_ = isDark;
        updateTrayIcon();
    }

    if (settingsDialog_ && settingsDialog_->isVisible()) {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
        settingsDialog_->updatePredictorInfo(
            predictorClient_.isModelAvailable(),
            predictorClient_.isHelperRunning(),
            predictorClient_.isCircuitTripped(),
            PowerMonitor::isLowPowerModeEnabled(),
            predictorClient_.lastLatencyMs()
        );
#endif
    }

    if (!hasBufferedData_.load(std::memory_order_relaxed)) return;
    int64_t nowNs = std::chrono::steady_clock::now().time_since_epoch().count();
    int64_t lastNs = lastKeyTimeNs_.load(std::memory_order_relaxed);
    constexpr int64_t kThirtySecondsNs = 30LL * 1000000000LL;
    if ((nowNs - lastNs) >= kThirtySecondsNs) {
        dismissGhost();
        hasBufferedData_.store(false, std::memory_order_relaxed);
        clearRequested_.store(true, std::memory_order_release);
        hook_.clearQueue();
        injector_.clearQueue();
        if (settingsDialog_ && settingsDialog_->isVisible()) {
            QString msg = QString("[IDLE_TIMEOUT] Inactivity elapsed (>30s). Buffer purged.");
            QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
        }
    }
}

void MainTrayApp::onIdleTimeout() {
    clearAllBuffers();
}

void MainTrayApp::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    (void)reason;
    clearAllBuffers();
}

void MainTrayApp::setupTray() {
    trayIcon_ = new QSystemTrayIcon(this);
    trayMenu_ = new QMenu();

    statusAction_ = trayMenu_->addAction(tr("Inline: Active"));
    statusAction_->setEnabled(false);

    trayMenu_->addSeparator();

    pauseAction_ = trayMenu_->addAction(tr("Pause Autocorrection"), this, &MainTrayApp::toggleMasterEnabled);
    sentenceToggleAction_ = trayMenu_->addAction(tr("Sentence Capitalization: Active"), this, &MainTrayApp::toggleSentenceCorrection);
    ghostToggleAction_ = trayMenu_->addAction(tr("Ghost Text Predictions (Beta): Inactive"), this, &MainTrayApp::toggleGhostText);
    addWordAction_ = trayMenu_->addAction(tr("Add Highlighted Word to Known Words (Cmd+Opt+Shift+D)"), this, &MainTrayApp::addSelectedWordToKnownWords);
    rebindAddWordHotkey();
    clearAction_ = trayMenu_->addAction(tr("Purge Keystroke Buffer"), this, &MainTrayApp::clearAllBuffers);

    trayMenu_->addSeparator();

    walkthroughAction_ = trayMenu_->addAction(tr("Feature Walkthrough..."), this, &MainTrayApp::openWalkthrough);
    permAction_ = trayMenu_->addAction(tr("System Permissions..."), this, &MainTrayApp::openPermissions);
    prefsAction_ = trayMenu_->addAction(tr("System Settings..."), this, &MainTrayApp::openPreferences);
    quitAction_ = trayMenu_->addAction(tr("Quit Inline"), this, &MainTrayApp::quitApp);

    trayIcon_->setContextMenu(trayMenu_);
    connect(trayIcon_, &QSystemTrayIcon::activated, this, &MainTrayApp::onTrayActivated);
    connect(trayMenu_, &QMenu::aboutToShow, this, &MainTrayApp::clearAllBuffers);

    if (QGuiApplication::styleHints()) {
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
            updateTrayIcon();
        });
    }

#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    registerAppearanceNotifications([this]() {
        QMetaObject::invokeMethod(this, [this]() {
            updateTrayIcon();
        }, Qt::QueuedConnection);
    });
    registerAppReopenCallback([this]() {
        QMetaObject::invokeMethod(this, [this]() {
            openPreferences();
        }, Qt::QueuedConnection);
    });
    registerPassiveCaretResetMonitors([this]() {
        dismissGhost();
        clearRequested_.store(true, std::memory_order_release);
    });
#endif

    updateTrayIcon();
    trayIcon_->setVisible(true);
    trayIcon_->show();

#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    // Hook native status bar item after window server populates it
    QTimer::singleShot(50, this, [this]() {
        updateTrayIcon();
    });
#endif
}

void MainTrayApp::updateTrayIcon() {
    bool enabled = false;
    bool sentenceEnabled = false;
    bool ghostEnabled = false;
    int appTheme = 0;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        enabled = sharedConfig.masterEnabled;
        sentenceEnabled = sharedConfig.sentenceCorrectionEnabled;
        ghostEnabled = sharedConfig.ghostTextEnabled;
        appTheme = sharedConfig.appTheme;
    }

    if (!enabled || !ghostEnabled) {
        dismissGhost();
    }

    bool isMenuDark = false;
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    // On macOS, the menu bar appearance is determined by the system desktop/menu bar style,
    // not by the application window's theme selection.
    isMenuDark = isSystemInDarkMode();
#else
    if (appTheme == 1) {
        isMenuDark = false; // Forced Light
    } else if (appTheme == 2) {
        isMenuDark = true;  // Forced Dark
    } else {
        isMenuDark = QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
    }
#endif

    TrayIconState state = iconStateFor(enabled);
    trayIcon_->setIcon(createStatusIcon(state == TrayIconState::Active, isMenuDark));
    trayIcon_->setVisible(true);

#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    updateNativeStatusBarIcon(state == TrayIconState::Active);
#endif

    if (enabled) {
        statusAction_->setText(tr("Inline: Active"));
        pauseAction_->setText(tr("Pause Autocorrection"));
    } else {
        statusAction_->setText(tr("Inline: Paused"));
        pauseAction_->setText(tr("Resume Autocorrection"));
    }

    sentenceToggleAction_->setText(sentenceEnabled ? tr("Sentence Capitalization: Active") : tr("Sentence Capitalization: Inactive"));
    if (ghostToggleAction_) {
        ghostToggleAction_->setText(ghostEnabled ? tr("Ghost Text Predictions (Beta): Active") : tr("Ghost Text Predictions (Beta): Inactive"));
    }
}

void MainTrayApp::toggleMasterEnabled() {
    AppConfig cfgCopy;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        sharedConfig.masterEnabled = !sharedConfig.masterEnabled;
        cfgCopy = sharedConfig;
    }
    saveConfigToFile(cfgCopy);
    updateTrayIcon();
    if (settingsDialog_ && settingsDialog_->isVisible()) {
        settingsDialog_->loadFromConfig();
    }
}

void MainTrayApp::toggleSentenceCorrection() {
    AppConfig cfgCopy;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        sharedConfig.sentenceCorrectionEnabled = !sharedConfig.sentenceCorrectionEnabled;
        cfgCopy = sharedConfig;
    }
    saveConfigToFile(cfgCopy);
    updateTrayIcon();
    if (settingsDialog_ && settingsDialog_->isVisible()) {
        settingsDialog_->loadFromConfig();
    }
}

void MainTrayApp::toggleGhostText() {
    bool nowEnabled = false;
    AppConfig cfgCopy;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        sharedConfig.ghostTextEnabled = !sharedConfig.ghostTextEnabled;
        nowEnabled = sharedConfig.ghostTextEnabled;
        cfgCopy = sharedConfig;
    }
    saveConfigToFile(cfgCopy);
    if (!nowEnabled) {
        dismissGhost();
    } else {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
        if (cfgCopy.smartPredictionsEnabled && !predictorClient_.isHelperRunning()) {
            std::thread([this]() {
                (void)predictorClient_.predict(0, L"Hello ", L"", 1);
            }).detach();
        }
#endif
    }
    updateTrayIcon();
    if (settingsDialog_ && settingsDialog_->isVisible()) {
        settingsDialog_->loadFromConfig();
    }
}

void MainTrayApp::addSelectedWordToKnownWords() {
    std::string rawSelected;
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    rawSelected = getSelectedTextFromFrontmostApp();
#endif

    // Trim whitespace and outer punctuation
    auto trimWord = [](const std::string& str) -> std::string {
        if (str.empty()) return "";
        size_t start = 0;
        while (start < str.size() && (std::isspace(static_cast<unsigned char>(str[start])) ||
               str[start] == '"' || str[start] == '\'' || str[start] == '`' ||
               str[start] == '(' || str[start] == '[' || str[start] == '{' ||
               str[start] == '<' || str[start] == '.' || str[start] == ',' ||
               str[start] == ';' || str[start] == ':' || str[start] == '!' ||
               str[start] == '?')) {
            start++;
        }
        if (start >= str.size()) return "";
        size_t end = str.size() - 1;
        while (end > start && (std::isspace(static_cast<unsigned char>(str[end])) ||
               str[end] == '"' || str[end] == '\'' || str[end] == '`' ||
               str[end] == ')' || str[end] == ']' || str[end] == '}' ||
               str[end] == '>' || str[end] == '.' || str[end] == ',' ||
               str[end] == ';' || str[end] == ':' || str[end] == '!' ||
               str[end] == '?')) {
            end--;
        }
        return str.substr(start, end - start + 1);
    };

    std::string selected = trimWord(rawSelected);
    std::wstring lastOrig = undoSM_.getLastOriginal();
    std::wstring lastCorr = undoSM_.getLastCorrected();

    std::string wordToAdd;
    bool shouldRevertInApp = false;

    // Check if the user highlighted the autocorrected word (e.g. highlighted "BIM" after "IBM" -> "BIM")
    if (!selected.empty() && !lastCorr.empty() && !lastOrig.empty()) {
        std::string lowerSelected = selected;
        for (char& c : lowerSelected) c = std::tolower(static_cast<unsigned char>(c));

        std::string lowerCorr = QString::fromStdWString(lastCorr).toLower().toStdString();
        if (lowerSelected == lowerCorr) {
            wordToAdd = QString::fromStdWString(lastOrig).toStdString();
            shouldRevertInApp = true;
        }
    }

    if (wordToAdd.empty()) {
        if (!selected.empty()) {
            wordToAdd = selected;
        } else if (undoSM_.isEligibleForUndo() && !lastOrig.empty()) {
            // Nothing highlighted, but an autocorrection just occurred: use last original word
            wordToAdd = QString::fromStdWString(lastOrig).toStdString();
            shouldRevertInApp = true;
        }
    }

    if (wordToAdd.empty()) {
        if (trayIcon_) {
            trayIcon_->showMessage(
                tr("Add to Known Words"),
                tr("Please highlight a word first, then press Cmd+Option+Shift+D."),
                QSystemTrayIcon::Information,
                3000
            );
        }
        return;
    }

    // If we detected that the word in the editor is the autocorrected version, revert it back to the original
    if (shouldRevertInApp) {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
        if (!selected.empty()) {
            replaceSelectedTextInFrontmostApp(wordToAdd);
        } else {
            handleBackspace(true);
        }
#endif
    }

    // Split in case user selected multiple words or comma-separated terms
    QStringList tokens = QString::fromStdString(wordToAdd).split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
    if (tokens.isEmpty()) {
        tokens.append(QString::fromStdString(wordToAdd));
    }

    std::vector<std::string> newlyAdded;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        for (const QString& tok : tokens) {
            std::string w = tok.trimmed().toStdString();
            if (w.empty()) continue;

            // Case-insensitive duplicate check
            std::string lowerW = w;
            for (char& c : lowerW) c = std::tolower(static_cast<unsigned char>(c));

            bool exists = false;
            for (const auto& existing : sharedConfig.ignoreWords) {
                std::string lowerEx = existing;
                for (char& c : lowerEx) c = std::tolower(static_cast<unsigned char>(c));
                if (lowerEx == lowerW) {
                    exists = true;
                    break;
                }
            }

            if (!exists) {
                sharedConfig.ignoreWords.push_back(w);
                newlyAdded.push_back(w);
            }
        }
    }

    if (!newlyAdded.empty()) {
        AppConfig cfgCopy;
        {
            std::lock_guard<std::mutex> lock(configMutex);
            cfgCopy = sharedConfig;
            cachedConfig_ = sharedConfig;
        }
        saveConfigToFile(cfgCopy);

        // Also index into SymSpell dictionary so it's a known valid word
        for (const auto& w : newlyAdded) {
            std::wstring ww = QString::fromStdString(w).toLower().toStdWString();
            dict_.addWord(ww, 1000000);
        }

        if (settingsDialog_ && settingsDialog_->isVisible()) {
            settingsDialog_->loadFromConfig();
        }

        QString addedList;
        for (size_t i = 0; i < newlyAdded.size(); ++i) {
            if (i > 0) addedList += ", ";
            addedList += QString("\"%1\"").arg(QString::fromStdString(newlyAdded[i]));
        }

        QString logMsg = QString("[KNOWN_WORDS] Added %1 to known words list. Autocorrection stopped.").arg(addedList);
        PrivacyLogger::instance().logDiagnosticText(logMsg.toStdString());
        if (settingsDialog_ && settingsDialog_->isVisible()) {
            QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, logMsg));
        }

        if (trayIcon_) {
            trayIcon_->showMessage(
                tr("Added to Known Words"),
                tr("%1 added to known words. Inline will no longer autocorrect this.").arg(addedList),
                QSystemTrayIcon::Information,
                3500
            );
        }
    } else {
        // Already in known words
        QString msg = QString("[KNOWN_WORDS] \"%1\" is already in the known words list.").arg(QString::fromStdString(wordToAdd));
        PrivacyLogger::instance().logDiagnosticText(msg.toStdString());
        if (settingsDialog_ && settingsDialog_->isVisible()) {
            QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
        }
        if (trayIcon_) {
            trayIcon_->showMessage(
                tr("Known Words"),
                tr("\"%1\" is already in your known words dictionary.").arg(QString::fromStdString(wordToAdd)),
                QSystemTrayIcon::Information,
                3000
            );
        }
    }
}

void MainTrayApp::openWalkthrough() {
    dismissGhost();
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    bringAppToFront();
#endif
    PermissionDialog dlg(nullptr, PermissionDialog::kHowItWorksSlide);
    dlg.show();
    dlg.raise();
    dlg.activateWindow();
    dlg.exec();
    updateTrayIcon();
    if (settingsDialog_ && settingsDialog_->isVisible()) {
        settingsDialog_->loadFromConfig();
        settingsDialog_->refreshPermissions();
    }
}

void MainTrayApp::openPermissions() {
    dismissGhost();
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    bringAppToFront();
#endif
    PermissionDialog dlg(nullptr, PermissionDialog::kPermissionSlide);
    dlg.show();
    dlg.raise();
    dlg.activateWindow();
    dlg.exec();
    if (settingsDialog_ && settingsDialog_->isVisible()) {
        settingsDialog_->refreshPermissions();
    }
}

void MainTrayApp::openPreferences() {
    dismissGhost();
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    bringAppToFront();
#endif
    if (!settingsDialog_) {
        settingsDialog_ = std::make_unique<SettingsDialog>();
        connect(settingsDialog_.get(), &SettingsDialog::configChanged, this, [this]() {
            updateTrayIcon();
            rebindAddWordHotkey();
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
            bool smartOn = true;
            {
                std::lock_guard<std::mutex> lock(configMutex);
                smartOn = sharedConfig.smartPredictionsEnabled && sharedConfig.ghostTextEnabled && sharedConfig.masterEnabled;
            }
            if (!smartOn) {
                predictorClient_.shutdownHelper();
            }
#endif
        });
    }
    settingsDialog_->updateStatusInfo(hook_.isRunning(), static_cast<int>(dict_.wordCount()));
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    settingsDialog_->updatePredictorInfo(
        predictorClient_.isModelAvailable(),
        predictorClient_.isHelperRunning(),
        predictorClient_.isCircuitTripped(),
        PowerMonitor::isLowPowerModeEnabled(),
        predictorClient_.lastLatencyMs()
    );
#endif
    settingsDialog_->loadFromConfig();
    settingsDialog_->show();
    settingsDialog_->raise();
    settingsDialog_->activateWindow();
}

void MainTrayApp::quitApp() {
    ghostWorkerRunning_.store(false, std::memory_order_relaxed);
    ghostWorkerCv_.notify_all();
    llmWorkerCv_.notify_all();
    if (ghostWorkerThread_.joinable()) {
        ghostWorkerThread_.join();
    }
    if (llmWorkerThread_.joinable()) {
        llmWorkerThread_.join();
    }
    dismissGhost();
    clearAllBuffers();
    hook_.stop();
    injector_.stop();
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    predictorClient_.shutdownHelper();
#endif
    QApplication::quit();
}

bool MainTrayApp::initEngine() {
    std::string dictPath = "dictionaries/frequency_dictionary_en_82_765.txt";
    std::string bigramPath = "dictionaries/frequency_bigramdictionary_en_243_342.txt";
    std::string appDir = QCoreApplication::applicationDirPath().toStdString();
    if (std::filesystem::exists(appDir + "/../Resources/dictionaries/frequency_dictionary_en_82_765.txt")) {
        dictPath = appDir + "/../Resources/dictionaries/frequency_dictionary_en_82_765.txt";
    } else if (std::filesystem::exists(appDir + "/dictionaries/frequency_dictionary_en_82_765.txt")) {
        dictPath = appDir + "/dictionaries/frequency_dictionary_en_82_765.txt";
    }

    if (std::filesystem::exists(appDir + "/../Resources/dictionaries/frequency_bigramdictionary_en_243_342.txt")) {
        bigramPath = appDir + "/../Resources/dictionaries/frequency_bigramdictionary_en_243_342.txt";
    } else if (std::filesystem::exists(appDir + "/dictionaries/frequency_bigramdictionary_en_243_342.txt")) {
        bigramPath = appDir + "/dictionaries/frequency_bigramdictionary_en_243_342.txt";
    }

    bool d1 = dict_.loadDictionary(dictPath, 0, 1);
    if (!d1) {
        PrivacyLogger::instance().logDiagnosticText("Warning: Failed to load frequency dictionary");
        std::cerr << "[TypingAssistant] Warning: Failed to load frequency dictionary from: " << dictPath << std::endl;
    }

    bool d2 = dict_.loadBigramDictionary(bigramPath);
    if (!d2) {
        PrivacyLogger::instance().logDiagnosticText("Warning: Failed to load bigram dictionary");
        std::cerr << "[TypingAssistant] Warning: Failed to load bigram dictionary from: " << bigramPath << std::endl;
    }

    ngramPredictor_ = std::make_unique<NgramPredictor>(dict_);

    {
        std::lock_guard<std::mutex> lock(configMutex);
        cachedConfig_ = sharedConfig;
    }

    ghostWorkerRunning_.store(true, std::memory_order_relaxed);
    ghostWorkerThread_ = std::thread(&MainTrayApp::ghostWorkerLoop, this);
    llmWorkerThread_ = std::thread(&MainTrayApp::llmWorkerLoop, this);

    hook_.setKeyFilterCallback([this](const RawKeyEvent& ev, bool isActiveTap) {
        return this->onRawKeyEvent(ev, isActiveTap);
    });
    hook_.setKeyCallback([this](const RawKeyEvent& ev) {
        this->onRawKeyEvent(ev, false);
    });

    injector_.start();
    hook_.start();

    // Register default global hotkey Cmd+Opt+Shift+K for Autocorrect
    GlobalHotkeyManager::instance().registerHotkey(
        KeyCode::K, Mod::Cmd | Mod::Option | Mod::Shift, [this]() {
            QMetaObject::invokeMethod(this, &MainTrayApp::toggleMasterEnabled, Qt::QueuedConnection);
        }
    );

    // Register global hotkey Cmd+Opt+Shift+G for Ghost Text Predictions
    GlobalHotkeyManager::instance().registerHotkey(
        KeyCode::G, Mod::Cmd | Mod::Option | Mod::Shift, [this]() {
            QMetaObject::invokeMethod(this, &MainTrayApp::toggleGhostText, Qt::QueuedConnection);
        }
    );

    // Register customized global hotkey to add highlighted word to known words (ignore list)
    rebindAddWordHotkey();

    return true;
}

void MainTrayApp::rebindAddWordHotkey() {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    if (addWordHotkeyId_ != 0) {
        GlobalHotkeyManager::instance().unregisterHotkey(addWordHotkeyId_);
        addWordHotkeyId_ = 0;
    }
    if (addWordAliasHotkeyId_ != 0) {
        GlobalHotkeyManager::instance().unregisterHotkey(addWordAliasHotkeyId_);
        addWordAliasHotkeyId_ = 0;
    }

    int idx = 0;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        idx = sharedConfig.addWordHotkeyIndex;
    }

    KeyCode key = KeyCode::D;
    ModifierFlags mods = Mod::Cmd | Mod::Option | Mod::Shift;
    QString keyText = "Cmd+Opt+Shift+D";

    switch (idx) {
        case 1:
            key = KeyCode::I;
            mods = Mod::Cmd | Mod::Option | Mod::Shift;
            keyText = "Cmd+Opt+Shift+I";
            break;
        case 2:
            key = KeyCode::D;
            mods = Mod::Cmd | Mod::Shift;
            keyText = "Cmd+Shift+D";
            break;
        case 3:
            key = KeyCode::D;
            mods = Mod::Option | Mod::Shift;
            keyText = "Opt+Shift+D";
            break;
        case 4:
            key = KeyCode::D;
            mods = Mod::Control | Mod::Option;
            keyText = "Ctrl+Opt+D";
            break;
        case 5:
            key = KeyCode::D;
            mods = Mod::Cmd | Mod::Option;
            keyText = "Cmd+Opt+D";
            break;
        case 0:
        default:
            key = KeyCode::D;
            mods = Mod::Cmd | Mod::Option | Mod::Shift;
            keyText = "Cmd+Opt+Shift+D";
            break;
    }

    addWordHotkeyId_ = GlobalHotkeyManager::instance().registerHotkey(
        key, mods, [this]() {
            QMetaObject::invokeMethod(this, &MainTrayApp::addSelectedWordToKnownWords, Qt::QueuedConnection);
        }
    );

    // Also register Cmd+Opt+Shift+I as alias if user selected default Cmd+Opt+Shift+D
    if (idx == 0) {
        addWordAliasHotkeyId_ = GlobalHotkeyManager::instance().registerHotkey(
            KeyCode::I, Mod::Cmd | Mod::Option | Mod::Shift, [this]() {
                QMetaObject::invokeMethod(this, &MainTrayApp::addSelectedWordToKnownWords, Qt::QueuedConnection);
            }
        );
    }

    if (addWordAction_) {
        addWordAction_->setText(tr("Add Highlighted Word to Known Words (%1)").arg(keyText));
    }
#endif
}

void MainTrayApp::appendDocumentHistory(const std::wstring& text) {
    if (text.empty()) return;
    documentHistory_ += text;
    constexpr size_t kMaxHistoryChars = 1500;
    if (documentHistory_.size() > kMaxHistoryChars) {
        documentHistory_.erase(0, documentHistory_.size() - kMaxHistoryChars);
    }
}

void MainTrayApp::popDocumentHistory(size_t count) {
    if (count == 0 || documentHistory_.empty()) return;
    if (count >= documentHistory_.size()) {
        documentHistory_.clear();
    } else {
        documentHistory_.erase(documentHistory_.size() - count);
    }
}

void MainTrayApp::scheduleGhostPrediction(bool typeThroughOnly, const std::wstring& typeThroughText, int settleMs, double minExpectedX, bool afterAcceptChain) {
    if (!cachedConfig_.masterEnabled || !cachedConfig_.ghostTextEnabled) {
        return;
    }
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    if (isFrontmostAppExcluded(cachedConfig_.ghostDisabledApps)) {
        dismissGhost();
        return;
    }
#endif

    GhostWorkerRequest req;
    req.generation = ghostSession_.currentGeneration();
    req.contextWords.assign(contextWindow_.words.begin(), contextWindow_.words.end());
    req.partialWord = currentWord_.chars;
    req.documentHistory = documentHistory_;
    req.typeThroughRemaining = typeThroughText;
    req.isSentenceStart = isSentenceStart_;
    req.typeThroughOnly = typeThroughOnly;
    req.settleMs = settleMs;
    req.config = cachedConfig_;
    req.currentWordFromGhost = currentWordFromGhost_;
    req.afterAcceptChain = afterAcceptChain;
    req.minExpectedCaretX = minExpectedX;
    req.lastCaretY = lastRenderedCaretY_.load(std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(ghostWorkerMutex_);
        pendingGhostReq_ = std::move(req);
        hasPendingGhostReq_ = true;
    }
    ghostWorkerCv_.notify_one();
}

std::optional<CaretGeometry> MainTrayApp::queryCaretGeometryWithSync(uint64_t expectedGeneration, const std::wstring& partialWord, bool afterSpace, double minExpectedX, double lastCaretY) {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    // Query AXCaretTracker immediately (zero initial sleep) and verify textTail / caret monotonicity,
    // aborting immediately if a newer keystroke bumped the generation.
    wchar_t expectedLast = L'\0';
    if (!partialWord.empty()) {
        expectedLast = static_cast<wchar_t>(std::towlower(partialWord.back()));
    } else if (afterSpace) {
        expectedLast = L' ';
    }

    std::optional<CaretGeometry> geom;
    for (int attempt = 0; attempt < 4; ++attempt) {
        if (expectedGeneration != ghostSession_.currentGeneration()) {
            return std::nullopt;
        }
        geom = caretTracker_.getCaretGeometry();
        if (expectedGeneration != ghostSession_.currentGeneration()) {
            return std::nullopt;
        }
        if (!geom) {
            if (attempt < 3) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            return std::nullopt;
        }

        // Caret forward monotonicity: if typing on the same line, and the host app reports
        // a caret position significantly to the left of minExpectedX, it has NOT processed the keystroke yet!
        bool isSameLine = geom->isSingleLine || (lastCaretY > 0.0 && std::abs(geom->caret.y - lastCaretY) < 12.0);
        if (isSameLine && minExpectedX > 0.0 && geom->caret.x + 1.2 < minExpectedX - 2.0) {
            int sleepMs = 2 + attempt;
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            continue;
        }

        if (expectedLast == L'\0') {
            break;
        }

        if (geom->textTail.empty()) {
            if (!isSameLine || minExpectedX <= 0.0 || geom->caret.x + 1.2 >= minExpectedX - 2.0) {
                break;
            }
            int sleepMs = 2 + attempt;
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            continue;
        }

        wchar_t actualLast = static_cast<wchar_t>(std::towlower(geom->textTail.back()));
        if (actualLast == expectedLast) {
            break; // Host app has processed the latest keystroke and updated its cursor!
        }

        int sleepMs = 2 + attempt; // 2ms, 3ms, 4ms, 5ms
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }

    if (!geom || expectedGeneration != ghostSession_.currentGeneration()) return std::nullopt;

    // Clamp caret.x so it NEVER moves backwards behind minExpectedX during typing on the same line!
    bool isSameLine = geom->isSingleLine || (lastCaretY > 0.0 && std::abs(geom->caret.y - lastCaretY) < 12.0);
    if (isSameLine && minExpectedX > 0.0 && geom->caret.x + 1.2 < minExpectedX) {
        geom->caret.x = minExpectedX - 1.2;
    }

    return geom;
#else
    (void)expectedGeneration;
    (void)partialWord;
    (void)afterSpace;
    (void)minExpectedX;
    (void)lastCaretY;
    return std::nullopt;
#endif
}

void MainTrayApp::ghostWorkerLoop() {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif

    while (ghostWorkerRunning_.load(std::memory_order_relaxed)) {
        GhostWorkerRequest req;
        {
            std::unique_lock<std::mutex> lock(ghostWorkerMutex_);
            ghostWorkerCv_.wait(lock, [this]() {
                return !ghostWorkerRunning_.load(std::memory_order_relaxed) || hasPendingGhostReq_;
            });
            if (!ghostWorkerRunning_.load(std::memory_order_relaxed)) break;
            req = std::move(pendingGhostReq_);
            hasPendingGhostReq_ = false;
        }

#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
        // Interruptible settle wait: wakes immediately if a newer keystroke arrives
        if (req.settleMs > 0) {
            std::unique_lock<std::mutex> lock(ghostWorkerMutex_);
            ghostWorkerCv_.wait_for(lock, std::chrono::milliseconds(req.settleMs), [this, &req]() {
                return !ghostWorkerRunning_.load(std::memory_order_relaxed) ||
                       hasPendingGhostReq_ ||
                       req.generation != ghostSession_.currentGeneration();
            });
            if (!ghostWorkerRunning_.load(std::memory_order_relaxed)) break;
            if (hasPendingGhostReq_ || req.generation != ghostSession_.currentGeneration()) {
                continue;
            }
        }

        if (!req.config.masterEnabled || !req.config.ghostTextEnabled ||
            IsSecureEventInputEnabled() ||
            isFrontmostAppExcluded(req.config.disabledApps) ||
            isFrontmostAppExcluded(req.config.ghostDisabledApps)) {
            dismissGhost();
            continue;
        }

        const bool isMidWord = !req.partialWord.empty() && !req.currentWordFromGhost;
        const bool afterSpace = (req.partialWord.empty() || req.currentWordFromGhost) && !req.contextWords.empty();

        // Layer 0: Compute instant N-Gram prediction FIRST (< 0.1 ms, zero AX IPC) so keystrokes
        // without a prediction or LLM opportunity skip Accessibility round-trips completely!
        PredictionResult instantPred;
        bool needInstant = !req.typeThroughOnly || req.typeThroughRemaining.empty() ||
                           req.typeThroughRemaining.find(L' ') == std::wstring::npos;
        if (needInstant && ngramPredictor_ && req.generation == ghostSession_.currentGeneration()) {
            instantPred = ngramPredictor_->predict(
                req.contextWords,
                req.partialWord,
                req.config.predictionLength,
                req.isSentenceStart,
                req.afterAcceptChain
            );
        }

        const bool canRunLlm = !req.afterAcceptChain &&
                               req.config.smartPredictionsEnabled &&
                               PowerMonitor::currentEngineMode() == EngineMode::Full &&
                               !predictorClient_.isCircuitTripped() &&
                               (!req.contextWords.empty() || !req.documentHistory.empty() || req.partialWord.size() >= 2);

        if (!req.typeThroughOnly && instantPred.text.empty() && !canRunLlm) {
            if (!ghostSession_.isVisible()) {
                ghostOverlay_.hide();
            }
            continue;
        }

        // Query caret geometry with generation-aware sync (aborts immediately on newer keystroke)
        auto geom = queryCaretGeometryWithSync(req.generation, req.partialWord, afterSpace, req.minExpectedCaretX, req.lastCaretY);
        if (req.generation != ghostSession_.currentGeneration()) continue;
        if (!geom || !geom->atEditableEnd) {
            dismissGhost();
            continue;
        }

        if (geom->textTail.empty()) {
            geom->afterWordChar = !req.partialWord.empty() || req.currentWordFromGhost;
        } else {
            wchar_t lastCh = geom->textTail.back();
            geom->afterWordChar = (lastCh != L' ' && lastCh != L'\t' && lastCh != L'\n' && lastCh != L'\r');
        }

        cachedFontName_ = geom->fontName;
        cachedFontSize_ = geom->fontSize;

        // Build rich document context for the AI model: prefer live Accessibility documentContext
        // (everything written in the active field before the caret, up to 1500 chars), falling back
        // to rolling typed documentHistory_ (for apps that don't expose AXValue), then contextWords.
        auto stripTrailingPartial = [&](std::wstring src) -> std::wstring {
            if (!req.partialWord.empty() && src.size() >= req.partialWord.size()) {
                bool match = true;
                const size_t off = src.size() - req.partialWord.size();
                for (size_t i = 0; i < req.partialWord.size(); ++i) {
                    if (std::towlower(src[off + i]) != std::towlower(req.partialWord[i])) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    src.erase(off);
                }
            }
            return src;
        };

        std::wstring docBeforePartial = stripTrailingPartial(geom->documentContext);
        std::wstring histBeforePartial = stripTrailingPartial(req.documentHistory);

        std::wstring contextKey;
        if (!docBeforePartial.empty() && docBeforePartial.size() >= histBeforePartial.size()) {
            contextKey = std::move(docBeforePartial);
        } else if (!histBeforePartial.empty()) {
            contextKey = std::move(histBeforePartial);
        } else {
            for (const auto& w : req.contextWords) {
                contextKey += w + L" ";
            }
            if (!geom->textTail.empty() && contextKey.empty()) {
                contextKey = stripTrailingPartial(geom->textTail);
            }
        }
        // Keep context safely within helper IPC cap (4096 UTF-8 bytes)
        constexpr size_t kMaxLlmContextChars = 1500;
        if (contextKey.size() > kMaxLlmContextChars) {
            contextKey.erase(0, contextKey.size() - kMaxLlmContextChars);
        }

        // Type-through fast path: if user typed a matching character and there is still multi-word text remaining,
        // re-anchor it immediately at the new cursor position, and if it's down to a short suffix, let Ngram/LLM extend it!
        bool renderedSomething = false;
        if (req.typeThroughOnly && !req.typeThroughRemaining.empty()) {
            std::wstring typeThroughText = req.typeThroughRemaining;
            std::wstring sessionGhost = ghostSession_.currentVisibleText();
            if (!sessionGhost.empty() && sessionGhost.size() <= typeThroughText.size()) {
                typeThroughText = sessionGhost;
            }
            bool midWordNow = isMidWord && (typeThroughText.empty() || typeThroughText.front() != L' ');
            GhostLayoutResult layout = computeGhostLayout(
                typeThroughText,
                *geom,
                midWordNow,
                GhostOverlay::measureTextWidth,
                req.config.ghostStyle
            );
            if (layout.valid) {
                ghostOverlay_.showLayout(layout, req.generation);
                renderedSomething = true;
                lastRenderedCaretX_.store(layout.boundingBox.x, std::memory_order_relaxed);
                lastRenderedCaretY_.store(layout.boundingBox.y, std::memory_order_relaxed);
            }
            // If typeThroughText still contains a space (multiple words ahead), no need to recompute
            if (typeThroughText.find(L' ') != std::wstring::npos) {
                continue;
            }
        }

        // Render pre-computed Layer 0 instant prediction immediately
        if (!instantPred.text.empty() && req.generation == ghostSession_.currentGeneration()) {
            bool midWordRender = isMidWord && instantPred.text.front() != L' ';
            GhostLayoutResult layout = computeGhostLayout(
                instantPred.text,
                *geom,
                midWordRender,
                GhostOverlay::measureTextWidth,
                req.config.ghostStyle
            );
            if (layout.valid) {
                instantPred.text = layout.fittedText;
                std::wstring textToRender;
                if (ghostSession_.applyCandidatePrediction(req.generation, contextKey, instantPred, textToRender)) {
                    ghostOverlay_.showLayout(layout, req.generation);
                    renderedSomething = true;
                    lastRenderedCaretX_.store(layout.boundingBox.x, std::memory_order_relaxed);
                    lastRenderedCaretY_.store(layout.boundingBox.y, std::memory_order_relaxed);
                    if (settingsDialog_ && settingsDialog_->isVisible()) {
                        QString msg = QString("[GHOST_PREDICT] source=Instant words=%1 latency=%2us")
                            .arg(instantPred.wordCount)
                            .arg(instantPred.latencyUs);
                        QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
                    }
                }
            }
        }

        if (!renderedSomething && !ghostSession_.isVisible()) {
            ghostOverlay_.hide();
        }

        // Hand off to non-blocking Layer 1 LLM worker thread for deep contextual multi-word continuation
        if (canRunLlm &&
            req.generation == ghostSession_.currentGeneration() &&
            (!contextKey.empty() || req.partialWord.size() >= 2)) {
            GhostWorkerRequest llmReq = req;
            llmReq.contextKey = contextKey;
            llmReq.geom = *geom;
            {
                std::lock_guard<std::mutex> lock(llmWorkerMutex_);
                pendingLlmReq_ = std::move(llmReq);
                hasPendingLlmReq_ = true;
            }
            llmWorkerCv_.notify_one();
        }
#endif
    }
}

void MainTrayApp::llmWorkerLoop() {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);

    // Pre-warm the out-of-process InlinePredictor helper on startup so the first prediction has zero cold start
    if (cachedConfig_.masterEnabled && cachedConfig_.ghostTextEnabled && cachedConfig_.smartPredictionsEnabled) {
        (void)predictorClient_.predict(0, L"Hello ", L"", 1);
    }
#endif

    while (ghostWorkerRunning_.load(std::memory_order_relaxed)) {
        GhostWorkerRequest req;
        {
            std::unique_lock<std::mutex> lock(llmWorkerMutex_);
            llmWorkerCv_.wait(lock, [this]() {
                return !ghostWorkerRunning_.load(std::memory_order_relaxed) || hasPendingLlmReq_;
            });
            if (!ghostWorkerRunning_.load(std::memory_order_relaxed)) break;
            req = std::move(pendingLlmReq_);
            hasPendingLlmReq_ = false;
        }

#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
        // Brief interruptible 14ms coalesce so rapid keystrokes wake immediately without blocking
        {
            std::unique_lock<std::mutex> lock(llmWorkerMutex_);
            llmWorkerCv_.wait_for(lock, std::chrono::milliseconds(14), [this, &req]() {
                return !ghostWorkerRunning_.load(std::memory_order_relaxed) ||
                       hasPendingLlmReq_ ||
                       req.generation != ghostSession_.currentGeneration();
            });
            if (!ghostWorkerRunning_.load(std::memory_order_relaxed)) break;
            if (hasPendingLlmReq_ || req.generation != ghostSession_.currentGeneration()) continue;
        }

        int maxW = maxWordsForLength(req.config.predictionLength);
        PredictionResult llmPred = predictorClient_.predict(
            req.generation,
            req.contextKey,
            req.partialWord,
            maxW
        );
        if (llmPred.text.empty() || req.generation != ghostSession_.currentGeneration()) {
            continue;
        }

        // Reuse req.geom captured for this exact generation (zero redundant AX IPC round-trip)
        CaretGeometry freshGeom = req.geom;
        if (!freshGeom.atEditableEnd || req.generation != ghostSession_.currentGeneration()) {
            continue;
        }

        const bool isMidWord = !req.partialWord.empty() && (llmPred.text.empty() || llmPred.text.front() != L' ');
        GhostLayoutResult layout = computeGhostLayout(
            llmPred.text,
            freshGeom,
            isMidWord,
            GhostOverlay::measureTextWidth,
            req.config.ghostStyle
        );
        if (layout.valid) {
            llmPred.text = layout.fittedText;
            std::wstring textToRender;
            if (ghostSession_.applyCandidatePrediction(req.generation, req.contextKey, llmPred, textToRender)) {
                GhostLayoutResult finalLayout = (textToRender == layout.fittedText)
                    ? layout
                    : computeGhostLayout(textToRender, freshGeom, isMidWord, GhostOverlay::measureTextWidth, req.config.ghostStyle);
                if (finalLayout.valid) {
                    ghostOverlay_.showLayout(finalLayout, req.generation);
                    lastRenderedCaretX_.store(finalLayout.boundingBox.x, std::memory_order_relaxed);
                    lastRenderedCaretY_.store(finalLayout.boundingBox.y, std::memory_order_relaxed);
                    if (settingsDialog_ && settingsDialog_->isVisible()) {
                        QString msg = QString("[GHOST_PREDICT] source=LLM words=%1 latency=%2ms")
                            .arg(llmPred.wordCount)
                            .arg(llmPred.latencyUs / 1000);
                        QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
                    }
                }
            }
        }
#endif
    }
}

static wchar_t keyCodeToChar(int64_t keycode, uint64_t flags) {
    bool shift = (flags & (1 << 17)) != 0;
    bool caps = (flags & (1 << 16)) != 0;
    bool upper = shift ^ caps;

    switch (keycode) {
        case kVK_ANSI_A: return upper ? L'A' : L'a';
        case kVK_ANSI_B: return upper ? L'B' : L'b';
        case kVK_ANSI_C: return upper ? L'C' : L'c';
        case kVK_ANSI_D: return upper ? L'D' : L'd';
        case kVK_ANSI_E: return upper ? L'E' : L'e';
        case kVK_ANSI_F: return upper ? L'F' : L'f';
        case kVK_ANSI_G: return upper ? L'G' : L'g';
        case kVK_ANSI_H: return upper ? L'H' : L'h';
        case kVK_ANSI_I: return upper ? L'I' : L'i';
        case kVK_ANSI_J: return upper ? L'J' : L'j';
        case kVK_ANSI_K: return upper ? L'K' : L'k';
        case kVK_ANSI_L: return upper ? L'L' : L'l';
        case kVK_ANSI_M: return upper ? L'M' : L'm';
        case kVK_ANSI_N: return upper ? L'N' : L'n';
        case kVK_ANSI_O: return upper ? L'O' : L'o';
        case kVK_ANSI_P: return upper ? L'P' : L'p';
        case kVK_ANSI_Q: return upper ? L'Q' : L'q';
        case kVK_ANSI_R: return upper ? L'R' : L'r';
        case kVK_ANSI_S: return upper ? L'S' : L's';
        case kVK_ANSI_T: return upper ? L'T' : L't';
        case kVK_ANSI_U: return upper ? L'U' : L'u';
        case kVK_ANSI_V: return upper ? L'V' : L'v';
        case kVK_ANSI_W: return upper ? L'W' : L'w';
        case kVK_ANSI_X: return upper ? L'X' : L'x';
        case kVK_ANSI_Y: return upper ? L'Y' : L'y';
        case kVK_ANSI_Z: return upper ? L'Z' : L'z';
        case kVK_ANSI_0: return shift ? L')' : L'0';
        case kVK_ANSI_1: return shift ? L'!' : L'1';
        case kVK_ANSI_2: return shift ? L'@' : L'2';
        case kVK_ANSI_3: return shift ? L'#' : L'3';
        case kVK_ANSI_4: return shift ? L'$' : L'4';
        case kVK_ANSI_5: return shift ? L'%' : L'5';
        case kVK_ANSI_6: return shift ? L'^' : L'6';
        case kVK_ANSI_7: return shift ? L'&' : L'7';
        case kVK_ANSI_8: return shift ? L'*' : L'8';
        case kVK_ANSI_9: return shift ? L'(' : L'9';
        case kVK_Space: return L' ';
        case kVK_Return: return L'\n';
        case kVK_Tab: return L'\t';
        case kVK_ANSI_Quote: return shift ? L'"' : L'\'';
        case kVK_ANSI_Period: return shift ? L'>' : L'.';
        case kVK_ANSI_Comma: return shift ? L'<' : L',';
        case kVK_ANSI_Minus: return shift ? L'_' : L'-';
        case kVK_ANSI_Equal: return shift ? L'+' : L'=';
        case kVK_ANSI_Semicolon: return shift ? L':' : L';';
        case kVK_ANSI_Slash: return shift ? L'?' : L'/';
        case kVK_ANSI_Backslash: return shift ? L'|' : L'\\';
        case kVK_ANSI_LeftBracket: return shift ? L'{' : L'[';
        case kVK_ANSI_RightBracket: return shift ? L'}' : L']';
        case kVK_ANSI_Grave: return shift ? L'~' : L'`';
        default: return L'\0';
    }
}

bool MainTrayApp::onRawKeyEvent(const RawKeyEvent& ev, bool isActiveTap) {
    // 1. Handle lock-free buffer clear request from UI / idle timer
    if (clearRequested_.exchange(false, std::memory_order_acquire)) {
        currentWord_.chars.clear();
        currentWordFromGhost_ = false;
        contextWindow_.words.clear();
        sentenceBuffer_.words.clear();
        documentHistory_.clear();
        undoSM_.reset();
    }

    // 2. A click can move the caret anywhere, so whatever we buffered no longer sits before the caret.
    // CGEventType values: LeftMouseDown=1, RightMouseDown=3, OtherMouseDown=25.
    if (ev.type == 1 || ev.type == 3 || ev.type == 25) {
        dismissGhost();
        currentWord_.chars.clear();
        currentWordFromGhost_ = false;
        undoSM_.onOtherKey();
        currentWordToken_++;
        injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);
        return false;
    }

    if (ev.type != kCGEventKeyDown) return false;

    const int64_t nowNs = std::chrono::steady_clock::now().time_since_epoch().count();
    lastKeyTimeNs_.store(nowNs, std::memory_order_relaxed);
    hasBufferedData_.store(true, std::memory_order_relaxed);

    // 3. Non-blocking config read: never stall the real-time hook thread if UI is writing config
    if (configMutex.try_lock()) {
        cachedConfig_ = sharedConfig;
        configMutex.unlock();
    }
    if (!cachedConfig_.masterEnabled) {
        dismissGhost();
        return false;
    }

#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    // 3b. Never buffer or autocorrect inside secure password inputs or excluded applications
    if (IsSecureEventInputEnabled() || isFrontmostAppExcluded(cachedConfig_.disabledApps)) {
        dismissGhost();
        currentWord_.chars.clear();
        currentWordFromGhost_ = false;
        undoSM_.onOtherKey();
        currentWordToken_++;
        injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);
        return false;
    }
#endif

    // 3c. Ghost Text accept check (runs BEFORE navigation/Tab word-boundary handling!)
    if (cachedConfig_.ghostTextEnabled &&
        ghostSession_.shouldSwallowKey(ev.keyCode, ev.flags, ev.isAutorepeat, isActiveTap, cachedConfig_.acceptKey, nowNs)) {
        if (ev.isAutorepeat) {
            return true; // Swallow held-down Tab key repeat within 500ms of accept
        }

        std::wstring acceptedChunk;
        std::wstring remainingVisible;
        if (ghostSession_.consumeAccept(cachedConfig_.ghostAcceptMode, nowNs, acceptedChunk, remainingVisible)) {
            currentWordToken_++;
            injector_.setValidToken(InjectionOp::GHOST_ACCEPT, currentWordToken_);

            std::wstring priorWord = currentWord_.chars;
            undoSM_.onGhostAccepted(acceptedChunk, priorWord);

            InjectionOp acceptOp{
                .type = InjectionOp::TYPE_TEXT,
                .count = 0,
                .text = acceptedChunk,
                .generation_token = currentWordToken_,
                .source = InjectionOp::GHOST_ACCEPT
            };
            injector_.executeImmediate(acceptOp);

            // Update currentWord_, contextWindow_, and documentHistory_ with accepted text
            appendDocumentHistory(acceptedChunk);
            std::wstring combined = currentWord_.chars + acceptedChunk;
            currentWord_.chars.clear();
            std::wstring token;
            for (wchar_t wc : combined) {
                if (wc == L' ') {
                    if (!token.empty()) {
                        contextWindow_.words.push_back(std::move(token));
                        if (contextWindow_.words.size() > 5) contextWindow_.words.pop_front();
                        token.clear();
                    }
                } else {
                    token.push_back(wc);
                }
            }
            currentWord_.chars = std::move(token);
            currentWordFromGhost_ = !currentWord_.chars.empty();
            if (!acceptedChunk.empty()) {
                wchar_t lastC = acceptedChunk.back();
                isSentenceStart_ = (lastC == L'.' || lastC == L'!' || lastC == L'?');
            }

            if (remainingVisible.empty()) {
                dismissGhost();
                lastRenderedCaretX_.store(0.0, std::memory_order_relaxed);
                lastRenderedCaretY_.store(0.0, std::memory_order_relaxed);
                // Chain next curated phrase continuation immediately after Tab accept (matches website simulator)
                if (!isSentenceStart_ && (!currentWord_.chars.empty() || !contextWindow_.words.empty())) {
                    int settle = std::clamp(static_cast<int>(acceptedChunk.size()) * 2 + 10, 12, 24);
                    scheduleGhostPrediction(false, L"", settle, 0.0, /*afterAcceptChain=*/true);
                }
            } else {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
                double chunkW = GhostOverlay::measureTextWidth(acceptedChunk, cachedFontName_, cachedFontSize_);
                ghostOverlay_.advanceLayout(chunkW, remainingVisible);
                double prevX = lastRenderedCaretX_.load(std::memory_order_relaxed);
                double newMinX = (prevX > 0.0) ? (prevX + chunkW * 0.7) : 0.0;
                if (prevX > 0.0) {
                    lastRenderedCaretX_.store(prevX + chunkW, std::memory_order_relaxed);
                }
#else
                double newMinX = 0.0;
#endif
                int settle = std::clamp(static_cast<int>(acceptedChunk.size()) * 2 + 10, 12, 28);
                scheduleGhostPrediction(true, remainingVisible, settle, newMinX);
            }

            if (settingsDialog_ && settingsDialog_->isVisible()) {
                QString msg = QString("[GHOST_ACCEPT] Inserted %1 characters via %2")
                    .arg(acceptedChunk.size())
                    .arg(cachedConfig_.acceptKey == KeyCode::RightArrow ? "RightArrow" : "Tab");
                QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
            }
            return true; // Swallow physical Tab / RightArrow
        }
    }

    // 4. Ignore keyboard shortcuts with Command (⌘) or Control (⌃) modifiers
    constexpr uint64_t kCmdMask = (1ULL << 20); // kCGEventFlagMaskCommand
    constexpr uint64_t kCtrlMask = (1ULL << 18); // kCGEventFlagMaskControl
    if ((ev.flags & (kCmdMask | kCtrlMask)) != 0) {
        dismissGhost();
        currentWord_.chars.clear();
        currentWordFromGhost_ = false;
        undoSM_.onOtherKey();
        currentWordToken_++;
        injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);
        return false;
    }

    // 5. Navigation and escape keys reset the current word buffer and hide ghost text
    if (ev.keyCode == kVK_LeftArrow || ev.keyCode == kVK_RightArrow ||
        ev.keyCode == kVK_UpArrow || ev.keyCode == kVK_DownArrow ||
        ev.keyCode == kVK_Escape || ev.keyCode == kVK_Home ||
        ev.keyCode == kVK_End || ev.keyCode == kVK_PageUp ||
        ev.keyCode == kVK_PageDown) {
        dismissGhost();
        currentWord_.chars.clear();
        currentWordFromGhost_ = false;
        undoSM_.onOtherKey();
        currentWordToken_++;
        injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);
        return false;
    }

    if (ev.keyCode == kVK_Delete) {
        return handleBackspace(isActiveTap);
    }

    wchar_t ch = keyCodeToChar(ev.keyCode, ev.flags);
    if (ch == L'\0') {
        dismissGhost();
        undoSM_.onOtherKey();
        return false;
    }

    // Never autocorrect or accept ghost text on Enter / Return: commit raw text as-is and pass Enter through directly
    if (ev.keyCode == kVK_Return || ev.keyCode == 0x4C /* kVK_ANSI_KeypadEnter */ || ch == L'\n' || ch == L'\r') {
        dismissGhost();
        appendDocumentHistory(L"\n");
        currentWord_.chars.clear();
        currentWordFromGhost_ = false;
        undoSM_.onOtherKey();
        currentWordToken_++;
        injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);
        isSentenceStart_ = true;
        return false;
    }

    // If Tab was pressed when no ghost text was visible (or with Shift+Tab), hide any ghost and pass Tab through
    // without triggering typo replacement on the preceding word!
    if (ev.keyCode == kVK_Tab || ch == L'\t') {
        dismissGhost();
        currentWord_.chars.clear();
        currentWordFromGhost_ = false;
        undoSM_.onOtherKey();
        currentWordToken_++;
        injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);
        return false;
    }

    if (isWordBoundary(ch)) {
        return handleWordBoundary(ch, isActiveTap);
    } else {
        handleCharacter(ch);
        return false;
    }
}

void MainTrayApp::handleCharacter(wchar_t ch) {
    currentWord_.chars.push_back(ch);
    appendDocumentHistory(std::wstring(1, ch));
    undoSM_.onOtherKey();
    currentWordToken_++;
    injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);

    const int64_t nowNs = std::chrono::steady_clock::now().time_since_epoch().count();
    const int64_t lastAcNs = lastAutocorrectTimeNs_.load(std::memory_order_relaxed);
    int settleMs = 0;
    if (lastAcNs > 0) {
        int64_t elapsedMs = (nowNs - lastAcNs) / 1000000LL;
        if (elapsedMs < 18) {
            settleMs = static_cast<int>(18 - elapsedMs);
        }
    }

    std::wstring remainingGhost;
    if (ghostSession_.onCharacterTyped(ch, remainingGhost)) {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
        double charW = GhostOverlay::measureTextWidth(std::wstring(1, ch), cachedFontName_, cachedFontSize_);
        ghostOverlay_.advanceLayout(charW, remainingGhost);
        double prevX = lastRenderedCaretX_.load(std::memory_order_relaxed);
        double newMinX = (prevX > 0.0) ? (prevX + charW * 0.7) : 0.0;
        if (prevX > 0.0) {
            lastRenderedCaretX_.store(prevX + charW, std::memory_order_relaxed);
        }
#else
        double newMinX = 0.0;
#endif
        scheduleGhostPrediction(true, remainingGhost, 0, newMinX);
    } else {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
        // Mismatch: user typed a character different from prediction.
        // Dismiss stale overlay immediately so it never lingers under the advancing cursor!
        ghostOverlay_.hide();
        lastRenderedCaretX_.store(0.0, std::memory_order_relaxed);
        lastRenderedCaretY_.store(0.0, std::memory_order_relaxed);
#endif
        scheduleGhostPrediction(false, L"", settleMs, 0.0);
    }

    if (settingsDialog_ && settingsDialog_->isVisible()) {
        QString msg = QString("Key typed: '%1'  |  Current word buffer: \"%2\"")
            .arg(QChar(ch))
            .arg(QString::fromStdWString(currentWord_.chars));
        QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
    }
}

bool MainTrayApp::handleBackspace(bool isActiveTap) {
    dismissGhost();

    // 1. Check if an auto-correction or Ghost Text accept just occurred right before this backspace
    UndoActionResult undoAction = cachedConfig_.singleKeyUndoEnabled ? undoSM_.onBackspace() : UndoActionResult{};
    if (undoAction.type == UndoAction::RevertGhostAccept) {
        currentWordToken_++;
        injector_.setValidToken(InjectionOp::UNDO_CORRECTION, currentWordToken_);

        int backspaces = static_cast<int>(undoAction.correctedText.size()) - (isActiveTap ? 0 : 1);
        if (backspaces > 0) {
            InjectionOp bsOp{
                .type = InjectionOp::BACKSPACE_N,
                .count = backspaces,
                .text = L"",
                .generation_token = currentWordToken_,
                .source = InjectionOp::UNDO_CORRECTION
            };
            injector_.executeImmediate(bsOp);
        }

        currentWord_.chars = undoAction.revertedText;
        currentWordFromGhost_ = false;
        popDocumentHistory(undoAction.correctedText.size());

        std::wstring contextKey;
        for (const auto& w : contextWindow_.words) contextKey += w + L" ";
        ghostSession_.markRejected(contextKey, undoAction.correctedText);

        if (settingsDialog_ && settingsDialog_->isVisible()) {
            QString msg = QString("[GHOST_UNDO] Reverted accepted suggestion (%1 chars)")
                .arg(undoAction.correctedText.size());
            QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
        }
        return isActiveTap;
    }

    if (undoAction.type == UndoAction::RevertCorrection) {
        currentWordToken_++;
        injector_.setValidToken(InjectionOp::UNDO_CORRECTION, currentWordToken_);

        size_t p = commonPrefixLength(undoAction.correctedText, undoAction.revertedText);

        int backspaces = static_cast<int>(undoAction.correctedText.size() - p) + (isActiveTap ? 1 : 0);
        std::wstring retype = undoAction.revertedText.substr(p);

        InjectionOp replaceOp{
            .type = InjectionOp::REPLACE_TEXT,
            .count = backspaces,
            .text = retype,
            .generation_token = currentWordToken_,
            .source = InjectionOp::UNDO_CORRECTION
        };

        injector_.executeImmediate(replaceOp);

        currentWord_.chars = undoAction.revertedText;
        currentWordFromGhost_ = false;
        popDocumentHistory(undoAction.correctedText.size() + 1);
        appendDocumentHistory(undoAction.revertedText);

        if (!contextWindow_.words.empty() && contextWindow_.words.back() == undoAction.correctedText) {
            contextWindow_.words.pop_back();
        }

        if (settingsDialog_ && settingsDialog_->isVisible()) {
            QString msg = QString("[UNDO_REVERT] \"%1\" -> \"%2\" (Common prefix \"%3\", Injected %4 Backspaces + \"%5\")")
                .arg(QString::fromStdWString(undoAction.correctedText))
                .arg(QString::fromStdWString(undoAction.revertedText))
                .arg(QString::fromStdWString(undoAction.correctedText.substr(0, p)))
                .arg(backspaces)
                .arg(QString::fromStdWString(retype));
            QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
        }
        return isActiveTap; // Swallow physical backspace when active tap is used
    }

    // 2. Normal backspace: pop last character from the current active word
    undoSM_.onOtherKey();
    currentWordToken_++;
    injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);
    if (!currentWord_.chars.empty()) {
        currentWord_.chars.pop_back();
    }
    popDocumentHistory(1);
    if (currentWord_.chars.empty()) {
        currentWordFromGhost_ = false;
    } else {
        scheduleGhostPrediction(false);
    }

    if (settingsDialog_ && settingsDialog_->isVisible()) {
        QString msg = QString("[BACKSPACE] Current word buffer: \"%1\"").arg(QString::fromStdWString(currentWord_.chars));
        QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
    }
    return false;
}

bool MainTrayApp::handleWordBoundary(wchar_t delimiter, bool isActiveTap) {
    if (currentWord_.chars.empty()) {
        dismissGhost();
        appendDocumentHistory(std::wstring(1, delimiter));
        undoSM_.onOtherKey();
        if (delimiter == L'.' || delimiter == L'!' || delimiter == L'?' || delimiter == L'\n') {
            isSentenceStart_ = true;
        } else if (delimiter == L',' || delimiter == L';' || delimiter == L':') {
            isSentenceStart_ = false;
        }
        return false;
    }

    std::wstring word = std::move(currentWord_.chars);
    currentWord_.chars.clear();
    currentWord_.chars.reserve(64);
    const bool skipAutocorrectFromGhost = currentWordFromGhost_;
    currentWordFromGhost_ = false;

    std::wstring wordForContext = word;
    std::wstring prevWord = L"";
    if (!isSentenceStart_ && !contextWindow_.words.empty()) {
        prevWord = contextWindow_.words.back();
    }

    bool swallowedDelimiter = false;
    CorrectionResult correction{};
    if (!skipAutocorrectFromGhost) {
        correction = correctWord(word, dict_, cachedConfig_, isSentenceStart_, prevWord);
    }

    if (correction.didCorrect) {
        dismissGhost();
        const int64_t nowNs = std::chrono::steady_clock::now().time_since_epoch().count();
        lastAutocorrectTimeNs_.store(nowNs, std::memory_order_relaxed);
        auto check = undoSM_.onWordCorrected(word, correction.corrected);
        if (!check.suppressed) {
            currentWordToken_++;
            injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);

            size_t p = commonPrefixLength(word, correction.corrected);

            int backspaces = static_cast<int>(word.size() - p) + (isActiveTap ? 0 : 1);
            std::wstring retype = correction.corrected.substr(p) + delimiter;

            InjectionOp replaceOp{
                .type = InjectionOp::REPLACE_TEXT,
                .count = backspaces,
                .text = retype,
                .generation_token = currentWordToken_,
                .source = InjectionOp::TIER1
            };

            injector_.executeImmediate(replaceOp);
            swallowedDelimiter = isActiveTap;

            if (settingsDialog_ && settingsDialog_->isVisible()) {
                QString msg = QString("[CORRECTED] \"%1\" -> \"%2\" (Common prefix \"%3\", Injected %4 Backspaces + \"%5\")")
                    .arg(QString::fromStdWString(word))
                    .arg(QString::fromStdWString(correction.corrected))
                    .arg(QString::fromStdWString(word.substr(0, p)))
                    .arg(backspaces)
                    .arg(QString::fromStdWString(retype));
                QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
            }

            wordForContext = correction.corrected;
            popDocumentHistory(word.size());
            appendDocumentHistory(correction.corrected + std::wstring(1, delimiter));
        } else {
            appendDocumentHistory(std::wstring(1, delimiter));
        }
    } else {
        appendDocumentHistory(std::wstring(1, delimiter));
        undoSM_.onOtherKey();
        if (settingsDialog_ && settingsDialog_->isVisible()) {
            QString msg = QString("Word confirmed: \"%1\" (No typo detected)").arg(QString::fromStdWString(word));
            QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
        }
    }

    // Update sentence state for next word
    if (delimiter == L'.' || delimiter == L'!' || delimiter == L'?' || delimiter == L'\n') {
        isSentenceStart_ = true;
    } else {
        isSentenceStart_ = false;
    }

    contextWindow_.words.push_back(std::move(wordForContext));
    if (contextWindow_.words.size() > 5) {
        contextWindow_.words.pop_front();
    }

    // Trigger next-word Ghost Text prediction when user presses Space
    if (delimiter == L' ') {
        std::wstring remGhost;
        if (!correction.didCorrect && ghostSession_.onCharacterTyped(L' ', remGhost)) {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
            double spaceW = GhostOverlay::measureTextWidth(L" ", cachedFontName_, cachedFontSize_);
            ghostOverlay_.advanceLayout(spaceW, remGhost);
            double prevX = lastRenderedCaretX_.load(std::memory_order_relaxed);
            double newMinX = (prevX > 0.0) ? (prevX + spaceW * 0.7) : 0.0;
            if (prevX > 0.0) {
                lastRenderedCaretX_.store(prevX + spaceW, std::memory_order_relaxed);
            }
#else
            double newMinX = 0.0;
#endif
            scheduleGhostPrediction(true, remGhost, 0, newMinX);
        } else {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
            ghostOverlay_.hide();
            lastRenderedCaretX_.store(0.0, std::memory_order_relaxed);
            lastRenderedCaretY_.store(0.0, std::memory_order_relaxed);
#endif
            int settle = correction.didCorrect ? std::clamp(static_cast<int>(correction.corrected.size()) * 2 + 10, 12, 22) : 0;
            scheduleGhostPrediction(false, L"", settle, 0.0);
        }
    } else {
        dismissGhost();
        lastRenderedCaretX_.store(0.0, std::memory_order_relaxed);
        lastRenderedCaretY_.store(0.0, std::memory_order_relaxed);
    }

    return swallowedDelimiter;
}
