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
    PlatformKeyboardHook hook_;
    PlatformInjector injector_;
    SymSpellDict dict_;
    UndoStateMachine undoSM_;

    CurrentWordBuffer currentWord_;
    ContextWindow contextWindow_;
    SentenceBuffer sentenceBuffer_;

    int currentWordToken_ = 0;
    bool isSentenceStart_ = true;
    QTimer* idleClearTimer_ = nullptr;

    void setupTray();
    void onRawKeyEvent(const RawKeyEvent& ev);
    void handleWordBoundary(wchar_t delimiter);
    void handleCharacter(wchar_t ch);
    void handleBackspace();
    void onIdleTimeout();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
};
