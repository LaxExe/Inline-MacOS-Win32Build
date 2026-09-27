#include "platform/mac/caret/overlay_position.h"

OverlayDecision decideOverlayPosition(std::optional<CGRect> axBounds, HeuristicEstimate heuristic, bool invalidatedSinceLastGood) {
    OverlayDecision decision;
    if (axBounds.has_value()) {
        decision.visible = true;
        decision.rect = *axBounds;
        return decision;
    }

    if (invalidatedSinceLastGood || !heuristic.confident) {
        decision.visible = false;
        return decision;
    }

    decision.visible = true;
    decision.rect = CGRectMake(heuristic.x, heuristic.y, heuristic.width, heuristic.height);
    return decision;
}
