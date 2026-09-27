#include "platform/mac/caret/mac_caret_tracker.h"
#include "platform/mac/caret/secure_field_classifier.h"
#import <AppKit/AppKit.h>
#include <string>

static std::string cfStringToStdString(CFStringRef cfStr) {
    if (!cfStr) return "";
    char buf[256];
    if (CFStringGetCString(cfStr, buf, sizeof(buf), kCFStringEncodingUTF8)) {
        return std::string(buf);
    }
    return "";
}

MacCaretTracker::MacCaretTracker() {
    systemWide_ = AXUIElementCreateSystemWide();
}

MacCaretTracker::~MacCaretTracker() {
    if (systemWide_) {
        CFRelease(systemWide_);
        systemWide_ = nullptr;
    }
}

bool MacCaretTracker::isFocusedFieldSecure() {
    if (!systemWide_) return false;

    AXUIElementRef focusedElement = nullptr;
    AXError err = AXUIElementCopyAttributeValue(systemWide_, kAXFocusedUIElementAttribute, (CFTypeRef*)&focusedElement);
    if (err != kAXErrorSuccess || !focusedElement) {
        return false;
    }

    CFStringRef role = nullptr;
    AXUIElementCopyAttributeValue(focusedElement, kAXRoleAttribute, (CFTypeRef*)&role);

    CFStringRef subrole = nullptr;
    AXUIElementCopyAttributeValue(focusedElement, kAXSubroleAttribute, (CFTypeRef*)&subrole);

    std::string roleStr = cfStringToStdString(role);
    std::string subroleStr = cfStringToStdString(subrole);

    if (role) CFRelease(role);
    if (subrole) CFRelease(subrole);
    CFRelease(focusedElement);

    return isSecureFieldRole(roleStr, subroleStr);
}

std::optional<CaretRect> MacCaretTracker::getCaretScreenRect() {
    if (systemWide_) {
        AXUIElementRef focusedElement = nullptr;
        AXError err = AXUIElementCopyAttributeValue(systemWide_, kAXFocusedUIElementAttribute, (CFTypeRef*)&focusedElement);
        if (err == kAXErrorSuccess && focusedElement) {
            // 1. Primary path: Caret range bounds
            CFTypeRef rangeValue = nullptr;
            err = AXUIElementCopyAttributeValue(focusedElement, kAXSelectedTextRangeAttribute, &rangeValue);
            if (err == kAXErrorSuccess && rangeValue) {
                CFTypeRef boundsValue = nullptr;
                err = AXUIElementCopyParameterizedAttributeValue(
                    focusedElement, kAXBoundsForRangeParameterizedAttribute, rangeValue, &boundsValue);
                CFRelease(rangeValue);

                if (err == kAXErrorSuccess && boundsValue) {
                    CGRect bounds;
                    bool gotBounds = AXValueGetValue((AXValueRef)boundsValue, kAXValueTypeCGRect, &bounds);
                    CFRelease(boundsValue);
                    if (gotBounds && (bounds.origin.x > 0 || bounds.origin.y > 0 || bounds.size.height > 0)) {
                        CFRelease(focusedElement);
                        return CaretRect{
                            .x = bounds.origin.x,
                            .y = bounds.origin.y,
                            .width = (bounds.size.width > 0 && bounds.size.width <= 4) ? bounds.size.width : 1.0,
                            .height = (bounds.size.height >= 10.0 && bounds.size.height <= 80.0) ? bounds.size.height : 18.0
                        };
                    }
                }
            }

            // 2. Secondary path: Element position + size
            CFTypeRef posValue = nullptr;
            CFTypeRef sizeValue = nullptr;
            AXError posErr = AXUIElementCopyAttributeValue(focusedElement, kAXPositionAttribute, &posValue);
            AXError sizeErr = AXUIElementCopyAttributeValue(focusedElement, kAXSizeAttribute, &sizeValue);
            CFRelease(focusedElement);

            if (posErr == kAXErrorSuccess && posValue && sizeErr == kAXErrorSuccess && sizeValue) {
                CGPoint pt;
                CGSize sz;
                bool gotPt = AXValueGetValue((AXValueRef)posValue, kAXValueTypeCGPoint, &pt);
                bool gotSz = AXValueGetValue((AXValueRef)sizeValue, kAXValueTypeCGSize, &sz);
                if (posValue) CFRelease(posValue);
                if (sizeValue) CFRelease(sizeValue);
                if (gotPt && gotSz && (pt.x > 0 || pt.y > 0)) {
                    return CaretRect{
                        .x = pt.x + 4,
                        .y = pt.y + std::max<CGFloat>(0.0, (sz.height - 18.0) / 2.0),
                        .width = 1,
                        .height = (sz.height >= 10.0 && sz.height <= 80.0) ? sz.height : 18.0
                    };
                }
            } else {
                if (posValue) CFRelease(posValue);
                if (sizeValue) CFRelease(sizeValue);
            }
        }
    }

    // 3. Fallback path: Mouse cursor location
    __block CaretRect fallbackRect{100.0, 100.0, 1, 18};
    auto mouseBlock = ^{
        NSPoint mouse = [NSEvent mouseLocation];
        NSScreen* primaryScreen = [NSScreen screens].firstObject;
        CGFloat screenHeight = primaryScreen ? primaryScreen.frame.size.height : 1080.0;
        fallbackRect = CaretRect{
            .x = mouse.x + 2.0,
            .y = screenHeight - mouse.y - 18.0,
            .width = 1,
            .height = 18
        };
    };

    if ([NSThread isMainThread]) {
        mouseBlock();
    } else {
        dispatch_sync(dispatch_get_main_queue(), mouseBlock);
    }
    return fallbackRect;
}
