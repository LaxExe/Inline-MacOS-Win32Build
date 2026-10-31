#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include "gui/settings_dialog.h"

#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
#include "platform/mac/hook/mac_keyboard_hook.h"
#include "platform/mac/inject/mac_injector.h"
#include "platform/mac/caret/mac_caret_tracker.h"
#include "platform/mac/ghost/ghost_overlay.h"
#include "platform/mac/predict/predictor_client.h"
#include "platform/mac/power/power_monitor.h"
using PlatformKeyboardHook = MacKeyboardHook;
using PlatformInjector = MacInjector;
#else
#include "platform/win/hook/win_keyboard_hook.h"
#include "platform/win/inject/win_injector.h"
using PlatformKeyboardHook = WinKeyboardHook;
using PlatformInjector = WinInjector;
#endif

#include "core/tier1/symspell_dict.h"
#include "core/undo/undo_state_machine.h"
#include "core/buffers/buffers.h"
#include "core/ghost/ghost_session.h"
#include "core/predict/ngram_predictor.h"
#include "core/reliability/privacy_logger.h"
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>

class MainTrayApp : public QObject {
    Q_OBJECT
public:
    explicit MainTrayApp(QObject* parent = nullptr);
    ~MainTrayApp() override;

    bool initEngine();
    void updateTrayIcon();

public slots:
    void toggleMasterEnabled();
    void toggleSentenceCorrection();
    void toggleGhostText();
    void openPreferences();
    void openPermissions();
    void openWalkthrough();
    void clearAllBuffers();
    void quitApp();

private:
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;
    QAction* statusAction_ = nullptr;
    QAction* pauseAction_ = nullptr;
    QAction* sentenceToggleAction_ = nullptr;
    QAction* ghostToggleAction_ = nullptr;
    QAction* clearAction_ = nullptr;
    QAction* walkthroughAction_ = nullptr;
    QAction* permAction_ = nullptr;
    QAction* prefsAction_ = nullptr;
    QAction* quitAction_ = nullptr;

    std::unique_ptr<SettingsDialog> settingsDialog_;

    // Core & platform components
    PlatformKeyboardHook hook_;
    PlatformInjector injector_;
    SymSpellDict dict_;
    UndoStateMachine undoSM_;
    GhostSession ghostSession_;
    std::unique_ptr<NgramPredictor> ngramPredictor_;

#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    MacCaretTracker caretTracker_;
    GhostOverlay ghostOverlay_;
    PredictorClient predictorClient_;
#endif

    std::thread ghostWorkerThread_;
    std::thread llmWorkerThread_;
    std::atomic<bool> ghostWorkerRunning_{false};
    std::mutex ghostWorkerMutex_;
    std::condition_variable ghostWorkerCv_;
    std::mutex llmWorkerMutex_;
    std::condition_variable llmWorkerCv_;
    struct GhostWorkerRequest {
        uint64_t generation = 0;
        std::vector<std::wstring> contextWords;
        std::wstring partialWord;
        std::wstring typeThroughRemaining;
        std::wstring contextKey;
        CaretGeometry geom;
        bool isSentenceStart = false;
        bool typeThroughOnly = false;
        AppConfig config;
    };
    GhostWorkerRequest pendingGhostReq_;
    bool hasPendingGhostReq_ = false;
    GhostWorkerRequest pendingLlmReq_;
    bool hasPendingLlmReq_ = false;

    CurrentWordBuffer currentWord_;
    ContextWindow contextWindow_;
    SentenceBuffer sentenceBuffer_;

    int currentWordToken_ = 0;
    bool isSentenceStart_ = true;
    bool currentWordFromGhost_ = false;
    bool lastKnownDarkMode_ = false;
    AppConfig cachedConfig_;
    QTimer* idleClearTimer_ = nullptr;
    std::atomic<int64_t> lastKeyTimeNs_{0};
    std::atomic<bool> hasBufferedData_{false};
    std::atomic<bool> clearRequested_{false};

    void setupTray();
    void dismissGhost();
    void scheduleGhostPrediction(bool typeThroughOnly, const std::wstring& typeThroughText = L"");
    void ghostWorkerLoop();
    void llmWorkerLoop();
    std::optional<CaretGeometry> queryCaretGeometryWithSync(const std::wstring& partialWord, bool afterSpace);
    bool onRawKeyEvent(const RawKeyEvent& ev, bool isActiveTap = false);
    bool handleWordBoundary(wchar_t delimiter, bool isActiveTap);
    void handleCharacter(wchar_t ch);
    bool handleBackspace(bool isActiveTap);
    void onIdleTimeout();
    void onIdleCheck();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
};
