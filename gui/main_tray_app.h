#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include "gui/settings_dialog.h"
#include "platform/mac/hook/mac_keyboard_hook.h"
#include "platform/mac/inject/mac_injector.h"
#include "platform/mac/caret/mac_caret_tracker.h"
#include "platform/mac/overlay/ghost_overlay_window.h"
#include "core/tier1/symspell_dict.h"
#include "core/ghosttext/predictor.h"
#include "core/undo/undo_state_machine.h"
#include "core/buffers/buffers.h"
#include "core/reliability/privacy_logger.h"
#include <memory>
#include <atomic>

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
    void openPreferences();
    void clearAllBuffers();
    void quitApp();

private:
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;
    QAction* statusAction_ = nullptr;
    QAction* pauseAction_ = nullptr;
    QAction* sentenceToggleAction_ = nullptr;
    QAction* clearAction_ = nullptr;
    QAction* prefsAction_ = nullptr;
    QAction* quitAction_ = nullptr;

    std::unique_ptr<SettingsDialog> settingsDialog_;

    // Core & platform components
    MacKeyboardHook hook_;
    MacInjector injector_;
    MacCaretTracker caretTracker_;
    GhostOverlayWindow overlayWindow_;
    SymSpellDict dict_;
    std::unique_ptr<GhostPredictor> predictor_;
    UndoStateMachine undoSM_;

    CurrentWordBuffer currentWord_;
    ContextWindow contextWindow_;
    SentenceBuffer sentenceBuffer_;

    int currentWordToken_ = 0;
    int currentSentenceToken_ = 0;
    std::wstring currentGhostText_;
    std::atomic<bool> hasGhostActive_{false};
    bool isSentenceStart_ = true;
    QTimer* pauseTimer_ = nullptr;
    QTimer* idleClearTimer_ = nullptr;

    void setupTray();
    void onRawKeyEvent(const RawKeyEvent& ev);
    void handleWordBoundary(wchar_t delimiter);
    void handleCharacter(wchar_t ch);
    void handleBackspace();
    void handleTabAccept();
    void onTypingPause();
    void onIdleTimeout();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
};
