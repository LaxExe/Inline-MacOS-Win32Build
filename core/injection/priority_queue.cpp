#include "core/injection/priority_queue.h"

void enqueueOp(std::deque<InjectionOp>& queue, InjectionOp op) {
    if (op.source == InjectionOp::GHOST_ACCEPT || op.source == InjectionOp::UNDO_CORRECTION) {
        queue.push_front(op);
    } else {
        queue.push_back(op);
    }
}
