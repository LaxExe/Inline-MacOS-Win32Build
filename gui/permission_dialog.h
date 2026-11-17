#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <vector>
#include <string>

// ============================================================================
// 1. Step Progress Widget (3 Segmented Pill Dashes)
// ============================================================================
class StepProgressWidget : public QWidget {
    Q_OBJECT
public:
    explicit StepProgressWidget(QWidget* parent = nullptr);
    void setStep(int step);
    void setDarkMode(bool dark);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int currentStep_ = 0;
    bool isDark_ = true;
};

// ============================================================================
// 2. Laptop Mockup Widget (Slide 2)
// ============================================================================
class LaptopMockupWidget : public QWidget {
    Q_OBJECT
public:
    explicit LaptopMockupWidget(bool isDark = true, QWidget* parent = nullptr);
    void setDarkMode(bool dark);
    void setShortcutText(const QString& shortcut);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    bool isDark_ = true;
    QString shortcutText_ = "⌘⌥⇧D";
};

// ============================================================================
// Main Walkthrough & Permission Dialog
// ============================================================================
class PermissionDialog : public QDialog {
    Q_OBJECT
public:
    static constexpr int kAutocorrectSlide  = 0;   // Step 1: How Autocorrect Works & Interactive Demo
    static constexpr int kShortcutsSlide    = 1;   // Step 2: Shortcuts & Teaching Words
    static constexpr int kPermissionsSlide  = 2;   // Step 3: System Permissions & Privacy
    static constexpr int kSlideCount        = 3;

    // Backward-compatible aliases for existing callers
    static constexpr int kVocabularySlide   = 0;
    static constexpr int kHowItWorksSlide   = 0;
    static constexpr int kFeaturesSlide     = 1;
    static constexpr int kGhostTextSlide    = 1;
    static constexpr int kModelsSlide       = 1;
    static constexpr int kPrivacySlide      = 1;
    static constexpr int kPlaygroundSlide   = 2;
    static constexpr int kPermissionSlide   = 2;

    explicit PermissionDialog(QWidget* parent = nullptr, int initialSlide = 0);
    ~PermissionDialog() override = default;

    static bool areAllPermissionsGranted();
    void setCurrentSlide(int index);
    bool isDarkMode() const { return isDarkMode_; }

public slots:
    void checkPermissions();
    void onOpenInputMonitoringClicked();
    void onOpenAccessibilityClicked();
    void onNextSlideClicked();
    void onPrevSlideClicked();
    void toggleTheme();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool isDarkMode_ = true;

    // Header controls
    StepProgressWidget* progressWidget_ = nullptr;
    QPushButton* themeToggleBtn_ = nullptr;
    QLabel* privacyLabel_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* descriptionLabel_ = nullptr;
    QStackedWidget* slideStack_ = nullptr;

    // Slide 1 controls (Demo & Autocorrect)
    QLineEdit* demoInput_ = nullptr;
    QLabel* demoFeedbackLabel_ = nullptr;
    QString lastDemoOriginal_;
    QString lastDemoCorrected_;

    // Slide 2 controls (Shortcuts & Word addition)
    LaptopMockupWidget* laptopMockup_ = nullptr;
    QLineEdit* addWordInput_ = nullptr;
    QLabel* addWordStatusLabel_ = nullptr;

    // Slide 3 controls (Permissions)
    QLabel* imStatusLabel_ = nullptr;
    QPushButton* imActionButton_ = nullptr;
    QLabel* axStatusLabel_ = nullptr;
    QPushButton* axActionButton_ = nullptr;
    QLabel* permNoteLabel_ = nullptr;

    // Footer controls
    QPushButton* backButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;

    QTimer* pollTimer_ = nullptr;

    void setupUi();
    void applyTheme();
    void updateSlideHeader();

    QWidget* createAutocorrectSlide();
    QWidget* createShortcutsSlide();
    QWidget* createPermissionsSlide();

    QString currentShortcutString() const;
};
