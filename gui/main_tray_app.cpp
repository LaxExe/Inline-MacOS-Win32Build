#include "gui/main_tray_app.h"
#include "gui/tray_state.h"
#include "core/buffers/buffer_gate.h"
#include "core/buffers/word_boundary.h"
#include "core/tier1/corrector.h"
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
#include "platform/mac/hotkey/global_hotkey.h"
#include <Carbon/Carbon.h>
#else
#include "platform/win/hotkey/global_hotkey.h"
#endif
#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <iostream>

static QIcon createStatusIcon(bool active) {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    if (active) {
        painter.setBrush(QColor(40, 180, 80)); // Active Green
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(4, 4, 24, 24);

        painter.setPen(QPen(Qt::white, 3));
        painter.drawLine(10, 16, 14, 20);
        painter.drawLine(14, 20, 22, 12);
    } else {
        painter.setBrush(QColor(160, 160, 160)); // Paused Gray
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(4, 4, 24, 24);

        painter.setPen(QPen(Qt::white, 3));
        painter.drawLine(10, 16, 22, 16);
    }

    return QIcon(pixmap);
}

MainTrayApp::MainTrayApp(QObject* parent) : QObject(parent) {
    idleClearTimer_ = new QTimer(this);
    idleClearTimer_->setSingleShot(true);
    idleClearTimer_->setInterval(30000); // 30 seconds
    connect(idleClearTimer_, &QTimer::timeout, this, &MainTrayApp::onIdleTimeout);

    setupTray();
}

MainTrayApp::~MainTrayApp() {
    clearAllBuffers();
    hook_.stop();
    injector_.stop();
}

void MainTrayApp::clearAllBuffers() {
    currentWord_.chars.clear();
    contextWindow_.words.clear();
    sentenceBuffer_.words.clear();
    undoSM_.reset();
    hook_.clearQueue();
    injector_.clearQueue();

    if (settingsDialog_ && settingsDialog_->isVisible()) {
        QString msg = QString("🧹 [BUFFER CLEARED] All keystrokes, word buffers, and queues purged.");
        QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
    }
}

void MainTrayApp::onIdleTimeout() {
    clearAllBuffers();
    if (settingsDialog_ && settingsDialog_->isVisible()) {
        QString msg = QString("⏳ [IDLE AUTO-CLEAR] Inactivity for >30s detected. Buffer automatically purged.");
        QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
    }
}

void MainTrayApp::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    (void)reason;
    clearAllBuffers();
}

void MainTrayApp::setupTray() {
    trayIcon_ = new QSystemTrayIcon(this);
    trayMenu_ = new QMenu();

    statusAction_ = trayMenu_->addAction(tr("● Typing Assistant: On"));
    statusAction_->setEnabled(false);

    trayMenu_->addSeparator();

    pauseAction_ = trayMenu_->addAction(tr("Pause Corrections"), this, &MainTrayApp::toggleMasterEnabled);
    sentenceToggleAction_ = trayMenu_->addAction(tr("Sentence Correction: On"), this, &MainTrayApp::toggleSentenceCorrection);
    clearAction_ = trayMenu_->addAction(tr("🧹 Clear Keystroke Buffer"), this, &MainTrayApp::clearAllBuffers);

    trayMenu_->addSeparator();

    prefsAction_ = trayMenu_->addAction(tr("Preferences..."), this, &MainTrayApp::openPreferences);
    quitAction_ = trayMenu_->addAction(tr("Quit"), this, &MainTrayApp::quitApp);

    trayIcon_->setContextMenu(trayMenu_);
    connect(trayIcon_, &QSystemTrayIcon::activated, this, &MainTrayApp::onTrayActivated);
    connect(trayMenu_, &QMenu::aboutToShow, this, &MainTrayApp::clearAllBuffers);

    updateTrayIcon();
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

    TrayIconState state = iconStateFor(enabled);
    trayIcon_->setIcon(createStatusIcon(state == TrayIconState::Active));

    if (enabled) {
        statusAction_->setText(tr("● Typing Assistant: On"));
        pauseAction_->setText(tr("Pause Corrections"));
    } else {
        statusAction_->setText(tr("○ Typing Assistant: Paused"));
        pauseAction_->setText(tr("Resume Corrections"));
    }

    sentenceToggleAction_->setText(sentenceEnabled ? tr("Sentence Correction: On") : tr("Sentence Correction: Off"));
}

void MainTrayApp::toggleMasterEnabled() {
    {
        std::lock_guard<std::mutex> lock(configMutex);
        sharedConfig.masterEnabled = !sharedConfig.masterEnabled;
    }
    updateTrayIcon();
}

void MainTrayApp::toggleSentenceCorrection() {
    {
        std::lock_guard<std::mutex> lock(configMutex);
        sharedConfig.sentenceCorrectionEnabled = !sharedConfig.sentenceCorrectionEnabled;
    }
    updateTrayIcon();
}

void MainTrayApp::openPreferences() {
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

    hook_.setKeyCallback([this](const RawKeyEvent& ev) {
        QMetaObject::invokeMethod(this, [this, ev]() {
            this->onRawKeyEvent(ev);
        }, Qt::QueuedConnection);
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

void MainTrayApp::onRawKeyEvent(const RawKeyEvent& ev) {
    // A click can move the caret anywhere, so whatever we buffered no longer sits before the caret.
    // CGEventType values: LeftMouseDown=1, RightMouseDown=3, OtherMouseDown=25.
    if (ev.type == 1 || ev.type == 3 || ev.type == 25) {
        currentWord_.chars.clear();
        undoSM_.onOtherKey();
        return;
    }

    if (ev.type != kCGEventKeyDown) return;

    if (idleClearTimer_) {
        idleClearTimer_->start(30000);
    }

    bool enabled = false;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        enabled = sharedConfig.masterEnabled;
    }
    if (!enabled) return;

    // Ignore keyboard shortcuts with Command (⌘) or Control (⌃) modifiers
    constexpr uint64_t kCmdMask = (1ULL << 20); // kCGEventFlagMaskCommand
    constexpr uint64_t kCtrlMask = (1ULL << 18); // kCGEventFlagMaskControl
    if ((ev.flags & (kCmdMask | kCtrlMask)) != 0) {
        currentWord_.chars.clear();
        undoSM_.onOtherKey();
        return;
    }

    // Navigation and escape keys reset the current word buffer
    if (ev.keyCode == kVK_LeftArrow || ev.keyCode == kVK_RightArrow ||
        ev.keyCode == kVK_UpArrow || ev.keyCode == kVK_DownArrow ||
        ev.keyCode == kVK_Escape || ev.keyCode == kVK_Home ||
        ev.keyCode == kVK_End || ev.keyCode == kVK_PageUp ||
        ev.keyCode == kVK_PageDown) {
        currentWord_.chars.clear();
        undoSM_.onOtherKey();
        return;
    }

    if (ev.keyCode == kVK_Delete) {
        handleBackspace();
        return;
    }

    wchar_t ch = keyCodeToChar(ev.keyCode, ev.flags);
    if (ch == L'\0') {
        undoSM_.onOtherKey();
        return;
    }

    if (isWordBoundary(ch)) {
        handleWordBoundary(ch);
    } else {
        handleCharacter(ch);
    }
}

void MainTrayApp::handleCharacter(wchar_t ch) {
    currentWord_.chars.push_back(ch);
    undoSM_.onOtherKey();

    if (settingsDialog_ && settingsDialog_->isVisible()) {
        QString msg = QString("Key typed: '%1'  |  Current word buffer: \"%2\"")
            .arg(QChar(ch))
            .arg(QString::fromStdWString(currentWord_.chars));
        QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
    }
}

void MainTrayApp::handleBackspace() {
    // 1. Check if an auto-correction just occurred right before this backspace
    UndoActionResult undoAction = undoSM_.onBackspace();
    if (undoAction.type == UndoAction::RevertCorrection) {
        currentWordToken_++;
        injector_.setValidToken(InjectionOp::UNDO_CORRECTION, currentWordToken_);

        // Physical backspace already removed the trailing delimiter in the active app.
        // We now atomically backspace the corrected word and retype the original word.
        int backspaces = static_cast<int>(undoAction.correctedText.size());
        InjectionOp replaceOp{
            .type = InjectionOp::REPLACE_TEXT,
            .count = backspaces,
            .text = undoAction.revertedText,
            .generation_token = currentWordToken_,
            .source = InjectionOp::UNDO_CORRECTION
        };

        injector_.enqueue(replaceOp);

        currentWord_.chars = undoAction.revertedText;

        if (!contextWindow_.words.empty() && contextWindow_.words.back() == undoAction.correctedText) {
            contextWindow_.words.pop_back();
        }

        if (settingsDialog_ && settingsDialog_->isVisible()) {
            QString msg = QString("↺ [REVERT] \"%1\" -> \"%2\" (Injected %3 Backspaces + \"%4\")")
                .arg(QString::fromStdWString(undoAction.correctedText))
                .arg(QString::fromStdWString(undoAction.revertedText))
                .arg(backspaces)
                .arg(QString::fromStdWString(undoAction.revertedText));
            QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
        }
        return;
    }

    // 2. Normal backspace: pop last character from the current active word
    undoSM_.onOtherKey();
    if (!currentWord_.chars.empty()) {
        currentWord_.chars.pop_back();
    }

    if (settingsDialog_ && settingsDialog_->isVisible()) {
        QString msg = QString("⌫ [BACKSPACE] Current word buffer: \"%1\"").arg(QString::fromStdWString(currentWord_.chars));
        QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
    }
}

void MainTrayApp::handleWordBoundary(wchar_t delimiter) {
    std::wstring word = currentWord_.chars;
    currentWord_.chars.clear();

    if (word.empty()) {
        undoSM_.onOtherKey();
        if (delimiter == L'.' || delimiter == L'!' || delimiter == L'?' || delimiter == L'\n') {
            isSentenceStart_ = true;
        } else if (delimiter == L',' || delimiter == L';' || delimiter == L':') {
            isSentenceStart_ = false;
        }
        return;
    }

    AppConfig cfg;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        cfg = sharedConfig;
    }

    std::wstring wordForContext = word;
    std::wstring prevWord = L"";
    if (!isSentenceStart_ && !contextWindow_.words.empty()) {
        prevWord = contextWindow_.words.back();
    }

    auto correction = correctWord(word, dict_, cfg, isSentenceStart_, prevWord);
    if (correction.didCorrect) {
        auto check = undoSM_.onWordCorrected(word, correction.corrected);
        if (!check.suppressed) {
            currentWordToken_++;
            injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);

            // Backspace word + delimiter, retype corrected + delimiter atomically
            int backspaces = static_cast<int>(word.size()) + 1;
            std::wstring retype = correction.corrected + delimiter;

            InjectionOp replaceOp{
                .type = InjectionOp::REPLACE_TEXT,
                .count = backspaces,
                .text = retype,
                .generation_token = currentWordToken_,
                .source = InjectionOp::TIER1
            };

            injector_.enqueue(replaceOp);

            if (settingsDialog_ && settingsDialog_->isVisible()) {
                QString msg = QString("✨ FIXED: \"%1\" -> \"%2\" (Injected %3 Backspaces + \"%4\")")
                    .arg(QString::fromStdWString(word))
                    .arg(QString::fromStdWString(correction.corrected))
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

    contextWindow_.words.push_back(wordForContext);
    if (contextWindow_.words.size() > 5) {
        contextWindow_.words.pop_front();
    }
}

