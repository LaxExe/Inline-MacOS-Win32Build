#include "gui/main_tray_app.h"
#include "gui/permission_dialog.h"
#include "gui/tray_state.h"
#include "core/buffers/buffer_gate.h"
#include "core/buffers/word_boundary.h"
#include "core/tier1/corrector.h"
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
#include "platform/mac/hotkey/global_hotkey.h"
#include "platform/mac/permissions.h"
#include <Carbon/Carbon.h>
#else
#include "platform/win/hotkey/global_hotkey.h"
#endif
#include <QApplication>
#include <QStyleHints>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
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

    // Vertical line (I)
    painter.setBrush(barColor);
    painter.drawRoundedRect(QRectF(5.0, 3.0, 2.2, 12.0), 0.5, 0.5);

    // Precision status dot next to it near bottom
    painter.setBrush(dotColor);
    painter.drawRoundedRect(QRectF(9.5, 9.8, 3.2, 3.2), 0.6, 0.6);

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
    clearAllBuffers();
    hook_.stop();
    injector_.stop();
}

void MainTrayApp::clearAllBuffers() {
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

    if (!hasBufferedData_.load(std::memory_order_relaxed)) return;
    int64_t nowNs = std::chrono::steady_clock::now().time_since_epoch().count();
    int64_t lastNs = lastKeyTimeNs_.load(std::memory_order_relaxed);
    constexpr int64_t kThirtySecondsNs = 30LL * 1000000000LL;
    if ((nowNs - lastNs) >= kThirtySecondsNs) {
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
        clearRequested_.store(true, std::memory_order_release);
    });
#endif

    updateTrayIcon();
    trayIcon_->setVisible(true);
    trayIcon_->show();
}

void MainTrayApp::updateTrayIcon() {
    bool enabled = false;
    bool sentenceEnabled = false;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        enabled = sharedConfig.masterEnabled;
        sentenceEnabled = sharedConfig.sentenceCorrectionEnabled;
    }

    bool isDark = false;
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    isDark = isSystemInDarkMode();
#else
    isDark = QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
#endif

    TrayIconState state = iconStateFor(enabled);
    trayIcon_->setIcon(createStatusIcon(state == TrayIconState::Active, isDark));
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
}

void MainTrayApp::toggleMasterEnabled() {
    {
        std::lock_guard<std::mutex> lock(configMutex);
        sharedConfig.masterEnabled = !sharedConfig.masterEnabled;
    }
    updateTrayIcon();
    if (settingsDialog_ && settingsDialog_->isVisible()) {
        settingsDialog_->loadFromConfig();
    }
}

void MainTrayApp::toggleSentenceCorrection() {
    {
        std::lock_guard<std::mutex> lock(configMutex);
        sharedConfig.sentenceCorrectionEnabled = !sharedConfig.sentenceCorrectionEnabled;
    }
    updateTrayIcon();
    if (settingsDialog_ && settingsDialog_->isVisible()) {
        settingsDialog_->loadFromConfig();
    }
}

void MainTrayApp::openWalkthrough() {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    bringAppToFront();
#endif
    PermissionDialog dlg(nullptr, 0);
    dlg.show();
    dlg.raise();
    dlg.activateWindow();
    dlg.exec();
    if (settingsDialog_ && settingsDialog_->isVisible()) {
        settingsDialog_->refreshPermissions();
    }
}

void MainTrayApp::openPermissions() {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    bringAppToFront();
#endif
    PermissionDialog dlg(nullptr, 3);
    dlg.show();
    dlg.raise();
    dlg.activateWindow();
    dlg.exec();
    if (settingsDialog_ && settingsDialog_->isVisible()) {
        settingsDialog_->refreshPermissions();
    }
}

void MainTrayApp::openPreferences() {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    bringAppToFront();
#endif
    if (!settingsDialog_) {
        settingsDialog_ = std::make_unique<SettingsDialog>();
    }
    settingsDialog_->updateStatusInfo(hook_.isRunning(), static_cast<int>(dict_.wordCount()));
    settingsDialog_->loadFromConfig();
    settingsDialog_->show();
    settingsDialog_->raise();
    settingsDialog_->activateWindow();
}

void MainTrayApp::quitApp() {
    clearAllBuffers();
    hook_.stop();
    injector_.stop();
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

    {
        std::lock_guard<std::mutex> lock(configMutex);
        cachedConfig_ = sharedConfig;
    }

    hook_.setKeyFilterCallback([this](const RawKeyEvent& ev, bool isActiveTap) {
        return this->onRawKeyEvent(ev, isActiveTap);
    });
    hook_.setKeyCallback([this](const RawKeyEvent& ev) {
        this->onRawKeyEvent(ev, false);
    });

    injector_.start();
    hook_.start();

    // Register default global hotkey ⌘⌥⇧K
    GlobalHotkeyManager::instance().registerHotkey(
        KeyCode::K, Mod::Cmd | Mod::Option | Mod::Shift, [this]() {
            QMetaObject::invokeMethod(this, &MainTrayApp::toggleMasterEnabled, Qt::QueuedConnection);
        }
    );

    return true;
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
        contextWindow_.words.clear();
        sentenceBuffer_.words.clear();
        undoSM_.reset();
    }

    // 2. A click can move the caret anywhere, so whatever we buffered no longer sits before the caret.
    // CGEventType values: LeftMouseDown=1, RightMouseDown=3, OtherMouseDown=25.
    if (ev.type == 1 || ev.type == 3 || ev.type == 25) {
        currentWord_.chars.clear();
        undoSM_.onOtherKey();
        currentWordToken_++;
        injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);
        return false;
    }

    if (ev.type != kCGEventKeyDown) return false;

    lastKeyTimeNs_.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_relaxed);
    hasBufferedData_.store(true, std::memory_order_relaxed);

    // 3. Non-blocking config read: never stall the real-time hook thread if UI is writing config
    if (configMutex.try_lock()) {
        cachedConfig_ = sharedConfig;
        configMutex.unlock();
    }
    if (!cachedConfig_.masterEnabled) return false;

#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    // 3b. Never buffer or autocorrect inside secure password inputs or excluded applications
    if (IsSecureEventInputEnabled() || isFrontmostAppExcluded(cachedConfig_.disabledApps)) {
        currentWord_.chars.clear();
        undoSM_.onOtherKey();
        currentWordToken_++;
        injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);
        return false;
    }
#endif

    // 4. Ignore keyboard shortcuts with Command (⌘) or Control (⌃) modifiers
    constexpr uint64_t kCmdMask = (1ULL << 20); // kCGEventFlagMaskCommand
    constexpr uint64_t kCtrlMask = (1ULL << 18); // kCGEventFlagMaskControl
    if ((ev.flags & (kCmdMask | kCtrlMask)) != 0) {
        currentWord_.chars.clear();
        undoSM_.onOtherKey();
        currentWordToken_++;
        injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);
        return false;
    }

    // 5. Navigation and escape keys reset the current word buffer
    if (ev.keyCode == kVK_LeftArrow || ev.keyCode == kVK_RightArrow ||
        ev.keyCode == kVK_UpArrow || ev.keyCode == kVK_DownArrow ||
        ev.keyCode == kVK_Escape || ev.keyCode == kVK_Home ||
        ev.keyCode == kVK_End || ev.keyCode == kVK_PageUp ||
        ev.keyCode == kVK_PageDown) {
        currentWord_.chars.clear();
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
        undoSM_.onOtherKey();
        return false;
    }

    // Never autocorrect on Enter / Return: commit raw text as-is and pass Enter through directly
    if (ev.keyCode == kVK_Return || ev.keyCode == 0x4C /* kVK_ANSI_KeypadEnter */ || ch == L'\n' || ch == L'\r') {
        currentWord_.chars.clear();
        undoSM_.onOtherKey();
        currentWordToken_++;
        injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);
        isSentenceStart_ = true;
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
    undoSM_.onOtherKey();
    currentWordToken_++;
    injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);

    if (settingsDialog_ && settingsDialog_->isVisible()) {
        QString msg = QString("Key typed: '%1'  |  Current word buffer: \"%2\"")
            .arg(QChar(ch))
            .arg(QString::fromStdWString(currentWord_.chars));
        QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
    }
}

bool MainTrayApp::handleBackspace(bool isActiveTap) {
    // 1. Check if an auto-correction just occurred right before this backspace
    UndoActionResult undoAction = cachedConfig_.singleKeyUndoEnabled ? undoSM_.onBackspace() : UndoActionResult{};
    if (undoAction.type == UndoAction::RevertCorrection) {
        currentWordToken_++;
        injector_.setValidToken(InjectionOp::UNDO_CORRECTION, currentWordToken_);

        // Common prefix optimization: only backspace the divergent suffix of correctedText
        // (plus trailing delimiter if physical backspace is swallowed by active tap),
        // then retype the divergent suffix of revertedText.
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

    if (settingsDialog_ && settingsDialog_->isVisible()) {
        QString msg = QString("[BACKSPACE] Current word buffer: \"%1\"").arg(QString::fromStdWString(currentWord_.chars));
        QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
    }
    return false;
}

bool MainTrayApp::handleWordBoundary(wchar_t delimiter, bool isActiveTap) {
    if (currentWord_.chars.empty()) {
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

    std::wstring wordForContext = word;
    std::wstring prevWord = L"";
    if (!isSentenceStart_ && !contextWindow_.words.empty()) {
        prevWord = contextWindow_.words.back();
    }

    bool swallowedDelimiter = false;
    auto correction = correctWord(word, dict_, cachedConfig_, isSentenceStart_, prevWord);
    if (correction.didCorrect) {
        auto check = undoSM_.onWordCorrected(word, correction.corrected);
        if (!check.suppressed) {
            currentWordToken_++;
            injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);

            // Common prefix optimization + Step 1 (delimiter swallowed before app sees it):
            // If isActiveTap is true, the delimiter has NOT reached the app yet, so we only delete
            // (word.size() - p) characters (0 backspaces when a missing letter is at the end!)
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
        }
    } else {
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
    return swallowedDelimiter;
}

