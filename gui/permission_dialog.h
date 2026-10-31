#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <vector>

class PermissionDialog : public QDialog {
    Q_OBJECT
public:
    static constexpr int kHowItWorksSlide = 0;
    static constexpr int kFeaturesSlide   = 1;
    static constexpr int kGhostTextSlide  = 2;
    static constexpr int kPrivacySlide    = 3;
    static constexpr int kPermissionSlide = 4;
    static constexpr int kSlideCount      = 5;

    explicit PermissionDialog(QWidget* parent = nullptr, int initialSlide = 0);
    ~PermissionDialog() override = default;

    static bool areAllPermissionsGranted();
    void setCurrentSlide(int index);

public slots:
    void checkPermissions();
    void onOpenInputMonitoringClicked();
    void onOpenAccessibilityClicked();
    void onNextSlideClicked();
    void onPrevSlideClicked();

private:
    QLabel* stepBadgeLabel_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* descriptionLabel_ = nullptr;
    QStackedWidget* slideStack_ = nullptr;

    // Slide 4: Subsystem inspection rows
    QLabel* imStatusLabel_ = nullptr;
    QPushButton* imActionButton_ = nullptr;

    QLabel* axStatusLabel_ = nullptr;
    QPushButton* axActionButton_ = nullptr;

    QLabel* statusSummaryLabel_ = nullptr;

    // Footer controls
    QPushButton* skipButton_ = nullptr;
    QPushButton* backButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;
    std::vector<QPushButton*> dotButtons_;

    QTimer* pollTimer_ = nullptr;

    void setupUi();
    void applyTheme();
    void updateSlideHeader();

    QWidget* createHowItWorksSlide();
    QWidget* createFeaturesSlide();
    QWidget* createGhostTextSlide();
    QWidget* createPrivacyAndControlsSlide();
    QWidget* createPermissionSlide();

    QWidget* createInfoCard(const std::vector<std::tuple<int, QString, QString, QString>>& rows);
    QWidget* createPermissionRow(int iconType, const QString& title, const QString& subtitle,
                                 QLabel*& statusLabel, QPushButton*& actionButton,
                                 const char* slot, bool isLast = false);
};
