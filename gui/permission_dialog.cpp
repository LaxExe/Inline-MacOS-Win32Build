#include "gui/permission_dialog.h"
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
#include "platform/mac/permissions.h"
#else
#include "platform/win/permissions.h"
#endif

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QIcon>
#include <QApplication>

bool PermissionDialog::areAllPermissionsGranted() {
    bool im = checkInputMonitoringPermission();
    bool ax = checkAccessibilityPermission(false);
    return im && ax;
}

PermissionDialog::PermissionDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Welcome to Inline"));
    setFixedSize(540, 620);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    setupUi();
    applyTheme();

    checkPermissions();

    pollTimer_ = new QTimer(this);
    connect(pollTimer_, &QTimer::timeout, this, &PermissionDialog::checkPermissions);
    pollTimer_->start(800);
}

void PermissionDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 24, 28, 24);
    mainLayout->setSpacing(14);

    // 1. Header
    titleLabel_ = new QLabel(tr("Permissions Required"), this);
    titleLabel_->setStyleSheet("font-size: 20px; font-weight: 700; color: #0F172A;");

    subtitleLabel_ = new QLabel(
        tr("Inline provides instant, system-wide autocorrection across all your apps. "
           "To listen for typos and replace words on macOS, two permissions are needed:"), this);
    subtitleLabel_->setWordWrap(true);
    subtitleLabel_->setStyleSheet("font-size: 13px; color: #475569; line-height: 1.4;");

    mainLayout->addWidget(titleLabel_);
    mainLayout->addWidget(subtitleLabel_);

    // 2. Privacy Callout Banner
    auto* privacyBanner = new QFrame(this);
    privacyBanner->setObjectName("privacyBanner");
    privacyBanner->setStyleSheet(
        "#privacyBanner {"
        "  background-color: #F0FDF4;"
        "  border: 1px solid #BBF7D0;"
        "  border-radius: 8px;"
        "  padding: 10px;"
        "}"
    );
    auto* privacyLayout = new QHBoxLayout(privacyBanner);
    privacyLayout->setContentsMargins(12, 8, 12, 8);
    privacyLayout->setSpacing(10);

    auto* lockIcon = new QLabel(tr("🔒"), privacyBanner);
    lockIcon->setStyleSheet("font-size: 18px;");
    privacyLayout->addWidget(lockIcon);

    privacyNoticeLabel_ = new QLabel(
        tr("<b>100% Private & On-Device:</b> Keystrokes are processed locally and never "
           "recorded or sent to any server. Passwords and secure fields are automatically ignored."),
        privacyBanner);
    privacyNoticeLabel_->setWordWrap(true);
    privacyNoticeLabel_->setStyleSheet("font-size: 12px; color: #166534; line-height: 1.3;");
    privacyLayout->addWidget(privacyNoticeLabel_, 1);

    mainLayout->addWidget(privacyBanner);

    // 3. Permission Cards
    imCard_ = createCard(
        tr("Input Monitoring"),
        tr("Allows Inline to observe keystrokes so it can detect mistyped words in real time."),
        tr("Used strictly on-device to match typos against local dictionaries."),
        imStatusBadge_, imActionButton_, SLOT(onOpenInputMonitoringClicked())
    );
    mainLayout->addWidget(imCard_);

    axCard_ = createCard(
        tr("Accessibility"),
        tr("Allows Inline to seamlessly replace misspelled words right where your cursor is."),
        tr("Used solely to backspace the mistyped letters and inject the corrected word."),
        axStatusBadge_, axActionButton_, SLOT(onOpenAccessibilityClicked())
    );
    mainLayout->addWidget(axCard_);

    // 4. Status / All-Set Message
    allSetLabel_ = new QLabel(this);
    allSetLabel_->setAlignment(Qt::AlignCenter);
    allSetLabel_->setStyleSheet("font-size: 13px; font-weight: 600; color: #15803D; padding: 4px;");
    allSetLabel_->setText(tr("Waiting for permissions in System Settings..."));
    mainLayout->addWidget(allSetLabel_);

    // 5. Footer Buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);

    skipButton_ = new QPushButton(tr("Skip for Now"), this);
    skipButton_->setStyleSheet(
        "QPushButton {"
        "  background-color: transparent;"
        "  color: #64748B;"
        "  border: 1px solid #CBD5E1;"
        "  border-radius: 6px;"
        "  padding: 8px 16px;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "  background-color: #F1F5F9;"
        "  color: #334155;"
        "}"
    );
    connect(skipButton_, &QPushButton::clicked, this, &QDialog::reject);

    doneButton_ = new QPushButton(tr("Get Started"), this);
    doneButton_->setStyleSheet(
        "QPushButton {"
        "  background-color: #2563EB;"
        "  color: #FFFFFF;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 8px 20px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  background-color: #1D4ED8;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #94A3B8;"
        "}"
    );
    connect(doneButton_, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addWidget(skipButton_);
    buttonLayout->addStretch();
    buttonLayout->addWidget(doneButton_);

    mainLayout->addLayout(buttonLayout);
}

QWidget* PermissionDialog::createCard(const QString& title, const QString& whyText, const QString& howText,
                                      QLabel*& statusBadge, QPushButton*& actionButton, const char* slot) {
    auto* card = new QFrame(this);
    card->setObjectName("permCard");
    card->setStyleSheet(
        "#permCard {"
        "  background-color: #FFFFFF;"
        "  border: 1px solid #E2E8F0;"
        "  border-radius: 10px;"
        "  padding: 12px 14px;"
        "}"
    );

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(14, 12, 14, 12);
    cardLayout->setSpacing(8);

    // Top row: Title + Status Pill
    auto* headerRow = new QHBoxLayout();
    auto* titleLbl = new QLabel(title, card);
    titleLbl->setStyleSheet("font-size: 14px; font-weight: 600; color: #1E293B;");

    statusBadge = new QLabel(tr("Action Required"), card);
    statusBadge->setStyleSheet(
        "background-color: #FEF3C7; color: #92400E; "
        "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: 600;"
    );

    headerRow->addWidget(titleLbl);
    headerRow->addStretch();
    headerRow->addWidget(statusBadge);
    cardLayout->addLayout(headerRow);

    // Descriptions
    auto* whyLbl = new QLabel(QString("<b>Why:</b> %1").arg(whyText), card);
    whyLbl->setWordWrap(true);
    whyLbl->setStyleSheet("font-size: 12px; color: #475569;");
    cardLayout->addWidget(whyLbl);

    auto* howLbl = new QLabel(QString("<b>How:</b> %1").arg(howText), card);
    howLbl->setWordWrap(true);
    howLbl->setStyleSheet("font-size: 12px; color: #64748B;");
    cardLayout->addWidget(howLbl);

    // Action button row
    auto* actionRow = new QHBoxLayout();
    actionButton = new QPushButton(tr("Open System Settings →"), card);
    actionButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #F1F5F9;"
        "  color: #2563EB;"
        "  border: 1px solid #CBD5E1;"
        "  border-radius: 6px;"
        "  padding: 5px 12px;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  background-color: #E2E8F0;"
        "}"
        "QPushButton:disabled {"
        "  color: #15803D;"
        "  background-color: #DCFCE7;"
        "  border: 1px solid #86EFAC;"
        "}"
    );
    connect(actionButton, SIGNAL(clicked()), this, slot);

    actionRow->addStretch();
    actionRow->addWidget(actionButton);
    cardLayout->addLayout(actionRow);

    return card;
}

void PermissionDialog::applyTheme() {
    setStyleSheet(
        "PermissionDialog {"
        "  background-color: #F8FAFC;"
        "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;"
        "}"
    );
}

void PermissionDialog::checkPermissions() {
    bool im = checkInputMonitoringPermission();
    bool ax = checkAccessibilityPermission(false);

    // Update Input Monitoring card
    if (im) {
        imStatusBadge_->setText(tr("✓ Granted"));
        imStatusBadge_->setStyleSheet(
            "background-color: #DCFCE7; color: #166534; "
            "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: 600;"
        );
        imActionButton_->setText(tr("✓ Enabled"));
        imActionButton_->setEnabled(false);
    } else {
        imStatusBadge_->setText(tr("Action Required"));
        imStatusBadge_->setStyleSheet(
            "background-color: #FEF3C7; color: #92400E; "
            "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: 600;"
        );
        imActionButton_->setText(tr("Open System Settings →"));
        imActionButton_->setEnabled(true);
    }

    // Update Accessibility card
    if (ax) {
        axStatusBadge_->setText(tr("✓ Granted"));
        axStatusBadge_->setStyleSheet(
            "background-color: #DCFCE7; color: #166534; "
            "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: 600;"
        );
        axActionButton_->setText(tr("✓ Enabled"));
        axActionButton_->setEnabled(false);
    } else {
        axStatusBadge_->setText(tr("Action Required"));
        axStatusBadge_->setStyleSheet(
            "background-color: #FEF3C7; color: #92400E; "
            "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: 600;"
        );
        axActionButton_->setText(tr("Open System Settings →"));
        axActionButton_->setEnabled(true);
    }

    if (im && ax) {
        allSetLabel_->setText(tr("🎉 All permissions granted! Inline is ready."));
        allSetLabel_->setStyleSheet("font-size: 13px; font-weight: 600; color: #15803D; padding: 4px;");
        doneButton_->setEnabled(true);
        doneButton_->setText(tr("Get Started"));
        skipButton_->setText(tr("Close"));
    } else {
        allSetLabel_->setText(tr("Toggle the switch in System Settings to enable permissions in real time."));
        allSetLabel_->setStyleSheet("font-size: 12px; font-weight: 500; color: #B45309; padding: 4px;");
    }
}

void PermissionDialog::onOpenInputMonitoringClicked() {
    openInputMonitoringSettings();
}

void PermissionDialog::onOpenAccessibilityClicked() {
    // Prompting registers the app with macOS Accessibility if not already in the list
    checkAccessibilityPermission(true);
    openAccessibilitySettings();
}
