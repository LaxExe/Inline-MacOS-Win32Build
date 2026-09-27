#include "core/injection/token_validation.h"

bool shouldApplyOp(const InjectionOp& op, int currentTokenForSource) {
    return op.generation_token == currentTokenForSource;
}
