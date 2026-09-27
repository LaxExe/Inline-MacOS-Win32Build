#pragma once

#include <cstdint>

enum class ProcessState {
    Running,
    Crashed,
    Stopped
};

enum class SupervisorAction {
    None,
    KillAndRestart
};

using DurationMs = int64_t;

SupervisorAction decideSupervisorAction(ProcessState state, DurationMs elapsedSinceRequest, DurationMs hardCeiling);
