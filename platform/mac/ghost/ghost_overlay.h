#pragma once

#include "core/ghost/ghost_layout.h"
#include <string>

class GhostOverlay {
public:
    GhostOverlay();
    ~GhostOverlay();

    // Renders the wrapped layout on the main thread
    void showLayout(const GhostLayoutResult& layout);

    // Immediately hides the overlay panel
    void hide();

    // Fast-path predictive horizontal advancement for type-through keystrokes.
    // Shifts the overlay right by deltaX and replaces line 0 text with remainingText.
    void advanceLayout(double deltaX, const std::wstring& remainingText);

    // Exact CoreText glyph width measurement for computeGhostLayout
    static double measureTextWidth(const std::wstring& text, const std::string& fontName, double fontSize);
};
