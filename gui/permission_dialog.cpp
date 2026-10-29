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
#include <QPainter>

namespace {
class PermissionBadge : public QWidget {
    int type_; // 0: Keyboard (Input Monitoring), 1: Accessibility / Pointer
public:
    explicit PermissionBadge(int t, QWidget* parent = nullptr) : QWidget(parent), type_(t) {
        setFixedSize(36, 36);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Monochrome container: #F3F4F6 with #E5E7EB border
        p.setBrush(QColor(243, 244, 246));
        p.setPen(QPen(QColor(229, 231, 235), 1.0));
        p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);

        // Crisp dark charcoal vector icon #111827
        p.setPen(QPen(QColor(17, 24, 39), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);

        QRectF r(8, 8, 20, 20);
        qreal cx = r.center().x();
        qreal cy = r.center().y();
        qreal sz = r.width();

        if (type_ == 0) {
            // Keyboard outline
            p.drawRoundedRect(QRectF(cx - sz * 0.42, cy - sz * 0.28, sz * 0.84, sz * 0.56), 2.5, 2.5);
            // Key rows
            p.drawLine(QPointF(cx - sz * 0.25, cy - sz * 0.08), QPointF(cx - sz * 0.15, cy - sz * 0.08));
            p.drawLine(QPointF(cx - sz * 0.05, cy - sz * 0.08), QPointF(cx + sz * 0.05, cy - sz * 0.08));
            p.drawLine(QPointF(cx + sz * 0.15, cy - sz * 0.08), QPointF(cx + sz * 0.25, cy - sz * 0.08));
            p.drawLine(QPointF(cx - sz * 0.22, cy + sz * 0.12), QPointF(cx + sz * 0.22, cy + sz * 0.12));
        } else {
            // Accessibility figure / target
            p.drawEllipse(QPointF(cx, cy - sz * 0.24), sz * 0.14, sz * 0.14);
            // Torso & arms
            p.drawLine(QPointF(cx - sz * 0.35, cy - sz * 0.04), QPointF(cx + sz * 0.35, cy - sz * 0.04));
            p.drawLine(QPointF(cx, cy - sz * 0.04), QPointF(cx, cy + sz * 0.18));
            // Legs
            p.drawLine(QPointF(cx, cy + sz * 0.18), QPointF(cx - sz * 0.22, cy + sz * 0.4));
            p.drawLine(QPointF(cx, cy + sz * 0.18), QPointF(cx + sz * 0.22, cy + sz * 0.4));
        }
    }
};
} // namespace

bool PermissionDialog::areAllPermissionsGranted() {
    bool im = checkInputMonitoringPermission();
    bool ax = checkAccessibilityPermission(false);
    return im && ax;
}

PermissionDialog::PermissionDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("System Permissions"));
    setFixedSize(540, 440);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    setupUi();
    applyTheme();

    checkPermissions();

    pollTimer_ = new QTimer(this);
    connect(pollTimer_, &QTimer::timeout, this, &PermissionDialog::checkPermissions);
    pollTimer_->start(500);
}

void PermissionDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 28, 28, 24);
    mainLayout->setSpacing(14);

    // Header Title
    titleLabel_ = new QLabel(tr("Permissions Required"), this);
    titleLabel_->setObjectName("dialogTitle");
    mainLayout->addWidget(titleLabel_);

    // Subtitle
    descriptionLabel_ = new QLabel(
        tr("Inline works strictly on-device to detect typos and insert real-time corrections. "
           "macOS requires two permissions to observe typing and insert text replacement."), this);
    descriptionLabel_->setWordWrap(true);
    descriptionLabel_->setObjectName("dialogSubtitle");
    mainLayout->addWidget(descriptionLabel_);

    mainLayout->addSpacing(4);

    // Permission Card Container
    auto* card = createPermissionCard();
    mainLayout->addWidget(card);

    mainLayout->addSpacing(4);

    // Status Summary text
    statusSummaryLabel_ = new QLabel(this);
    statusSummaryLabel_->setObjectName("statusSummary");
    statusSummaryLabel_->setText(tr("Awaiting system authorization..."));
    mainLayout->addWidget(statusSummaryLabel_);

    mainLayout->addStretch();

    // Footer actions
    auto* footerRow = new QHBoxLayout();
    footerRow->setContentsMargins(0, 0, 0, 0);
    footerRow->setSpacing(10);

    skipButton_ = new QPushButton(tr("Dismiss"), this);
    skipButton_->setObjectName("secondaryBtn");
    connect(skipButton_, &QPushButton::clicked, this, &QDialog::reject);

    continueButton_ = new QPushButton(tr("Continue"), this);
    continueButton_->setObjectName("primaryBtn");
    connect(continueButton_, &QPushButton::clicked, this, &QDialog::accept);

    footerRow->addWidget(skipButton_);
    footerRow->addStretch();
    footerRow->addWidget(continueButton_);
    mainLayout->addLayout(footerRow);
}

QWidget* PermissionDialog::createPermissionCard() {
    auto* card = new QWidget(this);
    card->setObjectName("permissionCard");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    // Row 1: Input Monitoring
    cardLayout->addWidget(createPermissionRow(
        0,
        tr("Input Monitoring"),
        tr("Observes keystrokes locally to detect typos as you type."),
        imStatusLabel_,
        imActionButton_,
        SLOT(onOpenInputMonitoringClicked()),
        false
    ));

    // Divider
    auto* line = new QFrame(card);
    line->setObjectName("rowDivider");
    line->setFrameShape(QFrame::HLine);
    cardLayout->addWidget(line);

    // Row 2: Accessibility
    cardLayout->addWidget(createPermissionRow(
        1,
        tr("Accessibility"),
        tr("Inserts autocorrected words seamlessly across any app."),
        axStatusLabel_,
        axActionButton_,
        SLOT(onOpenAccessibilityClicked()),
        true
    ));

    return card;
}

QWidget* PermissionDialog::createPermissionRow(int iconType, const QString& title, const QString& subtitle,
                                               QLabel*& statusLabel, QPushButton*& actionButton,
                                               const char* slot, bool isLast) {
    (void)isLast;
    auto* rowWidget = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(16, 14, 16, 14);
    rowLayout->setSpacing(12);

    // Vector Icon badge
    rowLayout->addWidget(new PermissionBadge(iconType, rowWidget));

    // Text labels
    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);

    auto* titleLbl = new QLabel(title, rowWidget);
    titleLbl->setStyleSheet("font-size: 13px; font-weight: 600; color: #111827;");
    textLayout->addWidget(titleLbl);

    auto* subLbl = new QLabel(subtitle, rowWidget);
    subLbl->setStyleSheet("font-size: 11px; font-weight: 400; color: #6B7280;");
    subLbl->setWordWrap(true);
    textLayout->addWidget(subLbl);

    rowLayout->addLayout(textLayout, 1);

    // Status Pill
    statusLabel = new QLabel(tr("Required"), rowWidget);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet(
        "background-color: #FEF2F2; color: #DC2626; font-size: 11px; font-weight: 600; "
        "padding: 4px 10px; border-radius: 10px; border: 1px solid #FECACA;"
    );
    rowLayout->addWidget(statusLabel);

    // Action button
    actionButton = new QPushButton(tr("Open Settings"), rowWidget);
    actionButton->setObjectName("rowActionBtn");
    connect(actionButton, SIGNAL(clicked()), this, slot);
    rowLayout->addWidget(actionButton);

    return rowWidget;
}

void PermissionDialog::applyTheme() {
    setStyleSheet(
        "PermissionDialog {"
        "  background-color: #F8F9FA;"
        "  color: #111827;"
        "  font-family: -apple-system, BlinkMacSystemFont, 'SF Pro Text', 'Helvetica Neue', Arial, sans-serif;"
        "}"
        "#dialogTitle {"
        "  font-size: 22px;"
        "  font-weight: 700;"
        "  color: #111827;"
        "}"
        "#dialogSubtitle {"
        "  font-size: 13px;"
        "  line-height: 1.45;"
        "  color: #6B7280;"
        "}"
        "#permissionCard {"
        "  background-color: #FFFFFF;"
        "  border: 1px solid #E5E7EB;"
        "  border-radius: 12px;"
        "}"
        "#rowDivider {"
        "  color: #F3F4F6;"
        "  background-color: #F3F4F6;"
        "  height: 1px;"
        "  border: none;"
        "}"
        "#statusSummary {"
        "  font-size: 11px;"
        "  color: #6B7280;"
        "  font-weight: 500;"
        "}"
        "#rowActionBtn {"
        "  background-color: #F3F4F6;"
        "  color: #111827;"
        "  border: 1px solid #E5E7EB;"
        "  border-radius: 6px;"
        "  padding: 6px 12px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
        "#rowActionBtn:hover {"
        "  background-color: #E5E7EB;"
        "}"
        "#rowActionBtn:disabled {"
        "  background-color: #F9FAFB;"
        "  color: #9CA3AF;"
        "  border-color: #E5E7EB;"
        "}"
        "#primaryBtn {"
        "  background-color: #111827;"
        "  color: #FFFFFF;"
        "  border: 1px solid #111827;"
        "  border-radius: 8px;"
        "  padding: 8px 20px;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "}"
        "#primaryBtn:hover {"
        "  background-color: #1F2937;"
        "}"
        "#primaryBtn:disabled {"
        "  background-color: #9CA3AF;"
        "  border-color: #9CA3AF;"
        "}"
        "#secondaryBtn {"
        "  background-color: transparent;"
        "  color: #6B7280;"
        "  border: 1px solid #D1D5DB;"
        "  border-radius: 8px;"
        "  padding: 8px 16px;"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "}"
        "#secondaryBtn:hover {"
        "  background-color: #F3F4F6;"
        "  color: #111827;"
        "}"
    );
}

void PermissionDialog::checkPermissions() {
    bool im = checkInputMonitoringPermission();
    bool ax = checkAccessibilityPermission(false);

    if (im) {
        imStatusLabel_->setText(tr("Granted"));
        imStatusLabel_->setStyleSheet(
            "background-color: #ECFDF5; color: #059669; font-size: 11px; font-weight: 600; "
            "padding: 4px 10px; border-radius: 10px; border: 1px solid #A7F3D0;"
        );
        imActionButton_->setText(tr("Enabled"));
        imActionButton_->setEnabled(false);
    } else {
        imStatusLabel_->setText(tr("Required"));
        imStatusLabel_->setStyleSheet(
            "background-color: #FEF2F2; color: #DC2626; font-size: 11px; font-weight: 600; "
            "padding: 4px 10px; border-radius: 10px; border: 1px solid #FECACA;"
        );
        imActionButton_->setText(tr("Open Settings"));
        imActionButton_->setEnabled(true);
    }

    if (ax) {
        axStatusLabel_->setText(tr("Granted"));
        axStatusLabel_->setStyleSheet(
            "background-color: #ECFDF5; color: #059669; font-size: 11px; font-weight: 600; "
            "padding: 4px 10px; border-radius: 10px; border: 1px solid #A7F3D0;"
        );
        axActionButton_->setText(tr("Enabled"));
        axActionButton_->setEnabled(false);
    } else {
        axStatusLabel_->setText(tr("Required"));
        axStatusLabel_->setStyleSheet(
            "background-color: #FEF2F2; color: #DC2626; font-size: 11px; font-weight: 600; "
            "padding: 4px 10px; border-radius: 10px; border: 1px solid #FECACA;"
        );
        axActionButton_->setText(tr("Open Settings"));
        axActionButton_->setEnabled(true);
    }

    if (im && ax) {
        statusSummaryLabel_->setText(tr("All system permissions are granted and ready."));
        statusSummaryLabel_->setStyleSheet("color: #059669; font-size: 11px; font-weight: 600;");
        continueButton_->setEnabled(true);
        continueButton_->setText(tr("Get Started"));
        skipButton_->setText(tr("Close"));
    } else {
        statusSummaryLabel_->setText(tr("Waiting for permissions in macOS System Settings..."));
        statusSummaryLabel_->setStyleSheet("color: #6B7280; font-size: 11px; font-weight: 500;");
        continueButton_->setText(tr("Continue"));
    }
}

void PermissionDialog::onOpenInputMonitoringClicked() {
    openInputMonitoringSettings();
}

void PermissionDialog::onOpenAccessibilityClicked() {
    checkAccessibilityPermission(true);
    openAccessibilitySettings();
}
