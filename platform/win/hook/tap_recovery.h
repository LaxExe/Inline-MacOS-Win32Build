#pragma once

#include <cstdint>

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#else
typedef uint32_t CGEventType;
#ifndef kCGEventTapDisabledByTimeout
#define kCGEventTapDisabledByTimeout 14
#endif
#ifndef kCGEventTapDisabledByUserInput
#define kCGEventTapDisabledByUserInput 15
#endif
#ifndef kCGEventKeyDown
#define kCGEventKeyDown 10
#endif
#endif

bool shouldReenableTap(CGEventType type);
