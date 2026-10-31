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

    // Exact CoreText glyph width measurement for computeGhostLayout
    static double measureTextWidth(const std::wstring& text, const std::string& fontName, double fontSize);
};
