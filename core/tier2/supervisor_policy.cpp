#include "core/tier2/supervisor_policy.h"

SupervisorAction decideSupervisorAction(ProcessState state, DurationMs elapsedSinceRequest, DurationMs hardCeiling) {
    if (state == ProcessState::Crashed) {
        return SupervisorAction::KillAndRestart;
    }
    if (state == ProcessState::Running && elapsedSinceRequest > hardCeiling) {
        return SupervisorAction::KillAndRestart;
    }
    return SupervisorAction::None;
}
