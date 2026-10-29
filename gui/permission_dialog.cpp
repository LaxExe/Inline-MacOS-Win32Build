#include "gui/permission_dialog.h"
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
#include "platform/mac/permissions.h"
#else
#include "platform/win/permissions.h"
#endif

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QApplication>

bool PermissionDialog::areAllPermissionsGranted() {
    bool im = checkInputMonitoringPermission();
    bool ax = checkAccessibilityPermission(false);
    return im && ax;
}

PermissionDialog::PermissionDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("System Permissions"));
    setFixedSize(540, 480);
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
    mainLayout->setContentsMargins(28, 28, 28, 24);
    mainLayout->setSpacing(0);

    // 1. Header Section
    categoryLabel_ = new QLabel(tr("SYSTEM CONFIGURATION"), this);
    categoryLabel_->setObjectName("categoryLabel");
    mainLayout->addWidget(categoryLabel_);

    titleLabel_ = new QLabel(tr("macOS Permissions Required"), this);
    titleLabel_->setObjectName("titleLabel");
    mainLayout->addWidget(titleLabel_);

    mainLayout->addSpacing(8);

    descriptionLabel_ = new QLabel(
        tr("Inline processes keystrokes strictly on-device to detect typographical errors and perform "
           "in-place text replacement. macOS requires explicit authorization for the background event "
           "tap and accessibility injection pipelines."), this);
    descriptionLabel_->setWordWrap(true);
    descriptionLabel_->setObjectName("descriptionLabel");
    mainLayout->addWidget(descriptionLabel_);

    mainLayout->addSpacing(20);

    // 2. Telemetry Ribbon
    auto* telemetryFrame = new QFrame(this);
    telemetryFrame->setObjectName("telemetryFrame");
    auto* telemetryLayout = new QHBoxLayout(telemetryFrame);
    telemetryLayout->setContentsMargins(12, 8, 12, 8);
    telemetryLayout->setSpacing(0);

    auto* telemetryText = new QLabel(
        tr("PRIVACY: 100% LOCAL  |  NETWORK: AIR-GAPPED  |  PASSWORDS: RAW SUPPRESSION"), telemetryFrame);
    telemetryText->setObjectName("telemetryText");
    telemetryLayout->addWidget(telemetryText);
    mainLayout->addWidget(telemetryFrame);

    mainLayout->addSpacing(16);

    // 3. Structured Subsystems Table
    auto* tableContainer = new QFrame(this);
    tableContainer->setObjectName("tableContainer");
    auto* tableLayout = new QVBoxLayout(tableContainer);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    tableLayout->setSpacing(0);

    // Table Header
    auto* headerRow = new QWidget(tableContainer);
    auto* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(14, 8, 14, 8);
    headerLayout->setSpacing(12);

    auto* col1 = new QLabel(tr("SUBSYSTEM"), headerRow);
    col1->setObjectName("tableHeader");
    col1->setFixedWidth(140);

    auto* col2 = new QLabel(tr("FUNCTION"), headerRow);
    col2->setObjectName("tableHeader");

    auto* col3 = new QLabel(tr("STATUS"), headerRow);
    col3->setObjectName("tableHeader");
    col3->setFixedWidth(90);

    auto* col4 = new QLabel(tr("ACTION"), headerRow);
    col4->setObjectName("tableHeader");
    col4->setFixedWidth(100);

    headerLayout->addWidget(col1);
    headerLayout->addWidget(col2, 1);
    headerLayout->addWidget(col3);
    headerLayout->addWidget(col4);
    tableLayout->addWidget(headerRow);

    auto* line1 = new QFrame(tableContainer);
    line1->setObjectName("hairline");
    line1->setFrameShape(QFrame::HLine);
    tableLayout->addWidget(line1);

    // Row 1: Input Monitoring
    tableLayout->addWidget(createSubsystemRow(
        tr("Input Monitoring"),
        tr("Keystroke observation tap"),
        imStatusLabel_, imActionButton_, SLOT(onOpenInputMonitoringClicked())
    ));

    auto* line2 = new QFrame(tableContainer);
    line2->setObjectName("hairline");
    line2->setFrameShape(QFrame::HLine);
    tableLayout->addWidget(line2);

    // Row 2: Accessibility
    tableLayout->addWidget(createSubsystemRow(
        tr("Accessibility"),
        tr("Synthetic replacement injection"),
        axStatusLabel_, axActionButton_, SLOT(onOpenAccessibilityClicked())
    ));

    mainLayout->addWidget(tableContainer);

    mainLayout->addSpacing(16);

    // 4. Live Status Summary
    statusSummaryLabel_ = new QLabel(this);
    statusSummaryLabel_->setObjectName("statusSummary");
    statusSummaryLabel_->setText(tr("Awaiting system authorization..."));
    mainLayout->addWidget(statusSummaryLabel_);

    mainLayout->addStretch();

    // 5. Actions Footer
    auto* footerRow = new QHBoxLayout();
    footerRow->setContentsMargins(0, 0, 0, 0);
    footerRow->setSpacing(10);

    skipButton_ = new QPushButton(tr("Dismiss"), this);
    skipButton_->setObjectName("secondaryBtn");
    connect(skipButton_, &QPushButton::clicked, this, &QDialog::reject);

    continueButton_ = new QPushButton(tr("Proceed"), this);
    continueButton_->setObjectName("primaryBtn");
    connect(continueButton_, &QPushButton::clicked, this, &QDialog::accept);

    footerRow->addWidget(skipButton_);
    footerRow->addStretch();
    footerRow->addWidget(continueButton_);
    mainLayout->addLayout(footerRow);
}

QWidget* PermissionDialog::createSubsystemRow(const QString& name, const QString& purpose,
                                              QLabel*& statusLabel, QPushButton*& actionButton,
                                              const char* slot) {
    auto* rowWidget = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(14, 10, 14, 10);
    rowLayout->setSpacing(12);

    auto* nameLabel = new QLabel(name, rowWidget);
    nameLabel->setObjectName("rowName");
    nameLabel->setFixedWidth(140);

    auto* descLabel = new QLabel(purpose, rowWidget);
    descLabel->setObjectName("rowDesc");

    statusLabel = new QLabel(tr("[ MISSING ]"), rowWidget);
    statusLabel->setObjectName("statusIndicator");
    statusLabel->setFixedWidth(90);

    actionButton = new QPushButton(tr("Authorize"), rowWidget);
    actionButton->setObjectName("tableActionBtn");
    actionButton->setFixedWidth(100);
    connect(actionButton, SIGNAL(clicked()), this, slot);

    rowLayout->addWidget(nameLabel);
    rowLayout->addWidget(descLabel, 1);
    rowLayout->addWidget(statusLabel);
    rowLayout->addWidget(actionButton);

    return rowWidget;
}

void PermissionDialog::applyTheme() {
    setStyleSheet(
        "PermissionDialog {"
        "  background-color: #FFFFFF;"
        "  color: #111827;"
        "  font-family: -apple-system, BlinkMacSystemFont, 'SF Pro Text', 'Helvetica Neue', Arial, sans-serif;"
        "}"
        "#categoryLabel {"
        "  font-size: 10px;"
        "  font-weight: 700;"
        "  letter-spacing: 0.1em;"
        "  color: #71717A;"
        "}"
        "#titleLabel {"
        "  font-size: 18px;"
        "  font-weight: 700;"
        "  color: #09090B;"
        "  margin-top: 2px;"
        "}"
        "#descriptionLabel {"
        "  font-size: 12px;"
        "  line-height: 1.45;"
        "  color: #52525B;"
        "}"
        "#telemetryFrame {"
        "  background-color: #F4F4F5;"
        "  border: 1px solid #E4E4E7;"
        "}"
        "#telemetryText {"
        "  font-family: 'SF Mono', Menlo, Monaco, monospace;"
        "  font-size: 10px;"
        "  color: #3F3F46;"
        "}"
        "#tableContainer {"
        "  background-color: #FFFFFF;"
        "  border: 1px solid #E4E4E7;"
        "}"
        "#tableHeader {"
        "  font-size: 10px;"
        "  font-weight: 700;"
        "  letter-spacing: 0.06em;"
        "  color: #71717A;"
        "}"
        "#hairline {"
        "  color: #E4E4E7;"
        "  background-color: #E4E4E7;"
        "  height: 1px;"
        "  border: none;"
        "}"
        "#rowName {"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  color: #09090B;"
        "}"
        "#rowDesc {"
        "  font-size: 12px;"
        "  color: #52525B;"
        "}"
        "#statusIndicator {"
        "  font-family: 'SF Mono', Menlo, Monaco, monospace;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
        "#statusSummary {"
        "  font-family: 'SF Mono', Menlo, Monaco, monospace;"
        "  font-size: 11px;"
        "  color: #71717A;"
        "}"
        "#tableActionBtn {"
        "  background-color: #FFFFFF;"
        "  color: #18181B;"
        "  border: 1px solid #D4D4D8;"
        "  border-radius: 2px;"
        "  padding: 4px 8px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
        "#tableActionBtn:hover {"
        "  background-color: #F4F4F5;"
        "  border-color: #A1A1AA;"
        "}"
        "#tableActionBtn:disabled {"
        "  color: #A1A1AA;"
        "  border-color: #E4E4E7;"
        "  background-color: #FAFAFA;"
        "}"
        "#primaryBtn {"
        "  background-color: #18181B;"
        "  color: #FAFAFA;"
        "  border: 1px solid #18181B;"
        "  border-radius: 2px;"
        "  padding: 6px 16px;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "}"
        "#primaryBtn:hover {"
        "  background-color: #27272A;"
        "}"
        "#secondaryBtn {"
        "  background-color: #FFFFFF;"
        "  color: #52525B;"
        "  border: 1px solid #D4D4D8;"
        "  border-radius: 2px;"
        "  padding: 6px 14px;"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "}"
        "#secondaryBtn:hover {"
        "  background-color: #F4F4F5;"
        "  color: #18181B;"
        "}"
    );
}

void PermissionDialog::checkPermissions() {
    bool im = checkInputMonitoringPermission();
    bool ax = checkAccessibilityPermission(false);

    // Update Input Monitoring row
    if (im) {
        imStatusLabel_->setText(tr("GRANTED"));
        imStatusLabel_->setStyleSheet("color: #15803D; font-weight: 700;");
        imActionButton_->setText(tr("Configured"));
        imActionButton_->setEnabled(false);
    } else {
        imStatusLabel_->setText(tr("MISSING"));
        imStatusLabel_->setStyleSheet("color: #B91C1C; font-weight: 700;");
        imActionButton_->setText(tr("Authorize"));
        imActionButton_->setEnabled(true);
    }

    // Update Accessibility row
    if (ax) {
        axStatusLabel_->setText(tr("GRANTED"));
        axStatusLabel_->setStyleSheet("color: #15803D; font-weight: 700;");
        axActionButton_->setText(tr("Configured"));
        axActionButton_->setEnabled(false);
    } else {
        axStatusLabel_->setText(tr("MISSING"));
        axStatusLabel_->setStyleSheet("color: #B91C1C; font-weight: 700;");
        axActionButton_->setText(tr("Authorize"));
        axActionButton_->setEnabled(true);
    }

    if (im && ax) {
        statusSummaryLabel_->setText(tr("STATUS: ALL SUBSYSTEMS AUTHORIZED AND OPERATIONAL"));
        statusSummaryLabel_->setStyleSheet("color: #15803D; font-weight: 600;");
        continueButton_->setEnabled(true);
        continueButton_->setText(tr("Launch Engine"));
        skipButton_->setText(tr("Close"));
    } else {
        statusSummaryLabel_->setText(tr("STATUS: AWAITING SYSTEM PERMISSION AUTHORIZATION"));
        statusSummaryLabel_->setStyleSheet("color: #71717A; font-weight: 600;");
        continueButton_->setText(tr("Proceed"));
    }
}

void PermissionDialog::onOpenInputMonitoringClicked() {
    openInputMonitoringSettings();
}

void PermissionDialog::onOpenAccessibilityClicked() {
    checkAccessibilityPermission(true);
    openAccessibilitySettings();
}
