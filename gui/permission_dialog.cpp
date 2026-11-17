#include "gui/permission_dialog.h"
#include "core/config/app_config.h"
#include "core/config/config_json.h"
#include "core/tier1/corrector.h"
#include "core/reliability/privacy_logger.h"
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
#include "platform/mac/permissions.h"
#endif

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <algorithm>
#include <cmath>

// ============================================================================
// 1. Step Progress Widget (3 Segmented Pill Dashes)
// ============================================================================
StepProgressWidget::StepProgressWidget(QWidget* parent) : QWidget(parent) {
    setFixedHeight(10);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void StepProgressWidget::setStep(int step) {
    currentStep_ = step;
    update();
}

void StepProgressWidget::setDarkMode(bool dark) {
    isDark_ = dark;
    update();
}

QSize StepProgressWidget::sizeHint() const {
    return QSize(120, 10);
}

void StepProgressWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int count = 3;
    const qreal segWidth = 32.0;
    const qreal segHeight = 4.0;
    const qreal spacing = 8.0;
    const qreal radius = 2.0;

    QColor activeColor(34, 197, 94); // #22C55E Inline signature green
    QColor inactiveColor = isDark_ ? QColor(50, 46, 42) : QColor(226, 221, 216);

    for (int i = 0; i < count; ++i) {
        QRectF r(i * (segWidth + spacing), (height() - segHeight) / 2.0, segWidth, segHeight);
        p.setBrush(i <= currentStep_ ? activeColor : inactiveColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(r, radius, radius);
    }
}

// ============================================================================
// 2. Laptop Mockup Widget (Slide 2: Teaching Words with Shortcut)
// ============================================================================
LaptopMockupWidget::LaptopMockupWidget(bool isDark, QWidget* parent)
    : QWidget(parent), isDark_(isDark) {
    setFixedHeight(165);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void LaptopMockupWidget::setDarkMode(bool dark) {
    isDark_ = dark;
    update();
}

void LaptopMockupWidget::setShortcutText(const QString& shortcut) {
    shortcutText_ = shortcut;
    update();
}

void LaptopMockupWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    qreal totalW = width();
    qreal laptopW = std::min(totalW - 60, 480.0);
    qreal laptopH = 155.0;
    qreal laptopX = (totalW - laptopW) / 2.0;
    qreal laptopY = 4.0;

    // 1. Laptop Outer Bezel
    QRectF lidRect(laptopX + 16, laptopY, laptopW - 32, laptopH);
    p.setBrush(isDark_ ? QColor(26, 24, 22) : QColor(230, 226, 222));
    p.setPen(QPen(isDark_ ? QColor(54, 50, 46) : QColor(200, 195, 190), 1.2));
    p.drawRoundedRect(lidRect, 12, 12);

    // 2. Screen Area
    QRectF screenRect = lidRect.adjusted(7, 7, -7, -7);
    p.setBrush(isDark_ ? QColor(18, 16, 15) : QColor(246, 244, 241));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(screenRect, 8, 8);

    // Top menu bar line in simulated screen
    QRectF miniMenuBar(screenRect.left(), screenRect.top(), screenRect.width(), 16);
    p.setBrush(isDark_ ? QColor(30, 28, 26) : QColor(236, 233, 229));
    p.drawRoundedRect(miniMenuBar, 8, 8);

    // Menu bar status icon: vertical bar and green pulse dot
    p.setPen(QPen(isDark_ ? QColor(255, 255, 255) : QColor(30, 30, 30), 1.2));
    p.drawLine(QPointF(screenRect.right() - 48, screenRect.top() + 4), QPointF(screenRect.right() - 48, screenRect.top() + 12));
    p.setBrush(QColor(34, 197, 94));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(screenRect.right() - 44, screenRect.top() + 6, 4, 4));

    // Time label 9:41
    QFont timeFont = p.font();
    timeFont.setPixelSize(9);
    timeFont.setWeight(QFont::Medium);
    p.setFont(timeFont);
    p.setPen(isDark_ ? QColor(160, 155, 150) : QColor(110, 105, 100));
    p.drawText(QRectF(screenRect.right() - 36, screenRect.top() + 1, 30, 14), Qt::AlignCenter, "9:41");

    // 3. Simulated Application Window (Notes app)
    QRectF winRect(screenRect.left() + 20, screenRect.top() + 24, screenRect.width() - 40, screenRect.height() - 32);
    p.setBrush(isDark_ ? QColor(34, 32, 30) : QColor(255, 255, 255));
    p.setPen(QPen(isDark_ ? QColor(56, 52, 48) : QColor(222, 217, 212), 1.0));
    p.drawRoundedRect(winRect, 8, 8);

    // Traffic lights in window
    p.setBrush(QColor(255, 95, 86)); p.drawEllipse(QRectF(winRect.left() + 8, winRect.top() + 7, 5, 5));
    p.setBrush(QColor(255, 189, 46)); p.drawEllipse(QRectF(winRect.left() + 16, winRect.top() + 7, 5, 5));
    p.setBrush(QColor(39, 201, 63)); p.drawEllipse(QRectF(winRect.left() + 24, winRect.top() + 7, 5, 5));

    // Window Title
    QFont titleFont = p.font();
    titleFont.setPixelSize(10);
    titleFont.setWeight(QFont::Medium);
    p.setFont(titleFont);
    p.setPen(isDark_ ? QColor(160, 155, 150) : QColor(120, 115, 110));
    p.drawText(QRectF(winRect.left(), winRect.top() + 4, winRect.width(), 12), Qt::AlignCenter, "Notes");

    // Window Content Text
    QFont bodyFont = p.font();
    bodyFont.setPixelSize(12);
    bodyFont.setWeight(QFont::Normal);
    p.setFont(bodyFont);

    // Single focused line: "Meeting with Sarah tomorrow at 10 AM" with "Sarah" highlighted
    qreal lineY = winRect.top() + 54;
    QString prefixText = "Meeting with ";
    QFontMetrics fm(bodyFont);
    int prefixW = fm.horizontalAdvance(prefixText);

    p.setPen(isDark_ ? QColor(240, 235, 230) : QColor(28, 25, 23));
    p.drawText(QPointF(winRect.left() + 24, lineY + 12), prefixText);

    // Selected Word "Sarah"
    QString targetWord = "Sarah";
    int wordW = fm.horizontalAdvance(targetWord);
    qreal wordX = winRect.left() + 24 + prefixW;
    QRectF highlightRect(wordX - 3, lineY, wordW + 6, 18);

    // Selection highlight background (subtle brand green glow)
    p.setBrush(isDark_ ? QColor(34, 197, 94, 65) : QColor(34, 197, 94, 40));
    p.setPen(QPen(QColor(34, 197, 94), 1.0));
    p.drawRoundedRect(highlightRect, 4, 4);

    // Highlighted text in bold
    QFont boldFont = bodyFont;
    boldFont.setWeight(QFont::Bold);
    p.setFont(boldFont);
    p.setPen(isDark_ ? QColor(255, 255, 255) : QColor(20, 83, 45));
    p.drawText(QPointF(wordX, lineY + 12), targetWord);

    // Suffix text after "Sarah": " tomorrow at 10 AM"
    QString suffixText = " tomorrow at 10 AM";
    p.setFont(bodyFont);
    p.setPen(isDark_ ? QColor(240, 235, 230) : QColor(28, 25, 23));
    p.drawText(QPointF(wordX + wordW + 3, lineY + 12), suffixText);

    // Callout tooltip badge above "Sarah"
    qreal badgeW = 135.0;
    qreal badgeH = 20.0;
    qreal badgeX = wordX + (wordW / 2.0) - (badgeW / 2.0);
    qreal badgeY = lineY - badgeH - 6.0;

    QRectF badgeRect(badgeX, badgeY, badgeW, badgeH);
    p.setBrush(QColor(34, 197, 94)); // vibrant green badge #22C55E
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(badgeRect, 5, 5);

    // Tiny pointer triangle towards highlight
    QPainterPath triangle;
    triangle.moveTo(wordX + (wordW / 2.0) - 4, badgeY + badgeH);
    triangle.lineTo(wordX + (wordW / 2.0) + 4, badgeY + badgeH);
    triangle.lineTo(wordX + (wordW / 2.0), badgeY + badgeH + 4);
    triangle.closeSubpath();
    p.drawPath(triangle);

    // Badge text: "[Shortcut] to remember"
    QFont badgeFont = p.font();
    badgeFont.setPixelSize(10);
    badgeFont.setWeight(QFont::Bold);
    p.setFont(badgeFont);
    p.setPen(Qt::white);
    p.drawText(badgeRect, Qt::AlignCenter, QString("%1 to remember").arg(shortcutText_));

    // 4. Laptop Base & Hinge
    QRectF baseRect(laptopX, laptopY + laptopH - 6, laptopW, 7);
    p.setBrush(isDark_ ? QColor(36, 33, 30) : QColor(212, 207, 202));
    p.setPen(QPen(isDark_ ? QColor(56, 52, 48) : QColor(190, 185, 180), 1.0));
    p.drawRoundedRect(baseRect, 3, 3);

    // Trackpad notch cut
    QRectF notchRect(totalW / 2.0 - 20, laptopY + laptopH - 6, 40, 3);
    p.setBrush(isDark_ ? QColor(22, 20, 18) : QColor(180, 175, 170));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(notchRect, 1.5, 1.5);
}

// ============================================================================
// Vector Theme Mode Icon (Sun in light mode, Moon in dark mode)
// ============================================================================
static QIcon renderThemeIcon(bool isDark) {
    QPixmap px(28, 28);
    px.fill(Qt::transparent);
    px.setDevicePixelRatio(2.0);

    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);

    QColor stroke = isDark ? QColor(255, 255, 255) : QColor(28, 25, 23);
    p.setPen(QPen(stroke, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    if (isDark) {
        // Crescent moon
        QPainterPath moon;
        moon.moveTo(9.5, 3.0);
        moon.cubicTo(6.0, 4.0, 4.0, 7.5, 4.0, 10.5);
        moon.cubicTo(4.0, 14.0, 7.0, 17.0, 10.5, 17.0);
        moon.cubicTo(12.5, 17.0, 14.5, 16.0, 15.5, 14.5);
        moon.cubicTo(11.5, 14.5, 9.0, 11.5, 9.5, 3.0);
        moon.closeSubpath();
        p.drawPath(moon);
    } else {
        // Sun: center circle + 8 rays
        p.drawEllipse(QPointF(9.0, 9.0), 3.5, 3.5);
        for (int i = 0; i < 8; ++i) {
            qreal angle = i * (3.14159265 / 4.0);
            qreal x1 = 9.0 + 5.0 * std::cos(angle);
            qreal y1 = 9.0 + 5.0 * std::sin(angle);
            qreal x2 = 9.0 + 6.8 * std::cos(angle);
            qreal y2 = 9.0 + 6.8 * std::sin(angle);
            p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
        }
    }
    return QIcon(px);
}

// ============================================================================
// PermissionDialog Implementation
// ============================================================================

bool PermissionDialog::areAllPermissionsGranted() {
    bool im = checkInputMonitoringPermission();
    bool ax = checkAccessibilityPermission(false);
    return im && ax;
}

QString PermissionDialog::currentShortcutString() const {
    int hotkeyIdx = 0;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        hotkeyIdx = sharedConfig.addWordHotkeyIndex;
    }
    switch (hotkeyIdx) {
        case 1: return "⌘⌥⇧I";
        case 2: return "⌘⇧D";
        case 3: return "⌥⇧D";
        case 4: return "⌃⌥D";
        case 5: return "⌘⌥D";
        case 0:
        default: return "⌘⌥⇧D";
    }
}

PermissionDialog::PermissionDialog(QWidget* parent, int initialSlide) : QDialog(parent) {
    setWindowTitle(tr("Inline"));
    setFixedSize(700, 520);
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    isDarkMode_ = true;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        if (sharedConfig.appTheme == 1) {
            isDarkMode_ = false; // Forced Light
        } else if (sharedConfig.appTheme == 2) {
            isDarkMode_ = true;  // Forced Dark
        } else {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
            isDarkMode_ = isSystemInDarkMode();
#endif
        }
    }

    setupUi();
    applyTheme();
    setCurrentSlide(std::clamp(initialSlide, 0, kSlideCount - 1));
    checkPermissions();

    pollTimer_ = new QTimer(this);
    connect(pollTimer_, &QTimer::timeout, this, &PermissionDialog::checkPermissions);
    pollTimer_->start(500);

#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    bringAppToFront();
#endif
}

void PermissionDialog::setupUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* mainContainer = new QFrame(this);
    mainContainer->setObjectName("dialogWindow");
    outerLayout->addWidget(mainContainer);

    auto* mainLayout = new QVBoxLayout(mainContainer);
    mainLayout->setContentsMargins(28, 18, 28, 18);
    mainLayout->setSpacing(10);

    // 1. Top Window Bar: Theme Toggle on Left, Centered Progress Dashes, Privacy Text on Right
    auto* topBar = new QHBoxLayout();
    topBar->setContentsMargins(0, 0, 0, 2);

    themeToggleBtn_ = new QPushButton(this);
    themeToggleBtn_->setObjectName("themeToggleBtn");
    connect(themeToggleBtn_, &QPushButton::clicked, this, &PermissionDialog::toggleTheme);
    topBar->addWidget(themeToggleBtn_);

    topBar->addStretch(1);

    progressWidget_ = new StepProgressWidget(this);
    progressWidget_->setDarkMode(isDarkMode_);
    topBar->addWidget(progressWidget_);

    topBar->addStretch(1);

    privacyLabel_ = new QLabel(tr("100% On-Device"), this);
    privacyLabel_->setObjectName("headerPrivacyText");
    topBar->addWidget(privacyLabel_);

    mainLayout->addLayout(topBar);

    // 2. Header Title & Subtitle (Clean single-line labels with ample height)
    auto* headerBox = new QVBoxLayout();
    headerBox->setContentsMargins(0, 4, 0, 6);
    headerBox->setSpacing(4);
    headerBox->setAlignment(Qt::AlignCenter);

    titleLabel_ = new QLabel(this);
    titleLabel_->setObjectName("dialogTitle");
    titleLabel_->setAlignment(Qt::AlignCenter);
    titleLabel_->setFixedHeight(30);
    headerBox->addWidget(titleLabel_);

    descriptionLabel_ = new QLabel(this);
    descriptionLabel_->setObjectName("dialogSubtitle");
    descriptionLabel_->setAlignment(Qt::AlignCenter);
    descriptionLabel_->setFixedHeight(20);
    headerBox->addWidget(descriptionLabel_);

    mainLayout->addLayout(headerBox);

    // 3. Stacked Slides (3 Steps: How It Works, Teaching Words, Permissions)
    slideStack_ = new QStackedWidget(this);
    slideStack_->addWidget(createAutocorrectSlide());  // 0: Fix typos as you type
    slideStack_->addWidget(createShortcutsSlide());    // 1: Teach Inline your words
    slideStack_->addWidget(createPermissionsSlide());  // 2: System Permissions
    mainLayout->addWidget(slideStack_, 1);

    // 4. Footer Action Bar
    auto* footerLayout = new QHBoxLayout();
    footerLayout->setContentsMargins(0, 6, 0, 0);

    backButton_ = new QPushButton(tr("Back"), this);
    backButton_->setObjectName("secondaryTextBtn");
    connect(backButton_, &QPushButton::clicked, this, &PermissionDialog::onPrevSlideClicked);
    footerLayout->addWidget(backButton_);

    footerLayout->addStretch();

    nextButton_ = new QPushButton(tr("Continue"), this);
    nextButton_->setObjectName("primaryAccentBtn");
    connect(nextButton_, &QPushButton::clicked, this, &PermissionDialog::onNextSlideClicked);
    footerLayout->addWidget(nextButton_);

    mainLayout->addLayout(footerLayout);
}

// ============================================================================
// Slide 1: How Autocorrect Works & Interactive Demo (Step 1)
// ============================================================================
QWidget* PermissionDialog::createAutocorrectSlide() {
    auto* slide = new QWidget(this);
    auto* layout = new QVBoxLayout(slide);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(12);

    auto* card = new QFrame(slide);
    card->setObjectName("mainCard");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(18, 16, 18, 16);
    cardLayout->setSpacing(14);

    // Row 1: Visual Typo Transformations (3 clear everyday examples)
    auto* examplesLayout = new QHBoxLayout();
    examplesLayout->setSpacing(10);

    auto makeExampleCard = [card](const QString& typo, const QString& fix) {
        auto* box = new QFrame(card);
        box->setObjectName("typoExampleBox");
        auto* l = new QHBoxLayout(box);
        l->setContentsMargins(12, 8, 12, 8);
        l->setSpacing(8);

        auto* typoLbl = new QLabel(typo, box);
        typoLbl->setObjectName("typoBeforeLabel");
        l->addWidget(typoLbl);

        auto* arrow = new QLabel("→", box);
        arrow->setObjectName("typoArrowLabel");
        l->addWidget(arrow);

        auto* fixLbl = new QLabel(fix, box);
        fixLbl->setObjectName("typoAfterLabel");
        l->addWidget(fixLbl);

        return box;
    };

    examplesLayout->addWidget(makeExampleCard("teh", "the"));
    examplesLayout->addWidget(makeExampleCard("dont", "don't"));
    examplesLayout->addWidget(makeExampleCard("becuase", "because"));
    cardLayout->addLayout(examplesLayout);

    // Row 2: Single-Key Undo Bar
    auto* undoBar = new QFrame(card);
    undoBar->setObjectName("undoBannerBox");
    auto* undoLayout = new QHBoxLayout(undoBar);
    undoLayout->setContentsMargins(12, 8, 12, 8);
    undoLayout->setSpacing(10);

    auto* backspaceKey = new QLabel("Backspace", undoBar);
    backspaceKey->setObjectName("keycapBadge");
    undoLayout->addWidget(backspaceKey);

    auto* undoText = new QLabel(tr("Single-Key Undo: Press Backspace immediately to revert any correction."), undoBar);
    undoText->setObjectName("undoTextLabel");
    undoLayout->addWidget(undoText, 1);
    cardLayout->addWidget(undoBar);

    // Row 3: Interactive Live Typing Sandbox
    auto* tryItContainer = new QFrame(card);
    tryItContainer->setObjectName("tryItBox");
    auto* tryLayout = new QVBoxLayout(tryItContainer);
    tryLayout->setContentsMargins(14, 12, 14, 12);
    tryLayout->setSpacing(8);

    auto* tryTitle = new QLabel(tr("Try typing a typo below (type 'teh ' or 'dont ' and hit space):"), tryItContainer);
    tryTitle->setObjectName("tryTitleLabel");
    tryLayout->addWidget(tryTitle);

    demoInput_ = new QLineEdit(tryItContainer);
    demoInput_->setObjectName("demoInputField");
    demoInput_->setPlaceholderText(tr("Type here to test autocorrect..."));
    demoInput_->installEventFilter(this);
    tryLayout->addWidget(demoInput_);

    demoFeedbackLabel_ = new QLabel(tr("Press space after a typo to see it autocorrect instantly."), tryItContainer);
    demoFeedbackLabel_->setObjectName("demoFeedbackLabel");
    tryLayout->addWidget(demoFeedbackLabel_);

    // Connect real-time autocorrect for the demo field
    connect(demoInput_, &QLineEdit::textEdited, this, [this](const QString& text) {
        struct DemoRule {
            QString typo;
            QString fix;
        };
        static const DemoRule rules[] = {
            {"teh ", "the "},
            {"dont ", "don't "},
            {"becuase ", "because "},
            {"whenI ", "when I "},
            {"woudl ", "would "},
            {"recieve ", "receive "}
        };

        for (const auto& r : rules) {
            if (text.endsWith(r.typo)) {
                QString corrected = text.left(text.length() - r.typo.length()) + r.fix;
                lastDemoOriginal_ = text;
                lastDemoCorrected_ = corrected;

                demoInput_->setText(corrected);
                demoFeedbackLabel_->setText(tr("Corrected \"%1\" → \"%2\" (Press Backspace to undo)")
                                            .arg(r.typo.trimmed(), r.fix.trimmed()));
                demoFeedbackLabel_->setStyleSheet("color: #16A34A; font-weight: 600; font-size: 11px;");
                return;
            }
        }
    });

    cardLayout->addWidget(tryItContainer);

    layout->addWidget(card);
    layout->addStretch();
    return slide;
}

// ============================================================================
// Slide 2: Teaching Words & Shortcuts (Step 2)
// ============================================================================
QWidget* PermissionDialog::createShortcutsSlide() {
    auto* slide = new QWidget(this);
    auto* layout = new QVBoxLayout(slide);
    layout->setContentsMargins(0, 2, 0, 0);
    layout->setSpacing(10);

    laptopMockup_ = new LaptopMockupWidget(isDarkMode_, slide);
    laptopMockup_->setShortcutText(currentShortcutString());
    layout->addWidget(laptopMockup_);

    auto* banner = new QFrame(slide);
    banner->setObjectName("shortcutBanner");
    auto* bLayout = new QVBoxLayout(banner);
    bLayout->setContentsMargins(16, 12, 16, 12);
    bLayout->setSpacing(10);

    // Row 1: Shortcut Keycaps & Explanation
    auto* r1 = new QHBoxLayout();
    r1->setSpacing(8);

    auto* lbl = new QLabel(tr("Shortcut:"), banner);
    lbl->setObjectName("shortcutBannerText");
    r1->addWidget(lbl);

    auto makeKeycap = [banner](const QString& key) {
        auto* k = new QLabel(key, banner);
        k->setObjectName("keycapBadge");
        k->setAlignment(Qt::AlignCenter);
        return k;
    };

    int hotkeyIdx = 0;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        hotkeyIdx = sharedConfig.addWordHotkeyIndex;
    }

    std::vector<QString> keys;
    switch (hotkeyIdx) {
        case 1: keys = {"⌘", "⌥", "⇧", "I"}; break;
        case 2: keys = {"⌘", "⇧", "D"}; break;
        case 3: keys = {"⌥", "⇧", "D"}; break;
        case 4: keys = {"⌃", "⌥", "D"}; break;
        case 5: keys = {"⌘", "⌥", "D"}; break;
        case 0:
        default: keys = {"⌘", "⌥", "⇧", "D"}; break;
    }

    for (const auto& k : keys) {
        r1->addWidget(makeKeycap(k));
    }

    auto* expLbl = new QLabel(tr("Highlight any word in any app to save it to your dictionary."), banner);
    expLbl->setObjectName("shortcutHintText");
    r1->addWidget(expLbl, 1);

    bLayout->addLayout(r1);

    // Divider
    auto* div = new QFrame(banner);
    div->setObjectName("permDivider");
    div->setFrameShape(QFrame::HLine);
    bLayout->addWidget(div);

    // Row 2: Add Custom Word Input (Generous width, no clipping)
    auto* r2 = new QHBoxLayout();
    r2->setSpacing(8);

    addWordInput_ = new QLineEdit(banner);
    addWordInput_->setObjectName("addWordInputField");
    addWordInput_->setPlaceholderText(tr("Add custom name or word (e.g. Sarah, Maya)..."));
    r2->addWidget(addWordInput_, 1);

    auto* addBtn = new QPushButton(tr("Add Word"), banner);
    addBtn->setObjectName("addWordBtn");
    addBtn->setFixedHeight(32);
    r2->addWidget(addBtn);

    bLayout->addLayout(r2);

    addWordStatusLabel_ = new QLabel(tr("Names and custom words saved here will never be autocorrected."), banner);
    addWordStatusLabel_->setObjectName("addWordStatusLabel");
    bLayout->addWidget(addWordStatusLabel_);

    auto handleAddWord = [this]() {
        if (!addWordInput_) return;
        QString word = addWordInput_->text().trimmed();
        if (!word.isEmpty()) {
            {
                std::lock_guard<std::mutex> lock(configMutex);
                if (std::find(sharedConfig.ignoreWords.begin(), sharedConfig.ignoreWords.end(), word.toStdString()) == sharedConfig.ignoreWords.end()) {
                    sharedConfig.ignoreWords.push_back(word.toStdString());
                    saveConfigToFile(sharedConfig);
                }
            }
            addWordInput_->clear();
            addWordStatusLabel_->setText(tr("Added \"%1\" to known dictionary.").arg(word));
            addWordStatusLabel_->setStyleSheet("color: #16A34A; font-weight: 600; font-size: 11px;");
        }
    };

    connect(addBtn, &QPushButton::clicked, this, handleAddWord);
    connect(addWordInput_, &QLineEdit::returnPressed, this, handleAddWord);

    layout->addWidget(banner);
    layout->addStretch();
    return slide;
}

// ============================================================================
// Slide 3: System Permissions (Step 3)
// ============================================================================
QWidget* PermissionDialog::createPermissionsSlide() {
    auto* slide = new QWidget(this);
    auto* layout = new QVBoxLayout(slide);
    layout->setContentsMargins(0, 6, 0, 0);
    layout->setSpacing(12);

    auto* permCard = new QFrame(slide);
    permCard->setObjectName("permCard");
    auto* pLayout = new QVBoxLayout(permCard);
    pLayout->setContentsMargins(18, 16, 18, 16);
    pLayout->setSpacing(12);

    // Row 1: Input Monitoring
    auto* r1 = new QHBoxLayout();
    r1->setSpacing(12);

    auto* imText = new QVBoxLayout();
    imText->setSpacing(2);
    auto* imTitle = new QLabel(tr("Input Monitoring"), permCard);
    imTitle->setObjectName("permRowTitle");
    auto* imSub = new QLabel(tr("Required to detect typos when you press space in any app."), permCard);
    imSub->setObjectName("permRowSubtitle");
    imText->addWidget(imTitle);
    imText->addWidget(imSub);
    r1->addLayout(imText, 1);

    imStatusLabel_ = new QLabel(tr("Checking..."), permCard);
    imStatusLabel_->setObjectName("permStatusLabel");
    r1->addWidget(imStatusLabel_);

    imActionButton_ = new QPushButton(tr("Grant Access"), permCard);
    imActionButton_->setObjectName("permActionBtn");
    connect(imActionButton_, &QPushButton::clicked, this, &PermissionDialog::onOpenInputMonitoringClicked);
    r1->addWidget(imActionButton_);
    pLayout->addLayout(r1);

    auto* div = new QFrame(permCard);
    div->setObjectName("permDivider");
    div->setFrameShape(QFrame::HLine);
    pLayout->addWidget(div);

    // Row 2: Accessibility
    auto* r2 = new QHBoxLayout();
    r2->setSpacing(12);

    auto* axText = new QVBoxLayout();
    axText->setSpacing(2);
    auto* axTitle = new QLabel(tr("Accessibility"), permCard);
    axTitle->setObjectName("permRowTitle");
    auto* axSub = new QLabel(tr("Required to insert corrected words and capture selected text."), permCard);
    axSub->setObjectName("permRowSubtitle");
    axText->addWidget(axTitle);
    axText->addWidget(axSub);
    r2->addLayout(axText, 1);

    axStatusLabel_ = new QLabel(tr("Checking..."), permCard);
    axStatusLabel_->setObjectName("permStatusLabel");
    r2->addWidget(axStatusLabel_);

    axActionButton_ = new QPushButton(tr("Grant Access"), permCard);
    axActionButton_->setObjectName("permActionBtn");
    connect(axActionButton_, &QPushButton::clicked, this, &PermissionDialog::onOpenAccessibilityClicked);
    r2->addWidget(axActionButton_);
    pLayout->addLayout(r2);

    layout->addWidget(permCard);

    // Ghost Type (Beta) Information Card
    auto* ghostCard = new QFrame(slide);
    ghostCard->setObjectName("ghostInfoCard");
    auto* ghLayout = new QVBoxLayout(ghostCard);
    ghLayout->setContentsMargins(16, 10, 16, 10);
    ghLayout->setSpacing(4);

    auto* ghHeaderLayout = new QHBoxLayout();
    ghHeaderLayout->setSpacing(8);

    auto* ghTitle = new QLabel(tr("Ghost Type Predictions (Beta)"), ghostCard);
    ghTitle->setObjectName("ghostCardTitle");
    ghHeaderLayout->addWidget(ghTitle);

    auto* betaBadge = new QLabel(tr("OFF BY DEFAULT"), ghostCard);
    betaBadge->setObjectName("betaBadge");
    ghHeaderLayout->addWidget(betaBadge);
    ghHeaderLayout->addStretch();
    ghLayout->addLayout(ghHeaderLayout);

    auto* ghSub = new QLabel(tr("Experimental next-word completions beside your cursor. It is currently in Beta and kept OFF by default. You can test and enable it anytime in Settings > Ghost Text."), ghostCard);
    ghSub->setObjectName("ghostCardSubtitle");
    ghSub->setWordWrap(true);
    ghLayout->addWidget(ghSub);

    layout->addWidget(ghostCard);

    // Privacy & Ready Information Card
    auto* privCard = new QFrame(slide);
    privCard->setObjectName("privacyInfoCard");
    auto* prLayout = new QVBoxLayout(privCard);
    prLayout->setContentsMargins(16, 10, 16, 10);
    prLayout->setSpacing(4);

    auto* privTitle = new QLabel(tr("100% On-Device & Private"), privCard);
    privTitle->setObjectName("privCardTitle");
    prLayout->addWidget(privTitle);

    auto* privSub = new QLabel(tr("Inline runs entirely on your Mac. No network requests, zero telemetry, and your typing is never stored or sent anywhere."), privCard);
    privSub->setObjectName("privCardSubtitle");
    privSub->setWordWrap(true);
    prLayout->addWidget(privSub);

    layout->addWidget(privCard);

    permNoteLabel_ = new QLabel(this);
    permNoteLabel_->setObjectName("permNoteLabel");
    permNoteLabel_->setAlignment(Qt::AlignCenter);
    layout->addWidget(permNoteLabel_);

    layout->addStretch();
    return slide;
}

// ============================================================================
// Event Filter for Interactive Backspace Single-Key Undo in Demo Field
// ============================================================================
bool PermissionDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == demoInput_ && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Backspace) {
            if (!lastDemoOriginal_.isEmpty() && demoInput_->text() == lastDemoCorrected_) {
                demoInput_->setText(lastDemoOriginal_);
                demoFeedbackLabel_->setText(tr("Single-Key Undo! Restored original text."));
                demoFeedbackLabel_->setStyleSheet(isDarkMode_ ? "color: #A8A29E; font-size: 11px;" : "color: #78716C; font-size: 11px;");
                lastDemoOriginal_.clear();
                lastDemoCorrected_.clear();
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

// ============================================================================
// Slide Navigation & State Management
// ============================================================================

void PermissionDialog::setCurrentSlide(int index) {
    if (!slideStack_) return;
    int clamped = std::clamp(index, 0, kSlideCount - 1);
    slideStack_->setCurrentIndex(clamped);

    if (progressWidget_) {
        progressWidget_->setStep(clamped);
    }
    updateSlideHeader();
}

void PermissionDialog::updateSlideHeader() {
    int cur = slideStack_ ? slideStack_->currentIndex() : 0;
    if (backButton_) {
        backButton_->setVisible(cur > 0);
    }

    switch (cur) {
        case kAutocorrectSlide:
            titleLabel_->setText(tr("Fix typos as you type"));
            descriptionLabel_->setText(tr("Inline silently corrects mistakes the instant you press space."));
            nextButton_->setText(tr("Continue"));
            break;
        case kShortcutsSlide:
            titleLabel_->setText(tr("Teach Inline your words"));
            descriptionLabel_->setText(tr("Highlight any word in any app and press your shortcut to save it."));
            nextButton_->setText(tr("Continue"));
            break;
        case kPermissionsSlide:
            titleLabel_->setText(tr("System permissions"));
            descriptionLabel_->setText(tr("Grant required macOS permissions to enable autocorrection."));
            nextButton_->setText(areAllPermissionsGranted() ? tr("Finish Setup") : tr("Done"));
            break;
    }
}

void PermissionDialog::onNextSlideClicked() {
    int cur = slideStack_->currentIndex();
    if (cur < kSlideCount - 1) {
        setCurrentSlide(cur + 1);
    } else {
        accept();
    }
}

void PermissionDialog::onPrevSlideClicked() {
    int cur = slideStack_->currentIndex();
    if (cur > 0) {
        setCurrentSlide(cur - 1);
    }
}

void PermissionDialog::toggleTheme() {
    isDarkMode_ = !isDarkMode_;
    applyTheme();

    if (progressWidget_) progressWidget_->setDarkMode(isDarkMode_);
    if (laptopMockup_) laptopMockup_->setDarkMode(isDarkMode_);
    for (auto* child : findChildren<QWidget*>()) {
        child->update();
    }
    update();
}

// ============================================================================
// Stylesheet & Theme Switching (Both Light and Dark Mode)
// ============================================================================

void PermissionDialog::applyTheme() {
    QString mainBg       = isDarkMode_ ? "#1B1917" : "#F8F6F4"; // warm dark / warm light paper
    QString cardBg       = isDarkMode_ ? "#242220" : "#FFFFFF";
    QString cardBorder   = isDarkMode_ ? "#383430" : "#E5E0DB";
    QString titleColor   = isDarkMode_ ? "#FFFFFF" : "#1C1917";
    QString subtitleColor= isDarkMode_ ? "#A8A29E" : "#78716C";
    QString keycapBg     = isDarkMode_ ? "#36322E" : "#EFEBE6";
    QString keycapBorder = isDarkMode_ ? "#4A4540" : "#D8D2CB";
    QString boxBg        = isDarkMode_ ? "#2C2926" : "#F4F1ED";
    QString accentColor  = "#22C55E"; // #22C55E Inline signature green

    themeToggleBtn_->setIcon(renderThemeIcon(isDarkMode_));
    themeToggleBtn_->setIconSize(QSize(13, 13));
    themeToggleBtn_->setText(isDarkMode_ ? tr("Dark") : tr("Light"));

    setStyleSheet(QString(
        "QDialog {"
        "  background: transparent;"
        "}"
        "#dialogWindow {"
        "  background-color: %1;"
        "  border: 1px solid %3;"
        "  border-radius: 16px;"
        "  color: %2;"
        "  font-family: 'Inter', -apple-system, sans-serif;"
        "}"
        "#dialogTitle {"
        "  font-size: 24px;"
        "  font-weight: 700;"
        "  color: %2;"
        "}"
        "#dialogSubtitle {"
        "  font-size: 13px;"
        "  color: %4;"
        "  font-weight: 400;"
        "}"
        "#headerPrivacyText {"
        "  background: transparent;"
        "  border: none;"
        "  font-size: 11px;"
        "  font-weight: 500;"
        "  color: %4;"
        "  padding: 4px 2px;"
        "}"
        "#themeToggleBtn {"
        "  background-color: %5;"
        "  border: 1px solid %3;"
        "  border-radius: 12px;"
        "  padding: 4px 10px;"
        "  font-size: 11px;"
        "  font-weight: 500;"
        "  color: %4;"
        "}"
        "#themeToggleBtn:hover {"
        "  color: %2;"
        "}"
        "#mainCard, #permCard, #shortcutBanner {"
        "  background-color: %5;"
        "  border: 1px solid %3;"
        "  border-radius: 14px;"
        "}"
        "#typoExampleBox {"
        "  background-color: %6;"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "}"
        "#typoBeforeLabel {"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "  color: %4;"
        "}"
        "#typoArrowLabel {"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  color: %4;"
        "}"
        "#typoAfterLabel {"
        "  font-size: 13px;"
        "  font-weight: 700;"
        "  color: #16A34A;"
        "}"
        "#undoBannerBox {"
        "  background-color: %6;"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "}"
        "#undoTextLabel {"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "  color: %4;"
        "}"
        "#tryItBox {"
        "  background-color: %6;"
        "  border: 1px solid %3;"
        "  border-radius: 10px;"
        "}"
        "#tryTitleLabel {"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  color: %2;"
        "}"
        "#demoInputField, #addWordInputField {"
        "  background-color: %5;"
        "  border: 1px solid %3;"
        "  border-radius: 7px;"
        "  padding: 6px 10px;"
        "  font-size: 13px;"
        "  color: %2;"
        "}"
        "#demoInputField:focus, #addWordInputField:focus {"
        "  border: 1.5px solid %7;"
        "}"
        "#demoFeedbackLabel, #addWordStatusLabel {"
        "  font-size: 11px;"
        "  color: %4;"
        "}"
        "#shortcutBannerText {"
        "  font-size: 13px;"
        "  color: %2;"
        "  font-weight: 600;"
        "}"
        "#shortcutHintText {"
        "  font-size: 12px;"
        "  color: %4;"
        "  font-weight: 400;"
        "}"
        "#keycapBadge {"
        "  background-color: %8;"
        "  border: 1px solid %9;"
        "  border-radius: 6px;"
        "  color: %2;"
        "  font-size: 12px;"
        "  font-weight: 700;"
        "  padding: 4px 8px;"
        "  min-width: 22px;"
        "}"
        "#addWordBtn {"
        "  background-color: %8;"
        "  border: 1px solid %9;"
        "  border-radius: 6px;"
        "  color: %2;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  padding: 4px 14px;"
        "}"
        "#addWordBtn:hover {"
        "  border-color: %7;"
        "}"
        "#permRowTitle {"
        "  font-size: 13px;"
        "  color: %2;"
        "  font-weight: 600;"
        "}"
        "#permRowSubtitle {"
        "  font-size: 11px;"
        "  color: %4;"
        "  font-weight: 400;"
        "}"
        "#permDivider {"
        "  background-color: %3;"
        "  height: 1px;"
        "  border: none;"
        "}"
        "#permStatusLabel {"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
        "#permActionBtn {"
        "  background-color: %8;"
        "  border: 1px solid %9;"
        "  border-radius: 6px;"
        "  padding: 5px 12px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  color: %2;"
        "}"
        "#permActionBtn:hover {"
        "  border-color: %7;"
        "}"
        "#ghostInfoCard, #privacyInfoCard {"
        "  background-color: %6;"
        "  border: 1px solid %3;"
        "  border-radius: 10px;"
        "}"
        "#ghostCardTitle, #privCardTitle {"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  color: %2;"
        "}"
        "#betaBadge {"
        "  background-color: %8;"
        "  border: 1px solid %9;"
        "  border-radius: 4px;"
        "  color: %4;"
        "  font-size: 10px;"
        "  font-weight: 600;"
        "  padding: 2px 6px;"
        "}"
        "#ghostCardSubtitle, #privCardSubtitle {"
        "  font-size: 11px;"
        "  color: %4;"
        "  line-height: 1.4;"
        "}"
        "#permNoteLabel {"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "  color: %4;"
        "  padding: 2px 0;"
        "}"
        "#secondaryTextBtn {"
        "  background: transparent;"
        "  border: none;"
        "  color: %4;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "  padding: 6px 12px;"
        "}"
        "#secondaryTextBtn:hover {"
        "  color: %2;"
        "}"
        "#primaryAccentBtn {"
        "  background-color: %7;"
        "  color: #FFFFFF;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 8px 24px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "}"
        "#primaryAccentBtn:hover {"
        "  background-color: #16A34A;"
        "}"
        "#primaryAccentBtn:pressed {"
        "  background-color: #15803D;"
        "}"
    ).arg(
        mainBg, titleColor, cardBorder, subtitleColor, cardBg,
        boxBg, accentColor, keycapBg, keycapBorder
    ));
}

// ============================================================================
// Real-time Permission Verification
// ============================================================================

void PermissionDialog::checkPermissions() {
    bool im = checkInputMonitoringPermission();
    bool ax = checkAccessibilityPermission(false);

    if (imStatusLabel_ && imActionButton_) {
        if (im) {
            imStatusLabel_->setText(tr("Granted"));
            imStatusLabel_->setStyleSheet("color: #16A34A; background: #DCFCE7; font-weight: 600; padding: 4px 8px; border-radius: 6px; font-size: 11px;");
            imActionButton_->setVisible(false);
        } else {
            imStatusLabel_->setText(tr("Required"));
            imStatusLabel_->setStyleSheet("color: #6B7280; background: #F3F4F6; font-weight: 500; padding: 4px 8px; border-radius: 6px; font-size: 11px;");
            imActionButton_->setVisible(true);
        }
    }

    if (axStatusLabel_ && axActionButton_) {
        if (ax) {
            axStatusLabel_->setText(tr("Granted"));
            axStatusLabel_->setStyleSheet("color: #16A34A; background: #DCFCE7; font-weight: 600; padding: 4px 8px; border-radius: 6px; font-size: 11px;");
            axActionButton_->setVisible(false);
        } else {
            axStatusLabel_->setText(tr("Required"));
            axStatusLabel_->setStyleSheet("color: #6B7280; background: #F3F4F6; font-weight: 500; padding: 4px 8px; border-radius: 6px; font-size: 11px;");
            axActionButton_->setVisible(true);
        }
    }

    if (permNoteLabel_) {
        if (im && ax) {
            permNoteLabel_->setText(tr("All permissions granted. Inline is active and ready to use."));
            permNoteLabel_->setStyleSheet("color: #16A34A; font-weight: 600; font-size: 12px;");
        } else {
            permNoteLabel_->setText(tr("Grant both permissions above to enable real-time autocorrection."));
            permNoteLabel_->setStyleSheet(isDarkMode_ ? "color: #A8A29E; font-size: 12px;" : "color: #78716C; font-size: 12px;");
        }
    }

    if (slideStack_ && slideStack_->currentIndex() == kPermissionsSlide) {
        if (nextButton_) {
            nextButton_->setText((im && ax) ? tr("Finish Setup") : tr("Done"));
        }
    }
}

void PermissionDialog::onOpenInputMonitoringClicked() {
    openInputMonitoringSettings();
}

void PermissionDialog::onOpenAccessibilityClicked() {
    openAccessibilitySettings();
}
