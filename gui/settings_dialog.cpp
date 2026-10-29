#include "gui/settings_dialog.h"
#include "gui/settings_binding.h"
#include "gui/permission_dialog.h"
#include "core/config/config_json.h"
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
#include "platform/mac/login/login_item.h"
#include "platform/mac/permissions.h"
#else
#include "platform/win/login/login_item.h"
#include "platform/win/permissions.h"
#endif

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QDateTime>
#include <filesystem>
#include <fstream>

// ============================================================================
// 1. Vector Icon Drawing Helper (100% Native Vector Graphics, NO Emojis)
// ============================================================================
static void paintVectorGlyph(QPainter& p, IconType type, const QRectF& r, const QColor& color) {
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    const qreal cx = r.center().x();
    const qreal cy = r.center().y();
    const qreal sz = std::min(r.width(), r.height());

    switch (type) {
        case IconType::Gear: {
            p.drawEllipse(QPointF(cx, cy), sz * 0.22, sz * 0.22);
            for (int i = 0; i < 8; ++i) {
                p.save();
                p.translate(cx, cy);
                p.rotate(i * 45);
                p.drawLine(QPointF(0, -sz * 0.32), QPointF(0, -sz * 0.44));
                p.restore();
            }
            break;
        }
        case IconType::Grid: {
            qreal b = sz * 0.35;
            p.drawRoundedRect(QRectF(cx - b - 1, cy - b - 1, b, b), 2, 2);
            p.drawRoundedRect(QRectF(cx + 1,     cy - b - 1, b, b), 2, 2);
            p.drawRoundedRect(QRectF(cx - b - 1, cy + 1,     b, b), 2, 2);
            p.drawRoundedRect(QRectF(cx + 1,     cy + 1,     b, b), 2, 2);
            break;
        }
        case IconType::Display: {
            QRectF scr(cx - sz * 0.42, cy - sz * 0.35, sz * 0.84, sz * 0.55);
            p.drawRoundedRect(scr, 3, 3);
            p.drawLine(QPointF(cx, scr.bottom()), QPointF(cx, cy + sz * 0.38));
            p.drawLine(QPointF(cx - sz * 0.22, cy + sz * 0.38), QPointF(cx + sz * 0.22, cy + sz * 0.38));
            break;
        }
        case IconType::Book: {
            p.drawArc(QRectF(cx - sz * 0.4, cy - sz * 0.25, sz * 0.4, sz * 0.3), 0, 180 * 16);
            p.drawArc(QRectF(cx,           cy - sz * 0.25, sz * 0.4, sz * 0.3), 0, 180 * 16);
            p.drawLine(QPointF(cx, cy - sz * 0.1), QPointF(cx, cy + sz * 0.35));
            p.drawLine(QPointF(cx - sz * 0.4, cy - sz * 0.1), QPointF(cx - sz * 0.4, cy + sz * 0.35));
            p.drawLine(QPointF(cx + sz * 0.4, cy - sz * 0.1), QPointF(cx + sz * 0.4, cy + sz * 0.35));
            p.drawLine(QPointF(cx - sz * 0.4, cy + sz * 0.35), QPointF(cx, cy + sz * 0.35));
            p.drawLine(QPointF(cx, cy + sz * 0.35), QPointF(cx + sz * 0.4, cy + sz * 0.35));
            break;
        }
        case IconType::Shield: {
            QPainterPath path;
            path.moveTo(cx, cy - sz * 0.42);
            path.lineTo(cx + sz * 0.36, cy - sz * 0.28);
            path.quadTo(cx + sz * 0.36, cy + sz * 0.18, cx, cy + sz * 0.42);
            path.quadTo(cx - sz * 0.36, cy + sz * 0.18, cx - sz * 0.36, cy - sz * 0.28);
            path.closeSubpath();
            p.drawPath(path);
            break;
        }
        case IconType::Terminal: {
            QRectF box(cx - sz * 0.42, cy - sz * 0.35, sz * 0.84, sz * 0.7);
            p.drawRoundedRect(box, 3, 3);
            p.drawLine(QPointF(cx - sz * 0.24, cy - sz * 0.14), QPointF(cx - sz * 0.1, cy));
            p.drawLine(QPointF(cx - sz * 0.1, cy), QPointF(cx - sz * 0.24, cy + sz * 0.14));
            p.drawLine(QPointF(cx, cy + sz * 0.14), QPointF(cx + sz * 0.2, cy + sz * 0.14));
            break;
        }
        case IconType::Lock: {
            p.drawArc(QRectF(cx - sz * 0.2, cy - sz * 0.4, sz * 0.4, sz * 0.4), 0, 180 * 16);
            p.drawRoundedRect(QRectF(cx - sz * 0.28, cy - sz * 0.15, sz * 0.56, sz * 0.48), 3, 3);
            p.drawEllipse(QPointF(cx, cy + sz * 0.05), 1.5, 1.5);
            break;
        }
        case IconType::Laptop: {
            QRectF lid(cx - sz * 0.34, cy - sz * 0.34, sz * 0.68, sz * 0.46);
            p.drawRoundedRect(lid, 2, 2);
            p.drawLine(QPointF(cx - sz * 0.45, cy + sz * 0.22), QPointF(cx + sz * 0.45, cy + sz * 0.22));
            p.drawLine(QPointF(cx - sz * 0.45, cy + sz * 0.22), QPointF(cx - sz * 0.38, cy + sz * 0.3));
            p.drawLine(QPointF(cx - sz * 0.38, cy + sz * 0.3),  QPointF(cx + sz * 0.38, cy + sz * 0.3));
            p.drawLine(QPointF(cx + sz * 0.38, cy + sz * 0.3),  QPointF(cx + sz * 0.45, cy + sz * 0.22));
            break;
        }
        case IconType::Globe: {
            p.drawEllipse(QPointF(cx, cy), sz * 0.38, sz * 0.38);
            p.drawLine(QPointF(cx - sz * 0.38, cy), QPointF(cx + sz * 0.38, cy));
            p.drawEllipse(QPointF(cx, cy), sz * 0.18, sz * 0.38);
            break;
        }
        case IconType::Bolt: {
            QPainterPath path;
            path.moveTo(cx + sz * 0.05, cy - sz * 0.42);
            path.lineTo(cx - sz * 0.28, cy);
            path.lineTo(cx - sz * 0.02, cy);
            path.lineTo(cx - sz * 0.12, cy + sz * 0.42);
            path.lineTo(cx + sz * 0.28, cy - sz * 0.02);
            path.lineTo(cx + sz * 0.02, cy - sz * 0.02);
            path.closeSubpath();
            p.drawPath(path);
            break;
        }
        case IconType::Undo: {
            QPainterPath path;
            path.moveTo(cx + sz * 0.28, cy + sz * 0.24);
            path.quadTo(cx + sz * 0.28, cy - sz * 0.22, cx - sz * 0.05, cy - sz * 0.22);
            p.drawPath(path);
            p.drawLine(QPointF(cx - sz * 0.25, cy - sz * 0.22), QPointF(cx - sz * 0.05, cy - sz * 0.22));
            p.drawLine(QPointF(cx - sz * 0.25, cy - sz * 0.22), QPointF(cx - sz * 0.1, cy - sz * 0.36));
            p.drawLine(QPointF(cx - sz * 0.25, cy - sz * 0.22), QPointF(cx - sz * 0.1, cy - sz * 0.08));
            break;
        }
        case IconType::Message: {
            QPainterPath path;
            path.addRoundedRect(QRectF(cx - sz * 0.38, cy - sz * 0.32, sz * 0.76, sz * 0.52), 4, 4);
            path.moveTo(cx - sz * 0.15, cy + sz * 0.2);
            path.lineTo(cx - sz * 0.28, cy + sz * 0.38);
            path.lineTo(cx - sz * 0.02, cy + sz * 0.2);
            p.drawPath(path);
            break;
        }
        case IconType::TextCase: {
            QFont f = p.font();
            f.setPointSize(static_cast<int>(sz * 0.55));
            f.setWeight(QFont::Bold);
            p.setFont(f);
            p.setPen(color);
            p.drawText(r, Qt::AlignCenter, "Aa");
            break;
        }
        case IconType::WordSplit: {
            p.drawRoundedRect(QRectF(cx - sz * 0.42, cy - sz * 0.24, sz * 0.36, sz * 0.48), 2, 2);
            p.drawRoundedRect(QRectF(cx + sz * 0.06, cy - sz * 0.24, sz * 0.36, sz * 0.48), 2, 2);
            p.setPen(QPen(color, 1.4, Qt::DashLine));
            p.drawLine(QPointF(cx, cy - sz * 0.32), QPointF(cx, cy + sz * 0.32));
            break;
        }
        case IconType::Tag: {
            QPainterPath path;
            path.moveTo(cx - sz * 0.35, cy - sz * 0.2);
            path.lineTo(cx, cy - sz * 0.38);
            path.lineTo(cx + sz * 0.35, cy);
            path.lineTo(cx, cy + sz * 0.38);
            path.lineTo(cx - sz * 0.35, cy + sz * 0.2);
            path.closeSubpath();
            p.drawPath(path);
            p.drawEllipse(QPointF(cx - sz * 0.15, cy), 1.5, 1.5);
            break;
        }
        case IconType::Gauge: {
            p.drawArc(QRectF(cx - sz * 0.38, cy - sz * 0.38, sz * 0.76, sz * 0.76), 0, 180 * 16);
            p.drawLine(QPointF(cx, cy), QPointF(cx + sz * 0.2, cy - sz * 0.2));
            break;
        }
        case IconType::Ruler: {
            QRectF box(cx - sz * 0.45, cy - sz * 0.2, sz * 0.9, sz * 0.4);
            p.drawRoundedRect(box, 2, 2);
            p.drawLine(QPointF(cx - sz * 0.22, box.top()), QPointF(cx - sz * 0.22, box.top() + 4));
            p.drawLine(QPointF(cx,             box.top()), QPointF(cx,             box.top() + 6));
            p.drawLine(QPointF(cx + sz * 0.22, box.top()), QPointF(cx + sz * 0.22, box.top() + 4));
            break;
        }
        case IconType::MenuBar: {
            QRectF bar(cx - sz * 0.45, cy - sz * 0.25, sz * 0.9, sz * 0.5);
            p.drawRoundedRect(bar, 2, 2);
            p.drawLine(QPointF(bar.left(), cy - sz * 0.08), QPointF(bar.right(), cy - sz * 0.08));
            p.drawEllipse(QPointF(cx + sz * 0.28, cy - sz * 0.16), 1.5, 1.5);
            break;
        }
        case IconType::Accessibility: {
            p.drawEllipse(QPointF(cx, cy), sz * 0.4, sz * 0.4);
            p.drawEllipse(QPointF(cx, cy - sz * 0.2), 1.5, 1.5);
            p.drawLine(QPointF(cx - sz * 0.25, cy - sz * 0.08), QPointF(cx + sz * 0.25, cy - sz * 0.08));
            p.drawLine(QPointF(cx, cy - sz * 0.12), QPointF(cx, cy + sz * 0.16));
            p.drawLine(QPointF(cx, cy + sz * 0.16), QPointF(cx - sz * 0.15, cy + sz * 0.32));
            p.drawLine(QPointF(cx, cy + sz * 0.16), QPointF(cx + sz * 0.15, cy + sz * 0.32));
            break;
        }
        case IconType::Keyboard: {
            QRectF kb(cx - sz * 0.42, cy - sz * 0.25, sz * 0.84, sz * 0.5);
            p.drawRoundedRect(kb, 3, 3);
            p.drawLine(QPointF(cx - sz * 0.2, kb.bottom() - 3), QPointF(cx + sz * 0.2, kb.bottom() - 3));
            break;
        }
        case IconType::Activity: {
            QPainterPath path;
            path.moveTo(cx - sz * 0.4, cy);
            path.lineTo(cx - sz * 0.18, cy);
            path.lineTo(cx - sz * 0.08, cy - sz * 0.32);
            path.lineTo(cx + sz * 0.05, cy + sz * 0.32);
            path.lineTo(cx + sz * 0.15, cy);
            path.lineTo(cx + sz * 0.4, cy);
            p.drawPath(path);
            break;
        }
        case IconType::Database: {
            p.drawEllipse(QPointF(cx, cy - sz * 0.25), sz * 0.38, sz * 0.14);
            p.drawArc(QRectF(cx - sz * 0.38, cy - sz * 0.12, sz * 0.76, sz * 0.28), 180 * 16, 180 * 16);
            p.drawArc(QRectF(cx - sz * 0.38, cy + sz * 0.08,  sz * 0.76, sz * 0.28), 180 * 16, 180 * 16);
            p.drawLine(QPointF(cx - sz * 0.38, cy - sz * 0.25), QPointF(cx - sz * 0.38, cy + sz * 0.22));
            p.drawLine(QPointF(cx + sz * 0.38, cy - sz * 0.25), QPointF(cx + sz * 0.38, cy + sz * 0.22));
            break;
        }
    }
}

QIcon SettingsDialog::renderVectorIcon(IconType type, const QColor& color, int size) {
    QPixmap px(size * 2, size * 2);
    px.fill(Qt::transparent);
    px.setDevicePixelRatio(2.0);

    QPainter p(&px);
    paintVectorGlyph(p, type, QRectF(0, 0, size, size), color);

    return QIcon(px);
}

QWidget* SettingsDialog::createIconBadge(IconType type) {
    class BadgeWidget : public QWidget {
        IconType type_;
    public:
        BadgeWidget(IconType t, QWidget* parent = nullptr) : QWidget(parent), type_(t) {
            setFixedSize(34, 34);
            setAttribute(Qt::WA_TransparentForMouseEvents);
        }
    protected:
        void paintEvent(QPaintEvent*) override {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);

            bool isDark = false;
            for (QWidget* w = parentWidget(); w; w = w->parentWidget()) {
                if (auto* dlg = qobject_cast<SettingsDialog*>(w)) {
                    isDark = dlg->isDarkTheme();
                    break;
                }
            }

            QColor bg = isDark ? QColor(30, 41, 59) : QColor(243, 244, 246);
            QColor border = isDark ? QColor(51, 65, 85) : QColor(229, 231, 235);
            QColor stroke = isDark ? QColor(248, 250, 252) : QColor(17, 24, 39);

            p.setBrush(bg);
            p.setPen(QPen(border, 1.0));
            p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);

            paintVectorGlyph(p, type_, QRectF(7, 7, 20, 20), stroke);
        }
    };

    return new BadgeWidget(type);
}

// ============================================================================
// 2. ToggleSwitch Implementation
// ============================================================================
ToggleSwitch::ToggleSwitch(QWidget* parent) : QAbstractButton(parent) {
    setCheckable(true);
    setChecked(false);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    anim_ = new QPropertyAnimation(this, "offset", this);
    anim_->setDuration(160);
    anim_->setEasingCurve(QEasingCurve::InOutQuad);
}

void ToggleSwitch::nextCheckState() {
    QAbstractButton::nextCheckState();
    bool checked = isChecked();
    anim_->stop();
    anim_->setStartValue(offset_);
    anim_->setEndValue(checked ? 1.0 : 0.0);
    anim_->start();
}

void ToggleSwitch::checkStateSet() {
    QAbstractButton::checkStateSet();
    offset_ = isChecked() ? 1.0 : 0.0;
    update();
}

void ToggleSwitch::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = 42;
    const int h = 24;
    const int radius = h / 2;

    // Track background
    QColor trackOff(209, 213, 219);     // #D1D5DB
    QColor trackOn(52, 199, 89);        // Apple green #34C759
    QColor currentTrack(
        static_cast<int>(trackOff.red()   + offset_ * (trackOn.red()   - trackOff.red())),
        static_cast<int>(trackOff.green() + offset_ * (trackOn.green() - trackOff.green())),
        static_cast<int>(trackOff.blue()  + offset_ * (trackOn.blue()  - trackOff.blue()))
    );

    p.setPen(Qt::NoPen);
    p.setBrush(currentTrack);
    p.drawRoundedRect(0, 0, w, h, radius, radius);

    // Knob
    const int knobMargin = 2;
    const int knobDiameter = h - (knobMargin * 2);
    const qreal minX = knobMargin;
    const qreal maxX = w - knobDiameter - knobMargin;
    const qreal currentX = minX + offset_ * (maxX - minX);

    // Subtle drop shadow
    p.setBrush(QColor(0, 0, 0, 24));
    p.drawEllipse(QPointF(currentX + knobDiameter / 2.0, knobMargin + knobDiameter / 2.0 + 1.0), knobDiameter / 2.0, knobDiameter / 2.0);

    // Knob body
    p.setBrush(Qt::white);
    p.drawEllipse(QPointF(currentX + knobDiameter / 2.0, knobMargin + knobDiameter / 2.0), knobDiameter / 2.0, knobDiameter / 2.0);
}

// ============================================================================
// 3. ThemePreviewCard Implementation
// ============================================================================
ThemePreviewCard::ThemePreviewCard(ThemeMode mode, const QString& label, QWidget* parent)
    : QWidget(parent), mode_(mode), label_(label) {
    setFixedSize(140, 110);
    setCursor(Qt::PointingHandCursor);
}

void ThemePreviewCard::setSelected(bool s) {
    if (selected_ != s) {
        selected_ = s;
        update();
    }
}

void ThemePreviewCard::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    QWidget::mousePressEvent(event);
}

void ThemePreviewCard::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Draw preview box (top 75px)
    QRect previewRect(4, 4, width() - 8, 72);
    QPainterPath path;
    path.addRoundedRect(previewRect, 8, 8);

    if (mode_ == ThemeMode::Light) {
        p.fillPath(path, QColor(243, 244, 246));
        p.setPen(QColor(229, 231, 235));
        p.drawPath(path);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(254, 202, 202)); p.drawEllipse(12, 12, 6, 6);
        p.setBrush(QColor(254, 240, 138)); p.drawEllipse(22, 12, 6, 6);
        p.setBrush(QColor(187, 247, 208)); p.drawEllipse(32, 12, 6, 6);

        p.setBrush(QColor(209, 213, 219));
        p.drawRoundedRect(12, 26, 40, 6, 3, 3);
        p.drawRoundedRect(60, 26, 60, 6, 3, 3);
        p.drawRoundedRect(12, 38, 50, 6, 3, 3);
        p.drawRoundedRect(70, 38, 50, 6, 3, 3);
    } else if (mode_ == ThemeMode::Dark) {
        p.fillPath(path, QColor(31, 41, 55));
        p.setPen(QColor(55, 65, 81));
        p.drawPath(path);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(239, 68, 68)); p.drawEllipse(12, 12, 6, 6);
        p.setBrush(QColor(234, 179, 8)); p.drawEllipse(22, 12, 6, 6);
        p.setBrush(QColor(34, 197, 94)); p.drawEllipse(32, 12, 6, 6);

        p.setBrush(QColor(75, 85, 99));
        p.drawRoundedRect(12, 26, 40, 6, 3, 3);
        p.drawRoundedRect(60, 26, 60, 6, 3, 3);
        p.drawRoundedRect(12, 38, 50, 6, 3, 3);
        p.drawRoundedRect(70, 38, 50, 6, 3, 3);
    } else { // System (diagonal split)
        p.fillPath(path, QColor(243, 244, 246));
        p.save();
        p.setClipPath(path);
        QPainterPath darkHalf;
        darkHalf.moveTo(previewRect.topLeft());
        darkHalf.lineTo(previewRect.bottomRight());
        darkHalf.lineTo(previewRect.bottomLeft());
        darkHalf.closeSubpath();
        p.fillPath(darkHalf, QColor(31, 41, 55));
        p.restore();

        p.setPen(QColor(229, 231, 235));
        p.drawPath(path);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(239, 68, 68)); p.drawEllipse(12, 12, 6, 6);
        p.setBrush(QColor(234, 179, 8)); p.drawEllipse(22, 12, 6, 6);
        p.setBrush(QColor(34, 197, 94)); p.drawEllipse(32, 12, 6, 6);
    }

    if (selected_) {
        p.setPen(QPen(QColor(245, 158, 11), 2.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(previewRect.adjusted(-1, -1, 1, 1), 9, 9);
    }

    p.setPen(selected_ ? QColor(217, 119, 6) : QColor(55, 65, 81));
    QFont font = p.font();
    font.setPointSize(11);
    font.setWeight(selected_ ? QFont::DemiBold : QFont::Medium);
    p.setFont(font);
    p.drawText(QRect(0, 84, width(), 22), Qt::AlignCenter, label_);
}

// ============================================================================
// 4. SettingsDialog Implementation
// ============================================================================
static std::string getConfigFilePath() {
    std::string path;
#ifdef _WIN32
    const char* appData = getenv("APPDATA");
    path = (appData ? appData : ".") + std::string("/Inline");
#else
    const char* home = getenv("HOME");
    path = (home ? home : ".") + std::string("/Library/Application Support/Inline");
#endif
    std::filesystem::create_directories(path);
    return path + "/config.json";
}

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("System Settings"));
    resize(840, 620);
    setMinimumSize(760, 540);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    setupUi();
    loadFromConfig();

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &SettingsDialog::refreshPermissions);
    refreshTimer_->start(1500);
}

void SettingsDialog::applyTheme(ThemePreviewCard::ThemeMode mode) {
    currentTheme_ = mode;
    bool isDark = (mode == ThemePreviewCard::ThemeMode::Dark);
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    if (mode == ThemePreviewCard::ThemeMode::System) {
        isDark = isSystemInDarkMode();
    }
#endif
    isDarkTheme_ = isDark;

    QString mainBg = isDark ? "#111827" : "#F8F9FA";
    QString sidebarBg = isDark ? "#0F172A" : "#F1F2F4";
    QString sidebarBorder = isDark ? "#1F2937" : "#E5E7EB";
    QString searchBg = isDark ? "#1E293B" : "#E5E7EB";
    QString searchBorder = isDark ? "#334155" : "#D1D5DB";
    QString cardBg = isDark ? "#1E293B" : "#FFFFFF";
    QString cardBorder = isDark ? "#334155" : "#E5E7EB";
    QString textColor = isDark ? "#F8FAFC" : "#111827";
    QString subtextColor = isDark ? "#94A3B8" : "#6B7280";
    QString dividerColor = isDark ? "#334155" : "#F3F4F6";
    QString inputBg = isDark ? "#0F172A" : "#FFFFFF";
    QString inputBorder = isDark ? "#334155" : "#E5E7EB";

    setStyleSheet(QString(
        "SettingsDialog {"
        "  background-color: %1;"
        "  color: %2;"
        "  font-family: -apple-system, BlinkMacSystemFont, 'SF Pro Text', 'SF Pro Display', 'Helvetica Neue', Arial, sans-serif;"
        "}"
        "#sidebarContainer {"
        "  background-color: %3;"
        "  border-right: 1px solid %4;"
        "}"
        "#searchEdit {"
        "  background-color: %5;"
        "  border: 1px solid %6;"
        "  border-radius: 8px;"
        "  padding: 6px 10px;"
        "  font-size: 12px;"
        "  color: %2;"
        "}"
        "#settingCard {"
        "  background-color: %7;"
        "  border: 1px solid %8;"
        "  border-radius: 12px;"
        "}"
        "#settingRowTitle {"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  color: %2;"
        "}"
        "#settingRowSubtitle {"
        "  font-size: 11px;"
        "  font-weight: 400;"
        "  color: %9;"
        "}"
        "#settingRowDivider {"
        "  background-color: %10;"
        "  height: 1px;"
        "  border: none;"
        "}"
        "#pageHeader {"
        "  font-size: 22px;"
        "  font-weight: 700;"
        "  color: %2;"
        "}"
        "#sectionHeader {"
        "  font-size: 11px;"
        "  font-weight: 700;"
        "  color: %9;"
        "  letter-spacing: 0.05em;"
        "}"
        "QScrollArea {"
        "  border: none;"
        "  background-color: transparent;"
        "}"
        "QLineEdit, QPlainTextEdit, QComboBox {"
        "  background-color: %11;"
        "  border: 1px solid %12;"
        "  border-radius: 8px;"
        "  padding: 6px 10px;"
        "  font-size: 13px;"
        "  color: %2;"
        "}"
        "QLineEdit:focus, QPlainTextEdit:focus, QComboBox:focus {"
        "  border: 1px solid #3B82F6;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  padding-right: 8px;"
        "}"
        "QPushButton {"
        "  background-color: %13;"
        "  border: 1px solid %14;"
        "  border-radius: 8px;"
        "  padding: 6px 14px;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  color: %2;"
        "}"
        "QListWidget {"
        "  background-color: transparent;"
        "  border: none;"
        "  outline: none;"
        "}"
        "QListWidget::item {"
        "  padding: 7px 10px;"
        "  border-radius: 8px;"
        "  margin: 1px 4px;"
        "  font-size: 13px;"
        "  color: %9;"
        "  font-weight: 500;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: %15;"
        "  color: %2;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: %16;"
        "  color: %2;"
        "  font-weight: 600;"
        "}"
    ).arg(
        mainBg, textColor, sidebarBg, sidebarBorder, searchBg, searchBorder,
        cardBg, cardBorder, subtextColor, dividerColor, inputBg, inputBorder,
        isDark ? "#334155" : "#E5E7EB", isDark ? "#475569" : "#D1D5DB",
        isDark ? "#1E293B" : "#E5E7EB", isDark ? "#334155" : "#E2E8F0"
    ));

    buildSidebar();
    update();
}

void SettingsDialog::setupUi() {
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ==========================================
    // Left Sidebar Pane
    // ==========================================
    auto* sidebarContainer = new QWidget(this);
    sidebarContainer->setObjectName("sidebarContainer");
    sidebarContainer->setFixedWidth(230);
    auto* sidebarLayout = new QVBoxLayout(sidebarContainer);
    sidebarLayout->setContentsMargins(10, 14, 10, 14);
    sidebarLayout->setSpacing(8);

    // Search bar
    searchEdit_ = new QLineEdit(sidebarContainer);
    searchEdit_->setObjectName("searchEdit");
    searchEdit_->setPlaceholderText(tr("Search settings"));
    connect(searchEdit_, &QLineEdit::textChanged, this, &SettingsDialog::onSearchFilterChanged);
    sidebarLayout->addWidget(searchEdit_);

    // Sidebar navigation list
    sidebarList_ = new QListWidget(sidebarContainer);
    buildSidebar();
    connect(sidebarList_, &QListWidget::currentRowChanged, this, &SettingsDialog::onSidebarItemChanged);
    sidebarLayout->addWidget(sidebarList_);

    // Version label at bottom
    auto* versionLabel = new QLabel(tr("Inline macOS • v1.1"), sidebarContainer);
    versionLabel->setStyleSheet("font-size: 10px; color: #9CA3AF; padding: 4px 6px; font-weight: 500;");
    sidebarLayout->addWidget(versionLabel);

    mainLayout->addWidget(sidebarContainer);

    // ==========================================
    // Right Content Area (Stacked Pages)
    // ==========================================
    pageStack_ = new QStackedWidget(this);
    pageStack_->addWidget(createGeneralPage());        // 0
    pageStack_->addWidget(createFeaturesPage());       // 1
    pageStack_->addWidget(createAppearancePage());     // 2
    pageStack_->addWidget(createVocabularyPage());     // 3
    pageStack_->addWidget(createExcludedAppsPage());   // 4
    pageStack_->addWidget(createPlaygroundPage());     // 5
    pageStack_->addWidget(createPermissionsPage());    // 6

    mainLayout->addWidget(pageStack_, 1);
    sidebarList_->setCurrentRow(1);
}

void SettingsDialog::buildSidebar() {
    sidebarList_->clear();

    struct NavItem {
        IconType icon;
        QString title;
        bool isHeader;
    };

    const std::vector<NavItem> items = {
        {IconType::Gear, tr("ESSENTIALS"), true},
        {IconType::Gear, tr("General"), false},
        {IconType::Grid, tr("Features"), false},
        {IconType::Display, tr("Appearance"), false},
        {IconType::Book, tr("CUSTOMIZATION"), true},
        {IconType::Book, tr("Vocabulary & Slang"), false},
        {IconType::Shield, tr("Excluded Apps"), false},
        {IconType::Terminal, tr("UTILITIES"), true},
        {IconType::Terminal, tr("Playground"), false},
        {IconType::Lock, tr("Permissions & Health"), false}
    };

    QColor iconCol = isDarkTheme_ ? QColor(156, 163, 175) : QColor(75, 85, 99);
    for (const auto& item : items) {
        auto* listItem = new QListWidgetItem(sidebarList_);
        if (item.isHeader) {
            listItem->setText(item.title);
            listItem->setFlags(Qt::NoItemFlags);
            QFont f = listItem->font();
            f.setPointSize(9);
            f.setWeight(QFont::Bold);
            listItem->setFont(f);
            listItem->setForeground(QColor(156, 163, 175));
            listItem->setSizeHint(QSize(200, 26));
        } else {
            listItem->setText(item.title);
            listItem->setIcon(renderVectorIcon(item.icon, iconCol, 16));
            listItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            listItem->setSizeHint(QSize(200, 34));
        }
        sidebarList_->addItem(listItem);
    }
}

void SettingsDialog::onSidebarItemChanged(int index) {
    int targetPage = 0;
    if (index == 1) targetPage = 0;
    else if (index == 2) targetPage = 1;
    else if (index == 3) targetPage = 2;
    else if (index == 5) targetPage = 3;
    else if (index == 6) targetPage = 4;
    else if (index == 8) targetPage = 5;
    else if (index == 9) targetPage = 6;
    else return;

    pageStack_->setCurrentIndex(targetPage);
}

void SettingsDialog::onSearchFilterChanged(const QString& query) {
    QString q = query.trimmed().toLower();
    for (int i = 0; i < sidebarList_->count(); ++i) {
        auto* item = sidebarList_->item(i);
        if (item->flags() == Qt::NoItemFlags) {
            item->setHidden(!q.isEmpty());
            continue;
        }
        item->setHidden(!q.isEmpty() && !item->text().toLower().contains(q));
    }
}

QWidget* SettingsDialog::createSettingCard() {
    auto* card = new QWidget(this);
    card->setObjectName("settingCard");
    return card;
}

QWidget* SettingsDialog::createSettingRow(IconType icon, const QString& title, const QString& subtitle, QWidget* controlWidget, bool isLast) {
    auto* row = new QWidget();
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(14, 12, 14, 12);
    rowLayout->setSpacing(12);

    // Vector Icon badge in monochrome rounded container
    rowLayout->addWidget(createIconBadge(icon));

    // Text descriptions
    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    auto* titleLabel = new QLabel(title, row);
    titleLabel->setObjectName("settingRowTitle");
    textLayout->addWidget(titleLabel);

    if (!subtitle.isEmpty()) {
        auto* subLabel = new QLabel(subtitle, row);
        subLabel->setObjectName("settingRowSubtitle");
        subLabel->setWordWrap(true);
        textLayout->addWidget(subLabel);
    }
    rowLayout->addLayout(textLayout, 1);

    if (controlWidget) {
        rowLayout->addWidget(controlWidget, 0, Qt::AlignRight | Qt::AlignVCenter);
    }

    if (!isLast) {
        auto* container = new QWidget();
        auto* containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);
        containerLayout->addWidget(row);

        auto* divider = new QFrame();
        divider->setObjectName("settingRowDivider");
        divider->setFrameShape(QFrame::HLine);
        divider->setFrameShadow(QFrame::Plain);
        containerLayout->addWidget(divider);
        return container;
    }

    return row;
}

// ============================================================================
// Page 0: General
// ============================================================================
QWidget* SettingsDialog::createGeneralPage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 28, 28, 28);
    layout->setSpacing(16);

    auto* header = new QLabel(tr("General"), content);
    header->setStyleSheet("font-size: 22px; font-weight: 700; color: #111827;");
    layout->addWidget(header);

    // Card 1: Core System Integration
    auto* card1 = createSettingCard();
    auto* card1Layout = new QVBoxLayout(card1);
    card1Layout->setContentsMargins(0, 0, 0, 0);
    card1Layout->setSpacing(0);

    launchAtLoginToggle_ = new ToggleSwitch(card1);
    launchAtLoginToggle_->setChecked(true);
    connect(launchAtLoginToggle_, &QAbstractButton::toggled, this, &SettingsDialog::saveToConfig);
    card1Layout->addWidget(createSettingRow(IconType::Laptop, tr("Launch at login"), tr("Opens automatically in the menu bar every time you start your Mac."), launchAtLoginToggle_, false));

    languageCombo_ = new QComboBox(card1);
    languageCombo_->addItem("English (US)");
    languageCombo_->setFixedWidth(130);
    card1Layout->addWidget(createSettingRow(IconType::Globe, tr("Language"), tr("Active autocorrection vocabulary rules and dictionary."), languageCombo_, true));
    layout->addWidget(card1);

    // Card 2: Engine Status & Undo
    auto* card2 = createSettingCard();
    auto* card2Layout = new QVBoxLayout(card2);
    card2Layout->setContentsMargins(0, 0, 0, 0);
    card2Layout->setSpacing(0);

    masterEnabledToggle_ = new ToggleSwitch(card2);
    masterEnabledToggle_->setChecked(true);
    connect(masterEnabledToggle_, &QAbstractButton::toggled, this, &SettingsDialog::saveToConfig);
    card2Layout->addWidget(createSettingRow(IconType::Bolt, tr("Autocorrect Engine"), tr("Instantly fix typos and misspellings across all native macOS apps."), masterEnabledToggle_, false));

    singleKeyUndoToggle_ = new ToggleSwitch(card2);
    singleKeyUndoToggle_->setChecked(true);
    connect(singleKeyUndoToggle_, &QAbstractButton::toggled, this, &SettingsDialog::saveToConfig);
    card2Layout->addWidget(createSettingRow(IconType::Undo, tr("Single-Key Undo on Backspace"), tr("Pressing Backspace right after a correction reverts the word back to your exact input."), singleKeyUndoToggle_, true));
    layout->addWidget(card2);

    // Card 3: Feedback
    auto* card3 = createSettingCard();
    auto* card3Layout = new QVBoxLayout(card3);
    card3Layout->setContentsMargins(0, 0, 0, 0);
    card3Layout->setSpacing(0);

    auto* feedbackBtn = new QPushButton(tr("Send feedback"), card3);
    connect(feedbackBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/LaxExe/Inline-MacOS-Win32Build/issues"));
    });
    card3Layout->addWidget(createSettingRow(IconType::Message, tr("Feedback"), tr("Send a bug report or feature idea directly to the person who maintains Inline."), feedbackBtn, true));
    layout->addWidget(card3);

    layout->addStretch();
    scroll->setWidget(content);
    return scroll;
}

// ============================================================================
// Page 1: Features
// ============================================================================
QWidget* SettingsDialog::createFeaturesPage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 28, 28, 28);
    layout->setSpacing(16);

    auto* header = new QLabel(tr("Features"), content);
    header->setStyleSheet("font-size: 22px; font-weight: 700; color: #111827;");
    layout->addWidget(header);

    auto* sec1 = new QLabel(tr("INTELLIGENT CORRECTIONS"), content);
    sec1->setStyleSheet("font-size: 11px; font-weight: 700; color: #6B7280; letter-spacing: 0.05em;");
    layout->addWidget(sec1);

    auto* card1 = createSettingCard();
    auto* card1Layout = new QVBoxLayout(card1);
    card1Layout->setContentsMargins(0, 0, 0, 0);
    card1Layout->setSpacing(0);

    sentenceCorrectionToggle_ = new ToggleSwitch(card1);
    sentenceCorrectionToggle_->setChecked(true);
    connect(sentenceCorrectionToggle_, &QAbstractButton::toggled, this, &SettingsDialog::saveToConfig);
    card1Layout->addWidget(createSettingRow(IconType::TextCase, tr("Sentence Capitalization"), tr("Automatically capitalizes the first letter of sentences and quotes."), sentenceCorrectionToggle_, false));

    runTogetherToggle_ = new ToggleSwitch(card1);
    runTogetherToggle_->setChecked(true);
    connect(runTogetherToggle_, &QAbstractButton::toggled, this, &SettingsDialog::saveToConfig);
    card1Layout->addWidget(createSettingRow(IconType::WordSplit, tr("Run-Together Word Splitting"), tr("Detects concatenated words (e.g. 'whenI', 'twowords') and cleanly adds spaces."), runTogetherToggle_, false));

    auto* slangBadge = new QLabel(tr("Protected"), card1);
    slangBadge->setStyleSheet("background-color: #DCFCE7; color: #166534; font-size: 11px; font-weight: 700; border-radius: 6px; padding: 4px 8px;");
    card1Layout->addWidget(createSettingRow(IconType::Tag, tr("Slang & Chat Acronyms"), tr("Preserves words like 'gng', 'brb', 'ttyl', 'idk', 'ngl', and 'repo' without corruption."), slangBadge, true));
    layout->addWidget(card1);

    auto* sec2 = new QLabel(tr("TUNING & THRESHOLDS"), content);
    sec2->setStyleSheet("font-size: 11px; font-weight: 700; color: #6B7280; letter-spacing: 0.05em;");
    layout->addWidget(sec2);

    auto* card2 = createSettingCard();
    auto* card2Layout = new QVBoxLayout(card2);
    card2Layout->setContentsMargins(0, 0, 0, 0);
    card2Layout->setSpacing(0);

    sensitivityCombo_ = new QComboBox(card2);
    sensitivityCombo_->addItem(tr("Conservative"));
    sensitivityCombo_->addItem(tr("Balanced"));
    sensitivityCombo_->addItem(tr("Aggressive"));
    sensitivityCombo_->setFixedWidth(130);
    connect(sensitivityCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::saveToConfig);
    card2Layout->addWidget(createSettingRow(IconType::Gauge, tr("Correction Confidence"), tr("Determines how confident the bigram & frequency model must be before replacing text."), sensitivityCombo_, false));

    editDistanceCombo_ = new QComboBox(card2);
    editDistanceCombo_->addItem(tr("1 letter mistake"));
    editDistanceCombo_->addItem(tr("2 letter mistakes"));
    editDistanceCombo_->setFixedWidth(130);
    connect(editDistanceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::saveToConfig);
    card2Layout->addWidget(createSettingRow(IconType::Ruler, tr("Maximum Typo Distance"), tr("Allows correcting typos that are up to 1 or 2 letters apart from the real word."), editDistanceCombo_, true));
    layout->addWidget(card2);

    layout->addStretch();
    scroll->setWidget(content);
    return scroll;
}

// ============================================================================
// Page 2: Appearance
// ============================================================================
QWidget* SettingsDialog::createAppearancePage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 28, 28, 28);
    layout->setSpacing(16);

    auto* header = new QLabel(tr("Appearance"), content);
    header->setStyleSheet("font-size: 22px; font-weight: 700; color: #111827;");
    layout->addWidget(header);

    auto* themeCard = createSettingCard();
    auto* themeLayout = new QVBoxLayout(themeCard);
    themeLayout->setContentsMargins(16, 16, 16, 16);
    themeLayout->setSpacing(12);

    auto* themeTitle = new QLabel(tr("Interface Theme"), themeCard);
    themeTitle->setStyleSheet("font-size: 13px; font-weight: 600; color: #111827;");
    themeLayout->addWidget(themeTitle);

    auto* cardRow = new QHBoxLayout();
    cardRow->setSpacing(16);

    systemThemeCard_ = new ThemePreviewCard(ThemePreviewCard::ThemeMode::System, tr("System"), themeCard);
    lightThemeCard_ = new ThemePreviewCard(ThemePreviewCard::ThemeMode::Light, tr("Light"), themeCard);
    darkThemeCard_ = new ThemePreviewCard(ThemePreviewCard::ThemeMode::Dark, tr("Dark"), themeCard);

    systemThemeCard_->setSelected(true);

    connect(systemThemeCard_, &ThemePreviewCard::clicked, this, [this]() { onThemeSelected(ThemePreviewCard::ThemeMode::System); });
    connect(lightThemeCard_, &ThemePreviewCard::clicked, this, [this]() { onThemeSelected(ThemePreviewCard::ThemeMode::Light); });
    connect(darkThemeCard_, &ThemePreviewCard::clicked, this, [this]() { onThemeSelected(ThemePreviewCard::ThemeMode::Dark); });

    cardRow->addWidget(systemThemeCard_);
    cardRow->addWidget(lightThemeCard_);
    cardRow->addWidget(darkThemeCard_);
    cardRow->addStretch();
    themeLayout->addLayout(cardRow);

    auto* themeSub = new QLabel(tr("Applies to Inline's own windows and panels, not to the whole Mac."), themeCard);
    themeSub->setStyleSheet("font-size: 11px; color: #6B7280;");
    themeLayout->addWidget(themeSub);
    layout->addWidget(themeCard);

    layout->addStretch();
    scroll->setWidget(content);
    return scroll;
}

void SettingsDialog::onThemeSelected(ThemePreviewCard::ThemeMode mode) {
    currentTheme_ = mode;
    systemThemeCard_->setSelected(mode == ThemePreviewCard::ThemeMode::System);
    lightThemeCard_->setSelected(mode == ThemePreviewCard::ThemeMode::Light);
    darkThemeCard_->setSelected(mode == ThemePreviewCard::ThemeMode::Dark);
    applyTheme(mode);
    saveToConfig();
}

// ============================================================================
// Page 3: Vocabulary & Slang
// ============================================================================
QWidget* SettingsDialog::createVocabularyPage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 28, 28, 28);
    layout->setSpacing(16);

    auto* header = new QLabel(tr("Custom Vocabulary"), content);
    header->setStyleSheet("font-size: 22px; font-weight: 700; color: #111827;");
    layout->addWidget(header);

    auto* card = createSettingCard();
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 16, 16, 16);
    cardLayout->setSpacing(10);

    auto* titleLabel = new QLabel(tr("Ignored Words & Technical Jargon"), card);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: 600; color: #111827;");
    cardLayout->addWidget(titleLabel);

    auto* subLabel = new QLabel(tr("Words entered here will never be touched, modified, or flagged by Inline. Separate words by commas or new lines."), card);
    subLabel->setStyleSheet("font-size: 11px; color: #6B7280;");
    cardLayout->addWidget(subLabel);

    ignoreWordsEdit_ = new QPlainTextEdit(card);
    ignoreWordsEdit_->setPlaceholderText(tr("Kubernetes, Jira, subagent, monorepo, SwiftUI"));
    ignoreWordsEdit_->setMinimumHeight(180);
    ignoreWordsEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(ignoreWordsEdit_, &QPlainTextEdit::textChanged, this, &SettingsDialog::saveToConfig);
    cardLayout->addWidget(ignoreWordsEdit_, 1);

    auto* infoBadge = new QLabel(tr("82,900+ English words and 180+ internet chat acronyms already loaded."), card);
    infoBadge->setStyleSheet("font-size: 11px; color: #059669; font-weight: 500;");
    cardLayout->addWidget(infoBadge);

    layout->addWidget(card, 1);
    scroll->setWidget(content);
    return scroll;
}

// ============================================================================
// Page 4: Excluded Applications
// ============================================================================
QWidget* SettingsDialog::createExcludedAppsPage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 28, 28, 28);
    layout->setSpacing(16);

    auto* header = new QLabel(tr("Excluded Apps"), content);
    header->setStyleSheet("font-size: 22px; font-weight: 700; color: #111827;");
    layout->addWidget(header);

    auto* card = createSettingCard();
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 16, 16, 16);
    cardLayout->setSpacing(10);

    auto* titleLabel = new QLabel(tr("Disabled Applications"), card);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: 600; color: #111827;");
    cardLayout->addWidget(titleLabel);

    auto* subLabel = new QLabel(tr("Inline automatically pauses autocorrection when typing in these applications (enter bundle identifiers or application names)."), card);
    subLabel->setStyleSheet("font-size: 11px; color: #6B7280;");
    cardLayout->addWidget(subLabel);

    disabledAppsEdit_ = new QPlainTextEdit(card);
    disabledAppsEdit_->setPlaceholderText(tr("com.apple.Terminal\ncom.googlecode.iterm2\ncom.microsoft.VSCode\ncom.apple.dt.Xcode"));
    disabledAppsEdit_->setMinimumHeight(180);
    disabledAppsEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(disabledAppsEdit_, &QPlainTextEdit::textChanged, this, &SettingsDialog::saveToConfig);
    cardLayout->addWidget(disabledAppsEdit_, 1);

    layout->addWidget(card, 1);
    scroll->setWidget(content);
    return scroll;
}

// ============================================================================
// Page 5: Playground
// ============================================================================
QWidget* SettingsDialog::createPlaygroundPage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 28, 28, 28);
    layout->setSpacing(16);

    auto* header = new QLabel(tr("Playground"), content);
    header->setStyleSheet("font-size: 22px; font-weight: 700; color: #111827;");
    layout->addWidget(header);

    auto* card1 = createSettingCard();
    auto* card1Layout = new QVBoxLayout(card1);
    card1Layout->setContentsMargins(16, 16, 16, 16);
    card1Layout->setSpacing(10);

    auto* testTitle = new QLabel(tr("Live Typing Sandbox"), card1);
    testTitle->setStyleSheet("font-size: 13px; font-weight: 600; color: #111827;");
    card1Layout->addWidget(testTitle);

    auto* testSub = new QLabel(tr("Test typing 'og', 'whenI', 'twowords', 'brb', or 'teh' here to verify real-time corrections."), card1);
    testSub->setStyleSheet("font-size: 11px; color: #6B7280;");
    card1Layout->addWidget(testSub);

    testInputEdit_ = new QLineEdit(card1);
    testInputEdit_->setPlaceholderText(tr("Type here to test live autocorrection..."));
    testInputEdit_->setStyleSheet("background-color: #F9FAFB; border: 1px solid #D1D5DB; border-radius: 8px; padding: 10px 12px; font-size: 14px;");
    card1Layout->addWidget(testInputEdit_);
    layout->addWidget(card1);

    auto* card2 = createSettingCard();
    card2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* card2Layout = new QVBoxLayout(card2);
    card2Layout->setContentsMargins(16, 16, 16, 16);
    card2Layout->setSpacing(10);

    auto* logHeaderLayout = new QHBoxLayout();
    auto* logTitle = new QLabel(tr("Live Correction Feed"), card2);
    logTitle->setStyleSheet("font-size: 13px; font-weight: 600; color: #111827;");
    logHeaderLayout->addWidget(logTitle);
    logHeaderLayout->addStretch();

    auto* clearBtn = new QPushButton(tr("Clear"), card2);
    clearBtn->setFixedWidth(64);
    clearBtn->setStyleSheet("background-color: #F3F4F6; border: 1px solid #E5E7EB; border-radius: 6px; padding: 4px 8px; font-size: 11px; font-weight: 500; color: #374151;");
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        if (liveLogEdit_) liveLogEdit_->clear();
    });
    logHeaderLayout->addWidget(clearBtn);
    card2Layout->addLayout(logHeaderLayout);

    liveLogEdit_ = new QPlainTextEdit(card2);
    liveLogEdit_->setReadOnly(true);
    liveLogEdit_->setMinimumHeight(180);
    liveLogEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    liveLogEdit_->setStyleSheet("background-color: #111827; color: #34D399; font-family: Menlo, Monaco, monospace; font-size: 11px; border-radius: 8px; padding: 8px;");
    card2Layout->addWidget(liveLogEdit_, 1);
    layout->addWidget(card2, 1);

    scroll->setWidget(content);
    return scroll;
}

// ============================================================================
// Page 6: Permissions & Health
// ============================================================================
QWidget* SettingsDialog::createPermissionsPage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 28, 28, 28);
    layout->setSpacing(16);

    auto* header = new QLabel(tr("Permissions & Health"), content);
    header->setStyleSheet("font-size: 22px; font-weight: 700; color: #111827;");
    layout->addWidget(header);

    auto* sec1 = new QLabel(tr("SYSTEM PERMISSIONS"), content);
    sec1->setStyleSheet("font-size: 11px; font-weight: 700; color: #6B7280; letter-spacing: 0.05em;");
    layout->addWidget(sec1);

    auto* card1 = createSettingCard();
    auto* card1Layout = new QVBoxLayout(card1);
    card1Layout->setContentsMargins(0, 0, 0, 0);
    card1Layout->setSpacing(0);

    auto* axRight = new QWidget();
    auto* axRightLayout = new QHBoxLayout(axRight);
    axRightLayout->setContentsMargins(0, 0, 0, 0);
    axRightLayout->setSpacing(8);
    axStatusBadge_ = new QLabel(tr("Checking..."), axRight);
    axActionBtn_ = new QPushButton(tr("Open Settings"), axRight);
    connect(axActionBtn_, &QPushButton::clicked, this, &SettingsDialog::onOpenAccessibilitySettings);
    axRightLayout->addWidget(axStatusBadge_);
    axRightLayout->addWidget(axActionBtn_);
    card1Layout->addWidget(createSettingRow(IconType::Accessibility, tr("Accessibility"), tr("Allows Inline to cleanly replace mistyped text inside active apps."), axRight, false));

    auto* imRight = new QWidget();
    auto* imRightLayout = new QHBoxLayout(imRight);
    imRightLayout->setContentsMargins(0, 0, 0, 0);
    imRightLayout->setSpacing(8);
    imStatusBadge_ = new QLabel(tr("Checking..."), imRight);
    imActionBtn_ = new QPushButton(tr("Open Settings"), imRight);
    connect(imActionBtn_, &QPushButton::clicked, this, &SettingsDialog::onOpenInputMonitoringSettings);
    imRightLayout->addWidget(imStatusBadge_);
    imRightLayout->addWidget(imActionBtn_);
    card1Layout->addWidget(createSettingRow(IconType::Keyboard, tr("Input Monitoring"), tr("Allows Inline to detect keystroke word boundaries locally on-device."), imRight, true));
    layout->addWidget(card1);

    auto* sec2 = new QLabel(tr("ENGINE STATUS"), content);
    sec2->setStyleSheet("font-size: 11px; font-weight: 700; color: #6B7280; letter-spacing: 0.05em;");
    layout->addWidget(sec2);

    auto* card2 = createSettingCard();
    auto* card2Layout = new QVBoxLayout(card2);
    card2Layout->setContentsMargins(0, 0, 0, 0);
    card2Layout->setSpacing(0);

    engineStatusBadge_ = new QLabel(tr("Active"), card2);
    engineStatusBadge_->setStyleSheet("background-color: #DCFCE7; color: #166534; font-size: 11px; font-weight: 700; border-radius: 6px; padding: 4px 8px;");
    card2Layout->addWidget(createSettingRow(IconType::Activity, tr("Background Event Hook"), tr("Native macOS CGEventTap pipeline state."), engineStatusBadge_, false));

    dictCountBadge_ = new QLabel(tr("82,946 terms"), card2);
    dictCountBadge_->setStyleSheet("background-color: #E0E7FF; color: #3730A3; font-size: 11px; font-weight: 700; border-radius: 6px; padding: 4px 8px;");
    card2Layout->addWidget(createSettingRow(IconType::Database, tr("Active Dictionary"), tr("Total indexed unigrams and contextual bigrams in memory."), dictCountBadge_, true));
    layout->addWidget(card2);

    layout->addStretch();
    scroll->setWidget(content);
    return scroll;
}

void SettingsDialog::onOpenAccessibilitySettings() {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    openAccessibilitySettings();
#endif
}

void SettingsDialog::onOpenInputMonitoringSettings() {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    openInputMonitoringSettings();
#endif
}

void SettingsDialog::refreshPermissions() {
#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    bool ax = checkAccessibilityPermission(false);
    bool im = checkInputMonitoringPermission();

    if (axStatusBadge_) {
        axStatusBadge_->setText(ax ? tr("Granted") : tr("Missing"));
        axStatusBadge_->setStyleSheet(ax
            ? "background-color: #DCFCE7; color: #166534; font-size: 11px; font-weight: 700; border-radius: 6px; padding: 4px 8px;"
            : "background-color: #FEE2E2; color: #991B1B; font-size: 11px; font-weight: 700; border-radius: 6px; padding: 4px 8px;");
    }
    if (axActionBtn_) axActionBtn_->setVisible(!ax);

    if (imStatusBadge_) {
        imStatusBadge_->setText(im ? tr("Granted") : tr("Missing"));
        imStatusBadge_->setStyleSheet(im
            ? "background-color: #DCFCE7; color: #166534; font-size: 11px; font-weight: 700; border-radius: 6px; padding: 4px 8px;"
            : "background-color: #FEE2E2; color: #991B1B; font-size: 11px; font-weight: 700; border-radius: 6px; padding: 4px 8px;");
    }
    if (imActionBtn_) imActionBtn_->setVisible(!im);
#endif
}

void SettingsDialog::openWizard() {
    auto* dlg = new PermissionDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void SettingsDialog::updateStatusInfo(bool hookRunning, size_t dictWords) {
    hookRunning_ = hookRunning;
    dictWords_ = dictWords;

    if (engineStatusBadge_) {
        engineStatusBadge_->setText(hookRunning_ ? tr("Running") : tr("Stopped"));
        engineStatusBadge_->setStyleSheet(hookRunning_
            ? "background-color: #DCFCE7; color: #166534; font-size: 11px; font-weight: 700; border-radius: 6px; padding: 4px 8px;"
            : "background-color: #FEE2E2; color: #991B1B; font-size: 11px; font-weight: 700; border-radius: 6px; padding: 4px 8px;");
    }
    if (dictCountBadge_) {
        dictCountBadge_->setText(QString("%1 words").arg(dictWords));
    }
}

void SettingsDialog::appendLog(const QString& msg) {
    if (liveLogEdit_) {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
        liveLogEdit_->appendPlainText(QString("[%1] %2").arg(timestamp, msg));
        liveLogEdit_->ensureCursorVisible();
    }
}

void SettingsDialog::loadFromConfig() {
    AppConfig cfg;
    std::string configPath = getConfigFilePath();
    std::ifstream file(configPath);
    if (file.is_open()) {
        std::string jsonStr((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        bool ok = false;
        cfg = deserializeConfig(jsonStr, ok);
    } else {
        std::lock_guard<std::mutex> lock(configMutex);
        cfg = sharedConfig;
    }

#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    cfg.launchAtLogin = isLaunchAtLoginEnabled();
#endif

    if (masterEnabledToggle_) masterEnabledToggle_->setChecked(cfg.masterEnabled);
    if (launchAtLoginToggle_) launchAtLoginToggle_->setChecked(cfg.launchAtLogin);
    if (singleKeyUndoToggle_) singleKeyUndoToggle_->setChecked(cfg.singleKeyUndoEnabled);
    if (sentenceCorrectionToggle_) sentenceCorrectionToggle_->setChecked(cfg.sentenceCorrectionEnabled);
    if (runTogetherToggle_) runTogetherToggle_->setChecked(cfg.splitRunTogetherWords);

    auto themeMode = (cfg.appTheme == 1) ? ThemePreviewCard::ThemeMode::Light
                   : (cfg.appTheme == 2) ? ThemePreviewCard::ThemeMode::Dark
                   : ThemePreviewCard::ThemeMode::System;
    currentTheme_ = themeMode;
    if (systemThemeCard_) systemThemeCard_->setSelected(themeMode == ThemePreviewCard::ThemeMode::System);
    if (lightThemeCard_) lightThemeCard_->setSelected(themeMode == ThemePreviewCard::ThemeMode::Light);
    if (darkThemeCard_) darkThemeCard_->setSelected(themeMode == ThemePreviewCard::ThemeMode::Dark);
    applyTheme(themeMode);

    if (sensitivityCombo_) {
        int idx = aggressivenessToSliderPosition(cfg.correctionAggressiveness);
        sensitivityCombo_->setCurrentIndex(idx < 33 ? 0 : (idx < 66 ? 1 : 2));
    }

    if (editDistanceCombo_) {
        editDistanceCombo_->setCurrentIndex(cfg.maxDictionaryEditDistance <= 1 ? 0 : 1);
    }

    if (ignoreWordsEdit_) {
        QString text;
        for (const auto& w : cfg.ignoreWords) {
            text += QString::fromStdString(w) + "\n";
        }
        ignoreWordsEdit_->setPlainText(text.trimmed());
    }

    if (disabledAppsEdit_) {
        QString text;
        for (const auto& app : cfg.disabledApps) {
            text += QString::fromStdString(app) + "\n";
        }
        disabledAppsEdit_->setPlainText(text.trimmed());
    }

    refreshPermissions();
}

void SettingsDialog::saveToConfig() {
    AppConfig cfg;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        cfg = sharedConfig;
    }

    if (masterEnabledToggle_) cfg.masterEnabled = masterEnabledToggle_->isChecked();
    if (launchAtLoginToggle_) cfg.launchAtLogin = launchAtLoginToggle_->isChecked();
    if (singleKeyUndoToggle_) cfg.singleKeyUndoEnabled = singleKeyUndoToggle_->isChecked();
    if (sentenceCorrectionToggle_) cfg.sentenceCorrectionEnabled = sentenceCorrectionToggle_->isChecked();
    if (runTogetherToggle_) cfg.splitRunTogetherWords = runTogetherToggle_->isChecked();
    cfg.appTheme = static_cast<int>(currentTheme_);

    if (sensitivityCombo_) {
        int idx = sensitivityCombo_->currentIndex();
        cfg.correctionAggressiveness = sliderPositionToAggressiveness(idx == 0 ? 20 : (idx == 1 ? 50 : 80));
    }

    if (editDistanceCombo_) {
        cfg.maxDictionaryEditDistance = (editDistanceCombo_->currentIndex() == 0) ? 1 : 2;
    }

    if (ignoreWordsEdit_) {
        cfg.ignoreWords.clear();
        QStringList lines = ignoreWordsEdit_->toPlainText().split(QRegularExpression("[,\n]"), Qt::SkipEmptyParts);
        for (const auto& l : lines) {
            QString trimmed = l.trimmed();
            if (!trimmed.isEmpty()) {
                cfg.ignoreWords.push_back(trimmed.toStdString());
            }
        }
    }

    if (disabledAppsEdit_) {
        cfg.disabledApps.clear();
        QStringList lines = disabledAppsEdit_->toPlainText().split('\n', Qt::SkipEmptyParts);
        for (const auto& l : lines) {
            QString trimmed = l.trimmed();
            if (!trimmed.isEmpty()) {
                cfg.disabledApps.push_back(trimmed.toStdString());
            }
        }
    }

#if defined(__APPLE__) && !defined(BUILD_WINDOWS_PLATFORM)
    setLaunchAtLogin(cfg.launchAtLogin);
#endif

    {
        std::lock_guard<std::mutex> lock(configMutex);
        sharedConfig = cfg;
    }

    std::string configPath = getConfigFilePath();
    std::ofstream file(configPath);
    if (file.is_open()) {
        file << serializeConfig(cfg);
    }
}
