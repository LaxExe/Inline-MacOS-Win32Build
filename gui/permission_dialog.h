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

    // Returns true if all needed permissions are granted
    static bool areAllPermissionsGranted();

public slots:
    void checkPermissions();
    void onOpenInputMonitoringClicked();
    void onOpenAccessibilityClicked();

private:
    QLabel* titleLabel_ = nullptr;
    QLabel* subtitleLabel_ = nullptr;
    QLabel* privacyNoticeLabel_ = nullptr;

    // Input Monitoring card
    QWidget* imCard_ = nullptr;
    QLabel* imStatusBadge_ = nullptr;
    QLabel* imDescLabel_ = nullptr;
    QPushButton* imActionButton_ = nullptr;

    // Accessibility card
    QWidget* axCard_ = nullptr;
    QLabel* axStatusBadge_ = nullptr;
    QLabel* axDescLabel_ = nullptr;
    QPushButton* axActionButton_ = nullptr;

    // Footer
    QLabel* allSetLabel_ = nullptr;
    QPushButton* skipButton_ = nullptr;
    QPushButton* doneButton_ = nullptr;

    QTimer* pollTimer_ = nullptr;

    void setupUi();
    void applyTheme();
    QWidget* createCard(const QString& title, const QString& whyText, const QString& howText,
                        QLabel*& statusBadge, QPushButton*& actionButton, const char* slot);
};
