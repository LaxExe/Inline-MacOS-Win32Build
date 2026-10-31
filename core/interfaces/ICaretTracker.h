#pragma once

#include <optional>
#include <string>
#include <cstdint>

struct CaretRect {
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
};

struct CaretGeometry {
    CaretRect caret;            // global top-left screen coordinates (AX space)
    CaretRect field;            // visible frame of the focused text container
    double lineStartX = 0;      // left X of the current visual line (for wrapped lines 2+)
    double lineHeight = 0;      // visual line height in points
    std::string fontName;       // host text font name if reported by AX
    double fontSize = 0;        // host text font size in pt if reported by AX
    uint32_t textColorRGBA = 0; // 0 if unknown
    bool isSingleLine = false;  // true for AXTextField / search bar / URL bar
    bool atEditableEnd = false; // true if char after caret is EOF or '\n'
    bool caretAtEOF = false;    // true if caret is at end of document
    std::wstring textTail;      // recent text before the caret (if reported by AX)
    int pid = 0;
};

class ICaretTracker {
public:
    virtual ~ICaretTracker() = default;
    virtual std::optional<CaretRect> getCaretScreenRect() = 0;
    virtual std::optional<CaretGeometry> getCaretGeometry() {
        auto r = getCaretScreenRect();
        if (!r) return std::nullopt;
        CaretGeometry g;
        g.caret = *r;
        g.field = {r->x, r->y, 600.0, 200.0};
        g.lineStartX = r->x;
        g.lineHeight = r->height > 0 ? r->height : 18.0;
        g.atEditableEnd = true;
        g.caretAtEOF = true;
        return g;
    }
};
