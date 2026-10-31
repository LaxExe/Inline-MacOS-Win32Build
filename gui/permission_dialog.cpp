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
#include <QPainterPath>

namespace {
// Pure Vector Icon Badge (36x36 monochrome container, #F3F4F6 bg, #E5E7EB border, #111827 ink)
// Icon types:
// 0: Keyboard (Input Monitoring)
// 1: Accessibility figure
// 2: Bolt (Speed / Spacebar evaluation)
// 3: Undo arrow (Single-Key Undo)
// 4: Book / Context (Bigram Context)
// 5: Split arrows (Run-Together Word Splitting)
// 6: Lock (On-Device Privacy & Secure Fields)
// 7: Code brackets (Slang & Developer Whitelist)
// 8: MenuBar icon (|▪)
// 9: Refresh / Reset
// 10: Ghost Text cursor + fading dashes
class WalkthroughBadge : public QWidget {
    int type_;
public:
    explicit WalkthroughBadge(int t, QWidget* parent = nullptr) : QWidget(parent), type_(t) {
        setFixedSize(36, 36);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        bool isDark = false;
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
        isDark = isSystemInDarkMode();
#endif
        // designlang flat tokens: #f5f5f5 in light, #1f1f1f in dark
        p.setBrush(isDark ? QColor(31, 31, 31) : QColor(245, 245, 245));
        p.setPen(QPen(isDark ? QColor(46, 46, 46) : QColor(229, 229, 229), 1.0));
        p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);

        // Crisp vector stroke #171717 in light, #FFFFFF in dark
        p.setPen(QPen(isDark ? QColor(255, 255, 255) : QColor(23, 23, 23), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);

        QRectF r(8, 8, 20, 20);
        qreal cx = r.center().x();
        qreal cy = r.center().y();
        qreal sz = r.width();

        switch (type_) {
            case 0: { // Keyboard outline
                p.drawRoundedRect(QRectF(cx - sz * 0.42, cy - sz * 0.28, sz * 0.84, sz * 0.56), 2.5, 2.5);
                p.drawLine(QPointF(cx - sz * 0.25, cy - sz * 0.08), QPointF(cx - sz * 0.15, cy - sz * 0.08));
                p.drawLine(QPointF(cx - sz * 0.05, cy - sz * 0.08), QPointF(cx + sz * 0.05, cy - sz * 0.08));
                p.drawLine(QPointF(cx + sz * 0.15, cy - sz * 0.08), QPointF(cx + sz * 0.25, cy - sz * 0.08));
                p.drawLine(QPointF(cx - sz * 0.22, cy + sz * 0.12), QPointF(cx + sz * 0.22, cy + sz * 0.12));
                break;
            }
            case 1: { // Accessibility figure
                p.drawEllipse(QPointF(cx, cy - sz * 0.24), sz * 0.14, sz * 0.14);
                p.drawLine(QPointF(cx - sz * 0.35, cy - sz * 0.04), QPointF(cx + sz * 0.35, cy - sz * 0.04));
                p.drawLine(QPointF(cx, cy - sz * 0.04), QPointF(cx, cy + sz * 0.18));
                p.drawLine(QPointF(cx, cy + sz * 0.18), QPointF(cx - sz * 0.22, cy + sz * 0.4));
                p.drawLine(QPointF(cx, cy + sz * 0.18), QPointF(cx + sz * 0.22, cy + sz * 0.4));
                break;
            }
            case 2: { // Lightning Bolt (Speed / Evaluation)
                QPainterPath path;
                path.moveTo(cx + sz * 0.05, cy - sz * 0.42);
                path.lineTo(cx - sz * 0.30, cy + sz * 0.04);
                path.lineTo(cx + sz * 0.02, cy + sz * 0.04);
                path.lineTo(cx - sz * 0.05, cy + sz * 0.42);
                path.lineTo(cx + sz * 0.30, cy - sz * 0.04);
                path.lineTo(cx - sz * 0.02, cy - sz * 0.04);
                path.closeSubpath();
                p.drawPath(path);
                break;
            }
            case 3: { // Undo curved arrow
                p.drawLine(QPointF(cx - sz * 0.35, cy - sz * 0.10), QPointF(cx - sz * 0.15, cy - sz * 0.30));
                p.drawLine(QPointF(cx - sz * 0.35, cy - sz * 0.10), QPointF(cx - sz * 0.15, cy + sz * 0.10));
                QPainterPath arc;
                arc.moveTo(cx - sz * 0.35, cy - sz * 0.10);
                arc.cubicTo(cx + sz * 0.35, cy - sz * 0.15, cx + sz * 0.40, cy + sz * 0.35, cx - sz * 0.15, cy + sz * 0.32);
                p.drawPath(arc);
                break;
            }
            case 4: { // Open Book (Bigram Context)
                p.drawRoundedRect(QRectF(cx - sz * 0.40, cy - sz * 0.30, sz * 0.38, sz * 0.60), 2, 2);
                p.drawRoundedRect(QRectF(cx + sz * 0.02, cy - sz * 0.30, sz * 0.38, sz * 0.60), 2, 2);
                break;
            }
            case 5: { // Word Split (Two columns / divider)
                p.drawLine(QPointF(cx, cy - sz * 0.36), QPointF(cx, cy + sz * 0.36));
                p.drawLine(QPointF(cx - sz * 0.12, cy), QPointF(cx - sz * 0.38, cy));
                p.drawLine(QPointF(cx - sz * 0.28, cy - sz * 0.14), QPointF(cx - sz * 0.38, cy));
                p.drawLine(QPointF(cx - sz * 0.28, cy + sz * 0.14), QPointF(cx - sz * 0.38, cy));
                p.drawLine(QPointF(cx + sz * 0.12, cy), QPointF(cx + sz * 0.38, cy));
                p.drawLine(QPointF(cx + sz * 0.28, cy - sz * 0.14), QPointF(cx + sz * 0.38, cy));
                p.drawLine(QPointF(cx + sz * 0.28, cy + sz * 0.14), QPointF(cx + sz * 0.38, cy));
                break;
            }
            case 6: { // Lock (On-Device Privacy)
                p.drawRoundedRect(QRectF(cx - sz * 0.32, cy - sz * 0.04, sz * 0.64, sz * 0.44), 3, 3);
                p.drawArc(QRectF(cx - sz * 0.20, cy - sz * 0.38, sz * 0.40, sz * 0.44), 0 * 16, 180 * 16);
                break;
            }
            case 7: { // Code Brackets (Slang & Dev Whitelist)
                p.drawLine(QPointF(cx - sz * 0.15, cy - sz * 0.26), QPointF(cx - sz * 0.38, cy));
                p.drawLine(QPointF(cx - sz * 0.38, cy), QPointF(cx - sz * 0.15, cy + sz * 0.26));
                p.drawLine(QPointF(cx + sz * 0.15, cy - sz * 0.26), QPointF(cx + sz * 0.38, cy));
                p.drawLine(QPointF(cx + sz * 0.38, cy), QPointF(cx + sz * 0.15, cy + sz * 0.26));
                break;
            }
            case 8: { // MenuBar Icon (|▪)
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(17, 24, 39));
                p.drawRoundedRect(QRectF(cx - 4.0, cy - 6.0, 2.4, 12.0), 0.6, 0.6);
                p.setBrush(QColor(22, 163, 74));
                p.drawRoundedRect(QRectF(cx + 1.0, cy + 2.2, 3.6, 3.6), 0.8, 0.8);
                break;
            }
            case 10: { // Ghost Text cursor bar + trailing fading dashes
                p.drawLine(QPointF(cx - sz * 0.36, cy - sz * 0.32), QPointF(cx - sz * 0.36, cy + sz * 0.32));
                p.drawLine(QPointF(cx - sz * 0.18, cy - sz * 0.08), QPointF(cx + sz * 0.12, cy - sz * 0.08));
                QPen fadedPen(QColor(107, 114, 128), 1.6, Qt::SolidLine, Qt::RoundCap);
                p.setPen(fadedPen);
                p.drawLine(QPointF(cx + sz * 0.22, cy - sz * 0.08), QPointF(cx + sz * 0.40, cy - sz * 0.08));
                p.drawLine(QPointF(cx - sz * 0.18, cy + sz * 0.16), QPointF(cx + sz * 0.26, cy + sz * 0.16));
                break;
            }
            default: { // Refresh / Reset loop
                p.drawArc(QRectF(cx - sz * 0.32, cy - sz * 0.32, sz * 0.64, sz * 0.64), 30 * 16, 290 * 16);
                p.drawLine(QPointF(cx + sz * 0.28, cy - sz * 0.16), QPointF(cx + sz * 0.32, cy - sz * 0.36));
                p.drawLine(QPointF(cx + sz * 0.28, cy - sz * 0.16), QPointF(cx + sz * 0.10, cy - sz * 0.18));
                break;
            }
        }
    }
};

// Interactive Animated Ghost Text Preview Banner (pure QPainter vector drawing)
class GhostDemoWidget : public QWidget {
    int phase_ = 0; // 0: typing, 1: ghost shown, 2: Tab pressed, 3: committed
    QTimer* animTimer_ = nullptr;
public:
    explicit GhostDemoWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(66);
        animTimer_ = new QTimer(this);
        connect(animTimer_, &QTimer::timeout, this, [this]() {
            phase_ = (phase_ + 1) % 4;
            update();
        });
        animTimer_->start(1100);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QRectF cardRect = rect().adjusted(0, 0, -1, -1);
        p.setBrush(QColor(255, 255, 255));
        p.setPen(QPen(QColor(229, 231, 235), 1.0));
        p.drawRoundedRect(cardRect, 12, 12);

        // Simulated editor field inside the card
        QRectF fieldRect(16, 13, width() - 120, 40);
        p.setBrush(QColor(249, 250, 251));
        p.setPen(QPen(QColor(229, 231, 235), 1.0));
        p.drawRoundedRect(fieldRect, 8, 8);

        QFont font = p.font();
        font.setPixelSize(13);
        font.setWeight(QFont::Medium);
        p.setFont(font);
        QFontMetrics fm(font);

        QString typed = (phase_ == 0) ? "I wanted to foll" : "I wanted to follow";
        QString ghost = " up on the meeting";

        qreal textX = fieldRect.left() + 12;
        qreal textY = fieldRect.center().y() + fm.ascent() * 0.36;

        // Draw typed text in dark ink
        p.setPen(QColor(17, 24, 39));
        if (phase_ == 3) {
            QString full = typed + ghost;
            p.drawText(QPointF(textX, textY), full);
            qreal caretX = textX + fm.horizontalAdvance(full) + 2;
            p.setPen(QPen(QColor(34, 197, 94), 1.8));
            p.drawLine(QPointF(caretX, fieldRect.center().y() - 8), QPointF(caretX, fieldRect.center().y() + 8));
        } else {
            p.drawText(QPointF(textX, textY), typed);
            qreal caretX = textX + fm.horizontalAdvance(typed) + 1;
            p.setPen(QPen(QColor(17, 24, 39), 1.6));
            p.drawLine(QPointF(caretX, fieldRect.center().y() - 8), QPointF(caretX, fieldRect.center().y() + 8));

            if (phase_ == 1 || phase_ == 2) {
                p.setPen(QColor(156, 163, 175));
                QString ghostStr = (phase_ == 0) ? "ow up on the meeting" : ghost;
                p.drawText(QPointF(caretX + 3, textY), ghostStr);
            }
        }

        // Right-side [Tab] keycap indicator
        QRectF tabRect(width() - 90, 18, 74, 30);
        bool tabActive = (phase_ == 2);
        p.setBrush(tabActive ? QColor(17, 24, 39) : QColor(243, 244, 246));
        p.setPen(QPen(tabActive ? QColor(17, 24, 39) : QColor(209, 213, 219), 1.0));
        p.drawRoundedRect(tabRect, 7, 7);

        QFont keyFont = p.font();
        keyFont.setPixelSize(11);
        keyFont.setWeight(QFont::Bold);
        p.setFont(keyFont);
        p.setPen(tabActive ? QColor(255, 255, 255) : QColor(55, 65, 81));
        p.drawText(tabRect, Qt::AlignCenter, tabActive ? "Tab  Accept" : "Tab");
    }
};
} // namespace

bool PermissionDialog::areAllPermissionsGranted() {
    bool im = checkInputMonitoringPermission();
    bool ax = checkAccessibilityPermission(false);
    return im && ax;
}

PermissionDialog::PermissionDialog(QWidget* parent, int initialSlide) : QDialog(parent) {
    setWindowTitle(tr("Inline — Walkthrough & Setup"));
    setFixedSize(580, 500);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    setupUi();
    applyTheme();
    setCurrentSlide(initialSlide);
    checkPermissions();

    pollTimer_ = new QTimer(this);
    connect(pollTimer_, &QTimer::timeout, this, &PermissionDialog::checkPermissions);
    pollTimer_->start(500);

#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    bringAppToFront();
#endif
}

void PermissionDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 24, 28, 22);
    mainLayout->setSpacing(10);

    // Top step pill
    auto* topRow = new QHBoxLayout();
    stepBadgeLabel_ = new QLabel(tr("STEP 1 OF 5 • HOW IT WORKS"), this);
    stepBadgeLabel_->setObjectName("stepBadge");
    topRow->addWidget(stepBadgeLabel_);
    topRow->addStretch();
    mainLayout->addLayout(topRow);

    // Header Title
    titleLabel_ = new QLabel(this);
    titleLabel_->setObjectName("dialogTitle");
    mainLayout->addWidget(titleLabel_);

    // Subtitle
    descriptionLabel_ = new QLabel(this);
    descriptionLabel_->setWordWrap(true);
    descriptionLabel_->setObjectName("dialogSubtitle");
    mainLayout->addWidget(descriptionLabel_);

    mainLayout->addSpacing(4);

    // Stacked Slides (0: How It Works, 1: Features, 2: Ghost Text, 3: Privacy & Controls, 4: Permissions)
    slideStack_ = new QStackedWidget(this);
    slideStack_->addWidget(createHowItWorksSlide());
    slideStack_->addWidget(createFeaturesSlide());
    slideStack_->addWidget(createGhostTextSlide());
    slideStack_->addWidget(createPrivacyAndControlsSlide());
    slideStack_->addWidget(createPermissionSlide());
    mainLayout->addWidget(slideStack_, 1);

    // Footer Navigation Bar
    auto* footerRow = new QHBoxLayout();
    footerRow->setContentsMargins(0, 6, 0, 0);
    footerRow->setSpacing(8);

    skipButton_ = new QPushButton(tr("Skip Walkthrough"), this);
    skipButton_->setObjectName("secondaryBtn");
    connect(skipButton_, &QPushButton::clicked, this, [this]() {
        if (slideStack_->currentIndex() < kPermissionSlide && !areAllPermissionsGranted()) {
            setCurrentSlide(kPermissionSlide);
        } else {
            accept();
        }
    });
    footerRow->addWidget(skipButton_);

    footerRow->addStretch();

    // Slide Dots (5 steps)
    for (int i = 0; i < kSlideCount; ++i) {
        auto* dot = new QPushButton(this);
        dot->setFixedSize(i == 0 ? 20 : 8, 8);
        dot->setCursor(Qt::PointingHandCursor);
        connect(dot, &QPushButton::clicked, this, [this, i]() {
            setCurrentSlide(i);
        });
        dotButtons_.push_back(dot);
        footerRow->addWidget(dot);
    }

    footerRow->addStretch();

    backButton_ = new QPushButton(tr("Back"), this);
    backButton_->setObjectName("secondaryBtn");
    connect(backButton_, &QPushButton::clicked, this, &PermissionDialog::onPrevSlideClicked);
    footerRow->addWidget(backButton_);

    nextButton_ = new QPushButton(tr("Next"), this);
    nextButton_->setObjectName("primaryBtn");
    connect(nextButton_, &QPushButton::clicked, this, &PermissionDialog::onNextSlideClicked);
    footerRow->addWidget(nextButton_);

    mainLayout->addLayout(footerRow);
}

QWidget* PermissionDialog::createInfoCard(const std::vector<std::tuple<int, QString, QString, QString>>& rows) {
    auto* wrapper = new QWidget(this);
    auto* wrapperLayout = new QVBoxLayout(wrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->setSpacing(0);

    auto* card = new QWidget(wrapper);
    card->setObjectName("permissionCard");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    for (size_t i = 0; i < rows.size(); ++i) {
        const auto& [iconType, title, subtitle, pillText] = rows[i];

        auto* rowWidget = new QWidget(card);
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(16, 13, 16, 13);
        rowLayout->setSpacing(12);

        rowLayout->addWidget(new WalkthroughBadge(iconType, rowWidget));

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

        if (!pillText.isEmpty()) {
            auto* pill = new QLabel(pillText, rowWidget);
            pill->setAlignment(Qt::AlignCenter);
            pill->setStyleSheet(
                "background-color: #F3F4F6; color: #111827; font-size: 11px; font-weight: 600; "
                "padding: 4px 10px; border-radius: 8px; border: 1px solid #E5E7EB;"
            );
            rowLayout->addWidget(pill);
        }

        cardLayout->addWidget(rowWidget);

        if (i + 1 < rows.size()) {
            auto* line = new QFrame(card);
            line->setObjectName("rowDivider");
            line->setFrameShape(QFrame::HLine);
            cardLayout->addWidget(line);
        }
    }

    wrapperLayout->addWidget(card);
    wrapperLayout->addStretch();
    return wrapper;
}

QWidget* PermissionDialog::createHowItWorksSlide() {
    return createInfoCard({
        {0, tr("1. Real-Time Keystroke Buffer"),
            tr("Observes characters locally in volatile memory with under 2 microseconds of latency. Resets automatically on mouse clicks or navigation keys."),
            tr("< 2 us")},
        {2, tr("2. Spacebar Evaluates, Enter Never Touches"),
            tr("Pressing Space checks your word against 82,000+ unigrams and 243,000+ bigrams. Pressing Return or Enter commits raw text untouched."),
            tr("Spacebar")},
        {5, tr("3. Atomic Common-Prefix Replacement"),
            tr("Only backspaces from the first mistyped character forward and injects the corrected suffix instantaneously across any macOS app."),
            tr("Instant")}
    });
}

QWidget* PermissionDialog::createFeaturesSlide() {
    return createInfoCard({
        {3, tr("Single-Key Undo on Backspace"),
            tr("If Inline corrects a word you intended to keep as-is, press Backspace once right after the correction to restore your exact raw keystrokes."),
            tr("1-Key Undo")},
        {4, tr("Context Bigrams & Run-Together Splitting"),
            tr("Disambiguates words using preceding context ('need help' vs 'say hello') and splits joined words automatically ('theclass' -> 'the class')."),
            tr("243K Bigrams")},
        {7, tr("Slang & Developer Jargon Whitelist"),
            tr("Protects 180+ internet acronyms (gng, brb, idk, ngl, tbh) and developer terms (repo, pr, cli, sdk, api, async) from false corrections."),
            tr("Protected")}
    });
}

QWidget* PermissionDialog::createGhostTextSlide() {
    auto* wrapper = new QWidget(this);
    auto* layout = new QVBoxLayout(wrapper);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    layout->addWidget(new GhostDemoWidget(wrapper));

    layout->addWidget(createInfoCard({
        {10, tr("Press Tab to Accept Inline Suggestions"),
             tr("Predicts the next 1-3 words right beside your cursor and wraps automatically to the text box width. Tab is only captured while a suggestion is visible."),
             tr("Tab Key")},
        {6,  tr("Bundled On-Device Language Model"),
             tr("Powered by a compact local language model and 243K bigrams with zero network calls. Disabled automatically in terminals and code editors."),
             tr("Cmd+Opt+Shift+G")}
    }), 1);

    return wrapper;
}

QWidget* PermissionDialog::createPrivacyAndControlsSlide() {
    return createInfoCard({
        {6, tr("100% On-Device Privacy & Password Protection"),
            tr("Zero network calls and zero disk logging. Secure password fields are excluded automatically, and buffers purge after 30s of inactivity."),
            tr("Offline")},
        {8, tr("Adaptive Menu Bar Indicator & Hotkeys"),
            tr("Lives in your macOS menu bar (|.). Toggle autocorrect with Cmd+Option+Shift+K or Ghost Text with Cmd+Option+Shift+G anytime."),
            tr("Cmd+Opt+Shift+K")},
        {0, tr("Live Playground & Custom Vocabulary"),
            tr("Use the Inline Settings window to test corrections and Ghost Text in real time, add ignored words, or configure excluded apps."),
            tr("Customizable")}
    });
}

QWidget* PermissionDialog::createPermissionSlide() {
    auto* wrapper = new QWidget(this);
    auto* wrapperLayout = new QVBoxLayout(wrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->setSpacing(10);

    auto* card = new QWidget(wrapper);
    card->setObjectName("permissionCard");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    // Row 1: Input Monitoring
    cardLayout->addWidget(createPermissionRow(
        0,
        tr("Input Monitoring"),
        tr("Observes keystrokes locally to detect word boundaries as you type."),
        imStatusLabel_,
        imActionButton_,
        SLOT(onOpenInputMonitoringClicked()),
        false
    ));

    auto* line1 = new QFrame(card);
    line1->setObjectName("rowDivider");
    line1->setFrameShape(QFrame::HLine);
    cardLayout->addWidget(line1);

    // Row 2: Accessibility
    cardLayout->addWidget(createPermissionRow(
        1,
        tr("Accessibility"),
        tr("Synthesizes backspaces, replacement text, and inline cursor tracking across macOS apps."),
        axStatusLabel_,
        axActionButton_,
        SLOT(onOpenAccessibilityClicked()),
        true
    ));

    wrapperLayout->addWidget(card);

    statusSummaryLabel_ = new QLabel(wrapper);
    statusSummaryLabel_->setObjectName("statusSummary");
    statusSummaryLabel_->setText(tr("Awaiting system authorization..."));
    wrapperLayout->addWidget(statusSummaryLabel_);

    wrapperLayout->addStretch();
    return wrapper;
}

QWidget* PermissionDialog::createPermissionRow(int iconType, const QString& title, const QString& subtitle,
                                               QLabel*& statusLabel, QPushButton*& actionButton,
                                               const char* slot, bool isLast) {
    (void)isLast;
    auto* rowWidget = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(16, 13, 16, 13);
    rowLayout->setSpacing(12);

    rowLayout->addWidget(new WalkthroughBadge(iconType, rowWidget));

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

    statusLabel = new QLabel(tr("Required"), rowWidget);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet(
        "background-color: #FEF2F2; color: #DC2626; font-size: 11px; font-weight: 600; "
        "padding: 4px 10px; border-radius: 10px; border: 1px solid #FECACA;"
    );
    rowLayout->addWidget(statusLabel);

    actionButton = new QPushButton(tr("Open Settings"), rowWidget);
    actionButton->setObjectName("rowActionBtn");
    connect(actionButton, SIGNAL(clicked()), this, slot);
    rowLayout->addWidget(actionButton);

    return rowWidget;
}

void PermissionDialog::setCurrentSlide(int index) {
    if (index < 0) index = 0;
    if (index >= kSlideCount) index = kSlideCount - 1;
    slideStack_->setCurrentIndex(index);
    updateSlideHeader();
}

void PermissionDialog::updateSlideHeader() {
    int idx = slideStack_->currentIndex();

    for (int i = 0; i < static_cast<int>(dotButtons_.size()); ++i) {
        bool active = (i == idx);
        dotButtons_[i]->setFixedSize(active ? 22 : 8, 8);
        dotButtons_[i]->setStyleSheet(active
            ? "background-color: #111827; border: none; border-radius: 4px;"
            : "background-color: #D1D5DB; border: none; border-radius: 4px;");
    }

    backButton_->setVisible(idx > 0);

    switch (idx) {
        case kHowItWorksSlide:
            stepBadgeLabel_->setText(tr("STEP 1 OF 5 • HOW IT WORKS"));
            titleLabel_->setText(tr("Welcome to Inline for macOS"));
            descriptionLabel_->setText(
                tr("Inline runs silently in your macOS menu bar to fix typos and predict words in real time. "
                   "Here is how the sub-millisecond correction pipeline works:"));
            nextButton_->setText(tr("Next: Features"));
            skipButton_->setText(areAllPermissionsGranted() ? tr("Close") : tr("Skip to Permissions"));
            break;
        case kFeaturesSlide:
            stepBadgeLabel_->setText(tr("STEP 2 OF 5 • CORE FEATURES"));
            titleLabel_->setText(tr("Intelligent Autocorrect Features"));
            descriptionLabel_->setText(
                tr("Designed for speed and accuracy whether you are writing messages, documents, or code:"));
            nextButton_->setText(tr("Next: Ghost Text"));
            skipButton_->setText(areAllPermissionsGranted() ? tr("Close") : tr("Skip to Permissions"));
            break;
        case kGhostTextSlide:
            stepBadgeLabel_->setText(tr("STEP 3 OF 5 • GHOST TEXT"));
            titleLabel_->setText(tr("Finish Sentences with Tab"));
            descriptionLabel_->setText(
                tr("Inline predicts the next couple of words right beside your cursor and wraps cleanly to any text box:"));
            nextButton_->setText(tr("Next: Privacy & Controls"));
            skipButton_->setText(areAllPermissionsGranted() ? tr("Close") : tr("Skip to Permissions"));
            break;
        case kPrivacySlide:
            stepBadgeLabel_->setText(tr("STEP 4 OF 5 • PRIVACY & CONTROLS"));
            titleLabel_->setText(tr("On-Device Privacy & Menu Bar Controls"));
            descriptionLabel_->setText(
                tr("Everything stays strictly on your Mac with instant access from the top-right menu bar:"));
            nextButton_->setText(tr("Next: System Permissions"));
            skipButton_->setText(areAllPermissionsGranted() ? tr("Close") : tr("Skip to Permissions"));
            break;
        case kPermissionSlide:
        default:
            stepBadgeLabel_->setText(tr("STEP 5 OF 5 • SYSTEM PERMISSIONS"));
            titleLabel_->setText(tr("Grant macOS System Permissions"));
            descriptionLabel_->setText(
                tr("Enable Input Monitoring and Accessibility in macOS System Settings. "
                   "If macOS asks to 'Quit & Reopen' after toggling a switch, click Quit & Reopen."));
            skipButton_->setText(tr("Dismiss"));
            checkPermissions();
            break;
    }
}

void PermissionDialog::onNextSlideClicked() {
    int idx = slideStack_->currentIndex();
    if (idx < kSlideCount - 1) {
        setCurrentSlide(idx + 1);
    } else {
        accept();
    }
}

void PermissionDialog::onPrevSlideClicked() {
    int idx = slideStack_->currentIndex();
    if (idx > 0) {
        setCurrentSlide(idx - 1);
    }
}

void PermissionDialog::applyTheme() {
    bool isDark = false;
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    isDark = isSystemInDarkMode();
#endif
    // designlang flat tokens: Inter typography, 14px radii, neutral ramps
    QString mainBg     = isDark ? "#121212" : "#FFFFFF";
    QString cardBg     = isDark ? "#171717" : "#FFFFFF";
    QString cardBorder = isDark ? "#2E2E2E" : "#E5E5E5";
    QString titleColor = isDark ? "#FFFFFF" : "#171717";
    QString subColor   = isDark ? "#9F9F9F" : "#525252";
    QString divColor   = isDark ? "#262626" : "#EAEAEA";
    QString actBtnBg   = isDark ? "#1F1F1F" : "#F5F5F5";
    QString actBtnBrd  = isDark ? "#2E2E2E" : "#E5E5E5";
    QString primBtnBg  = isDark ? "#FFFFFF" : "#171717";
    QString primBtnTxt = isDark ? "#171717" : "#FFFFFF";

    setStyleSheet(QString(
        "PermissionDialog {"
        "  background-color: %1;"
        "  color: %2;"
        "  font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'SF Pro Text', sans-serif;"
        "}"
        "#stepBadge {"
        "  font-size: 11px;"
        "  font-weight: 700;"
        "  letter-spacing: 0.05em;"
        "  color: %3;"
        "}"
        "#dialogTitle {"
        "  font-size: 22px;"
        "  font-weight: 700;"
        "  color: %2;"
        "}"
        "#dialogSubtitle {"
        "  font-size: 13px;"
        "  line-height: 1.45;"
        "  color: %3;"
        "}"
        "#permissionCard {"
        "  background-color: %4;"
        "  border: 1px solid %5;"
        "  border-radius: 14px;"
        "}"
        "#rowDivider {"
        "  color: %6;"
        "  background-color: %6;"
        "  height: 1px;"
        "  border: none;"
        "}"
        "#statusSummary {"
        "  font-size: 11px;"
        "  color: %3;"
        "  font-weight: 500;"
        "}"
        "#rowActionBtn {"
        "  background-color: %7;"
        "  color: %2;"
        "  border: 1px solid %8;"
        "  border-radius: 6px;"
        "  padding: 6px 12px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
        "#rowActionBtn:hover {"
        "  background-color: %5;"
        "}"
        "#rowActionBtn:disabled {"
        "  background-color: transparent;"
        "  color: %3;"
        "  border-color: %6;"
        "}"
        "#primaryBtn {"
        "  background-color: %9;"
        "  color: %10;"
        "  border: 1px solid %9;"
        "  border-radius: 8px;"
        "  padding: 8px 18px;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "}"
        "#primaryBtn:hover {"
        "  opacity: 0.9;"
        "}"
        "#primaryBtn:disabled {"
        "  background-color: #6B7280;"
        "  border-color: #6B7280;"
        "  color: #D1D5DB;"
        "}"
        "#secondaryBtn {"
        "  background-color: transparent;"
        "  color: %3;"
        "  border: 1px solid %8;"
        "  border-radius: 8px;"
        "  padding: 8px 14px;"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "}"
        "#secondaryBtn:hover {"
        "  background-color: %7;"
        "  color: %2;"
        "}"
    ).arg(
        mainBg, titleColor, subColor, cardBg, cardBorder,
        divColor, actBtnBg, actBtnBrd, primBtnBg, primBtnTxt
    ));
}

void PermissionDialog::checkPermissions() {
    bool im = checkInputMonitoringPermission();
    bool ax = checkAccessibilityPermission(false);

    bool isDark = false;
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    isDark = isSystemInDarkMode();
#endif

    QString grantedStyle = isDark
        ? "background-color: #064E3B; color: #34D399; font-size: 11px; font-weight: 600; padding: 4px 10px; border-radius: 10px; border: 1px solid #059669;"
        : "background-color: #ECFDF5; color: #059669; font-size: 11px; font-weight: 600; padding: 4px 10px; border-radius: 10px; border: 1px solid #A7F3D0;";

    QString reqStyle = isDark
        ? "background-color: #7F1D1D; color: #F87171; font-size: 11px; font-weight: 600; padding: 4px 10px; border-radius: 10px; border: 1px solid #DC2626;"
        : "background-color: #FEF2F2; color: #DC2626; font-size: 11px; font-weight: 600; padding: 4px 10px; border-radius: 10px; border: 1px solid #FECACA;";

    if (im) {
        imStatusLabel_->setText(tr("Granted"));
        imStatusLabel_->setStyleSheet(grantedStyle);
        imActionButton_->setText(tr("Enabled"));
        imActionButton_->setEnabled(false);
    } else {
        imStatusLabel_->setText(tr("Required"));
        imStatusLabel_->setStyleSheet(reqStyle);
        imActionButton_->setText(tr("Open Settings"));
        imActionButton_->setEnabled(true);
    }

    if (ax) {
        axStatusLabel_->setText(tr("Granted"));
        axStatusLabel_->setStyleSheet(grantedStyle);
        axActionButton_->setText(tr("Enabled"));
        axActionButton_->setEnabled(false);
    } else {
        axStatusLabel_->setText(tr("Required"));
        axStatusLabel_->setStyleSheet(reqStyle);
        axActionButton_->setText(tr("Open Settings"));
        axActionButton_->setEnabled(true);
    }

    if (im && ax) {
        statusSummaryLabel_->setText(tr("All system permissions are granted and ready."));
        statusSummaryLabel_->setStyleSheet("color: #059669; font-size: 11px; font-weight: 600;");
        if (slideStack_->currentIndex() == kPermissionSlide) {
            nextButton_->setText(tr("Open Inline Settings"));
            skipButton_->setText(tr("Close"));
        }
    } else {
        statusSummaryLabel_->setText(tr("Waiting for permissions in macOS System Settings..."));
        statusSummaryLabel_->setStyleSheet("color: #6B7280; font-size: 11px; font-weight: 500;");
        if (slideStack_->currentIndex() == kPermissionSlide) {
            nextButton_->setText(tr("Open Inline Settings"));
        }
    }
}

void PermissionDialog::onOpenInputMonitoringClicked() {
    openInputMonitoringSettings();
}

void PermissionDialog::onOpenAccessibilityClicked() {
    openAccessibilitySettings();
}
