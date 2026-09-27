#include "platform/mac/hook/tap_recovery.h"

bool shouldReenableTap(CGEventType type) {
    return (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput);
}
