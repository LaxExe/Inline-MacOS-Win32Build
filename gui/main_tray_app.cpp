#include "gui/main_tray_app.h"
#include "gui/permission_dialog.h"
#include "gui/tray_state.h"
#include "core/buffers/buffer_gate.h"
#include "core/buffers/word_boundary.h"
#include "core/tier1/corrector.h"
#include "core/ghost/ghost_layout.h"
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
    ghostToggleAction_ = trayMenu_->addAction(tr("Ghost Text Predictions: Active"), this, &MainTrayApp::toggleGhostText);
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
}

void MainTrayApp::updateTrayIcon() {
    bool enabled = false;
    bool sentenceEnabled = false;
    bool ghostEnabled = false;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        enabled = sharedConfig.masterEnabled;
        sentenceEnabled = sharedConfig.sentenceCorrectionEnabled;
        ghostEnabled = sharedConfig.ghostTextEnabled;
    }

    if (!enabled || !ghostEnabled) {
        dismissGhost();
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
    if (ghostToggleAction_) {
        ghostToggleAction_->setText(ghostEnabled ? tr("Ghost Text Predictions: Active") : tr("Ghost Text Predictions: Inactive"));
    }
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

void MainTrayApp::toggleGhostText() {
    bool nowEnabled = false;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        sharedConfig.ghostTextEnabled = !sharedConfig.ghostTextEnabled;
        nowEnabled = sharedConfig.ghostTextEnabled;
    }
    if (!nowEnabled) {
        dismissGhost();
    }
    updateTrayIcon();
    if (settingsDialog_ && settingsDialog_->isVisible()) {
        settingsDialog_->loadFromConfig();
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

    return true;
}

void MainTrayApp::scheduleGhostPrediction(bool typeThroughOnly, const std::wstring& typeThroughText, int settleMs) {
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
    req.typeThroughRemaining = typeThroughText;
    req.isSentenceStart = isSentenceStart_;
    req.typeThroughOnly = typeThroughOnly;
    req.settleMs = settleMs;
    req.config = cachedConfig_;

    {
        std::lock_guard<std::mutex> lock(ghostWorkerMutex_);
        pendingGhostReq_ = std::move(req);
        hasPendingGhostReq_ = true;
    }
    ghostWorkerCv_.notify_one();
}

std::optional<CaretGeometry> MainTrayApp::queryCaretGeometryWithSync(const std::wstring& partialWord, bool afterSpace) {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    // 1. Wait 3ms so the physical or synthetic keystroke finishes dispatching to the target app
    std::this_thread::sleep_for(std::chrono::milliseconds(3));

    // 2. If our own Inline Settings window (e.g. Playground QLineEdit) is active, query the Qt widget directly
    // for 100% pixel-exact cursor coordinates without AX IPC overhead.
    if (settingsDialog_ && settingsDialog_->isVisible() && QApplication::activeWindow() != nullptr) {
        std::optional<CaretGeometry> selfGeom;
        QMetaObject::invokeMethod(settingsDialog_.get(), [&]() {
            QWidget* fw = QApplication::focusWidget();
            if (auto* le = qobject_cast<QLineEdit*>(fw)) {
                struct PublicLineEdit : public QLineEdit { using QLineEdit::cursorRect; };
                QRect cr = static_cast<PublicLineEdit*>(le)->cursorRect();
                QFontMetricsF fm(le->font());
                QString fullText = le->text();
                int cpos = le->cursorPosition();
                QString before = fullText.left(cpos);
                QString after = fullText.mid(cpos);

                double lineH = std::max(16.0, fm.height());
                int topYLocal = static_cast<int>(std::round((le->height() - lineH) * 0.5));
                QPoint globalCaret = le->mapToGlobal(QPoint(cr.center().x() + 1, topYLocal));
                QPoint globalField = le->mapToGlobal(QPoint(0, 0));

                CaretGeometry g;
                g.pid = static_cast<int>(::getpid());
                g.caret = {static_cast<double>(globalCaret.x()), static_cast<double>(globalCaret.y()), 2.5, lineH};
                g.field = {static_cast<double>(globalField.x()), static_cast<double>(globalField.y()),
                           static_cast<double>(le->width()), static_cast<double>(le->height())};
                g.lineStartX = g.field.x + 10.0;
                g.lineHeight = lineH;
                g.fontSize = le->fontInfo().pointSizeF() > 0 ? le->fontInfo().pointSizeF() : 13.0;
                g.isSingleLine = true;
                g.atEditableEnd = after.isEmpty();
                g.caretAtEOF = after.isEmpty();
                g.textTail = before.toStdWString();
                selfGeom = g;
            }
        }, Qt::BlockingQueuedConnection);
        if (selfGeom) {
            return selfGeom;
        }
    }

    // 3. External macOS app: query AXCaretTracker and ensure textTail has caught up to the latest keystroke
    wchar_t expectedLast = L'\0';
    if (!partialWord.empty()) {
        expectedLast = static_cast<wchar_t>(std::towlower(partialWord.back()));
    } else if (afterSpace) {
        expectedLast = L' ';
    }

    std::optional<CaretGeometry> geom;
    for (int attempt = 0; attempt < 3; ++attempt) {
        geom = caretTracker_.getCaretGeometry();
        if (!geom) {
            // Give waking Chromium/Electron accessibility trees a brief moment to finish building
            if (attempt < 2) {
                std::this_thread::sleep_for(std::chrono::milliseconds(6));
                continue;
            }
            return std::nullopt;
        }
        if (expectedLast == L'\0' || geom->textTail.empty()) {
            break;
        }
        wchar_t actualLast = static_cast<wchar_t>(std::towlower(geom->textTail.back()));
        if (actualLast == expectedLast) {
            break; // Host app has processed the latest keystroke and updated its cursor!
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return geom;
#else
    (void)partialWord;
    (void)afterSpace;
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
        if (req.settleMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(req.settleMs));
            if (req.generation != ghostSession_.currentGeneration()) {
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

        const bool isMidWord = !req.partialWord.empty();
        const bool afterSpace = req.partialWord.empty() && !req.contextWords.empty();

        // Query caret geometry immediately (with 3ms keystroke settle + textTail sync so cursor is never stale)
        auto geom = queryCaretGeometryWithSync(req.partialWord, afterSpace);
        if (req.generation != ghostSession_.currentGeneration()) continue;
        if (!geom || !geom->atEditableEnd) {
            dismissGhost();
            continue;
        }

        // Build context string for rejection key and LLM prompt
        std::wstring contextKey;
        for (const auto& w : req.contextWords) {
            contextKey += w + L" ";
        }
        if (!geom->textTail.empty() && contextKey.empty()) {
            contextKey = geom->textTail;
            if (!req.partialWord.empty() && contextKey.size() >= req.partialWord.size()) {
                contextKey.erase(contextKey.size() - req.partialWord.size());
            }
        }

        // Type-through fast path: if user typed a matching character and there is still multi-word text remaining,
        // re-anchor it immediately at the new cursor position, and if it's down to a short suffix, let Ngram/LLM extend it!
        bool renderedSomething = false;
        if (req.typeThroughOnly && !req.typeThroughRemaining.empty()) {
            bool midWordNow = req.typeThroughRemaining.front() != L' ';
            GhostLayoutResult layout = computeGhostLayout(
                req.typeThroughRemaining,
                *geom,
                midWordNow,
                GhostOverlay::measureTextWidth,
                req.config.ghostStyle
            );
            if (layout.valid) {
                ghostOverlay_.showLayout(layout);
                renderedSomething = true;
            }
            // If typeThroughRemaining still contains a space (multiple words ahead), no need to recompute
            if (req.typeThroughRemaining.find(L' ') != std::wstring::npos) {
                continue;
            }
        }

        // Layer 0: Instant N-Gram & Phrase Predictor (< 0.1 ms, ZERO debounce — active on every keystroke!)
        if (ngramPredictor_ && req.generation == ghostSession_.currentGeneration()) {
            PredictionResult instantPred = ngramPredictor_->predict(
                req.contextWords,
                req.partialWord,
                req.config.predictionLength,
                req.isSentenceStart
            );
            if (!instantPred.text.empty()) {
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
                        ghostOverlay_.showLayout(layout);
                        renderedSomething = true;
                        if (settingsDialog_ && settingsDialog_->isVisible()) {
                            QString msg = QString("[GHOST_PREDICT] source=Instant words=%1 latency=%2us")
                                .arg(instantPred.wordCount)
                                .arg(instantPred.latencyUs);
                            QMetaObject::invokeMethod(settingsDialog_.get(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
                        }
                    }
                }
            }
        }

        if (!renderedSomething && !ghostSession_.isVisible()) {
            ghostOverlay_.hide();
        }

        // Hand off to non-blocking Layer 1 LLM worker thread for deep contextual multi-word continuation
        if (req.config.smartPredictionsEnabled &&
            PowerMonitor::currentEngineMode() == EngineMode::Full &&
            !predictorClient_.isCircuitTripped() &&
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
        // Brief 22ms coalesce so rapid keystrokes don't queue unnecessary LLM calls while Layer 0 handles every key
        std::this_thread::sleep_for(std::chrono::milliseconds(22));
        if (!ghostWorkerRunning_.load(std::memory_order_relaxed)) break;
        if (req.generation != ghostSession_.currentGeneration()) continue;

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

        // Re-verify latest caret geometry in case the host view scrolled or moved
        auto freshGeom = queryCaretGeometryWithSync(req.partialWord, req.partialWord.empty() && !req.contextWords.empty());
        if (!freshGeom || !freshGeom->atEditableEnd || req.generation != ghostSession_.currentGeneration()) {
            continue;
        }

        const bool isMidWord = !req.partialWord.empty() && llmPred.text.front() != L' ';
        GhostLayoutResult layout = computeGhostLayout(
            llmPred.text,
            *freshGeom,
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
                    : computeGhostLayout(textToRender, *freshGeom, isMidWord, GhostOverlay::measureTextWidth, req.config.ghostStyle);
                if (finalLayout.valid) {
                    ghostOverlay_.showLayout(finalLayout);
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
            // Dismiss current overlay immediately so old ghost text vanishes instantly while synthetic keystrokes are typed
            ghostOverlay_.hide();

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

            // Update currentWord_ and contextWindow_ with accepted text
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
                // Do not immediately predict new words on Tab accept!
                // Allow the injected text to settle cleanly in the host application.
                // When the user resumes typing or hits space, ghost predictions resume naturally.
            } else {
                int settle = std::clamp(static_cast<int>(acceptedChunk.size()) * 4 + 15, 20, 45);
                scheduleGhostPrediction(true, remainingVisible, settle);
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
    undoSM_.onOtherKey();
    currentWordToken_++;
    injector_.setValidToken(InjectionOp::TIER1, currentWordToken_);

    std::wstring remainingGhost;
    if (ghostSession_.onCharacterTyped(ch, remainingGhost)) {
        scheduleGhostPrediction(true, remainingGhost);
    } else {
        scheduleGhostPrediction(false);
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

    // Trigger next-word Ghost Text prediction when user presses Space
    if (delimiter == L' ') {
        std::wstring remGhost;
        if (!correction.didCorrect && ghostSession_.onCharacterTyped(L' ', remGhost)) {
            scheduleGhostPrediction(true, remGhost);
        } else {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
            if (correction.didCorrect) {
                ghostOverlay_.hide();
            }
#endif
            int settle = correction.didCorrect ? std::clamp(static_cast<int>(correction.corrected.size()) * 4 + 20, 25, 45) : 0;
            scheduleGhostPrediction(false, L"", settle);
        }
    } else {
        dismissGhost();
    }

    return swallowedDelimiter;
}
