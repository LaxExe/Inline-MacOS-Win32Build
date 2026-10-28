#include "platform/win/hook/tap_recovery.h"

bool shouldReenableTap(CGEventType type) {
    return (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput);
}
