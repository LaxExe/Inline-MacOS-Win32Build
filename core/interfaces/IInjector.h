#pragma once

#include "core/injection/injection_op.h"

class IInjector {
public:
    virtual ~IInjector() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void enqueue(const InjectionOp& op) = 0;
    virtual void setValidToken(InjectionOp::Source source, int token) = 0;
};
