#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

class PermissionDialog : public QDialog {
    Q_OBJECT
public:
    explicit PermissionDialog(QWidget* parent = nullptr);
    ~PermissionDialog() override = default;

    static bool areAllPermissionsGranted();

public slots:
    void checkPermissions();
    void onOpenInputMonitoringClicked();
    void onOpenAccessibilityClicked();

private:
    QLabel* titleLabel_ = nullptr;
    QLabel* descriptionLabel_ = nullptr;

    // Subsystem inspection rows
    QLabel* imStatusLabel_ = nullptr;
    QPushButton* imActionButton_ = nullptr;

    QLabel* axStatusLabel_ = nullptr;
    QPushButton* axActionButton_ = nullptr;

    QLabel* statusSummaryLabel_ = nullptr;
    QPushButton* skipButton_ = nullptr;
    QPushButton* continueButton_ = nullptr;

    QTimer* pollTimer_ = nullptr;

    void setupUi();
    void applyTheme();
    QWidget* createPermissionCard();
    QWidget* createPermissionRow(int iconType, const QString& title, const QString& subtitle,
                                 QLabel*& statusLabel, QPushButton*& actionButton,
                                 const char* slot, bool isLast = false);
};
