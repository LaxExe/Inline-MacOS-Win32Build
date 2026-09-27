#pragma once

#include "core/injection/injection_op.h"
#include <deque>

void enqueueOp(std::deque<InjectionOp>& queue, InjectionOp op);
