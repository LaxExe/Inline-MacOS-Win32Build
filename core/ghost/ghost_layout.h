#pragma once

#include <string>
#include <vector>
#include <functional>
#include "core/interfaces/ICaretTracker.h"

struct GhostLineRun {
    double x = 0;      // global AX X
    double y = 0;      // global AX Y (top-left of line)
    double width = 0;
    double height = 0;
    std::wstring text;
};

struct GhostLayoutResult {
    bool valid = false;
    std::wstring fittedText; // exact text visible across all lines (used on Tab accept)
    CaretRect boundingBox;   // union of all line runs in AX coordinates
    std::vector<GhostLineRun> lines;
    std::string fontName;
    double fontSize = 13.0;
    double lineHeight = 18.0;
    int styleMode = 1;       // 0 = Subtle, 1 = Standard, 2 = High contrast
};

using TextWidthMeasureFn = std::function<double(const std::wstring& text, const std::string& fontName, double fontSize)>;

GhostLayoutResult computeGhostLayout(
    const std::wstring& suggestion,
    const CaretGeometry& geom,
    bool isMidWord,
    const TextWidthMeasureFn& measureFn = nullptr,
    int styleMode = 1
);
