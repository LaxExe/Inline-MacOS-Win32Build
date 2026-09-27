#pragma once

#include <optional>
#include <CoreGraphics/CoreGraphics.h>

struct HeuristicEstimate {
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
    bool confident = true;
};

struct OverlayDecision {
    bool visible = false;
    CGRect rect = {{0,0},{0,0}};
};

OverlayDecision decideOverlayPosition(std::optional<CGRect> axBounds, HeuristicEstimate heuristic, bool invalidatedSinceLastGood);
