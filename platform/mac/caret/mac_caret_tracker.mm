#include "platform/mac/caret/mac_caret_tracker.h"
#include "platform/mac/caret/secure_field_classifier.h"
#import <Cocoa/Cocoa.h>
#import <ApplicationServices/ApplicationServices.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_set>
#include <mutex>

MacCaretTracker::MacCaretTracker() = default;

void MacCaretTracker::enableChromiumAccessibilityIfNeeded(int pid) {
    if (pid <= 0) return;
    static int sLastEnabledPid = 0;
    if (sLastEnabledPid == pid) return;
    sLastEnabledPid = pid;

    AXUIElementRef appElem = AXUIElementCreateApplication(pid);
    if (!appElem) return;
    AXUIElementSetMessagingTimeout(appElem, 0.15f);
    AXUIElementSetAttributeValue(appElem, CFSTR("AXEnhancedUserInterface"), kCFBooleanTrue);
    AXUIElementSetAttributeValue(appElem, CFSTR("AXManualAccessibility"), kCFBooleanTrue);
    CFRelease(appElem);
}

static std::string cfStringToStdString(CFStringRef str) {
    if (!str) return "";
    CFIndex len = CFStringGetLength(str);
    if (len <= 0) return "";
    CFIndex maxBytes = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
    std::string buf(static_cast<size_t>(maxBytes), '\0');
    if (CFStringGetCString(str, buf.data(), maxBytes, kCFStringEncodingUTF8)) {
        buf.resize(std::strlen(buf.c_str()));
        return buf;
    }
    return "";
}

static std::wstring cfStringToWString(CFStringRef str) {
    if (!str) return L"";
    CFIndex len = CFStringGetLength(str);
    if (len <= 0) return L"";
    std::vector<UniChar> chars(static_cast<size_t>(len));
    CFStringGetCharacters(str, CFRangeMake(0, len), chars.data());
    std::wstring out;
    out.reserve(static_cast<size_t>(len));
    for (UniChar c : chars) {
        out.push_back(static_cast<wchar_t>(c));
    }
    return out;
}

static bool isZeroWidthOrFormatting(wchar_t c) {
    return c == 0x200B || c == 0xFEFF || c == 0x200C || c == 0x200D || c == 0x2060;
}

static bool isZeroWidthOrWhitespace(wchar_t c) {
    return c == L' ' || c == L'\t' || c == L'\n' || c == L'\r' ||
           c == 0x200B || c == 0xFEFF || c == 0x200C || c == 0x200D ||
           c == 0x00A0 || c == 0x2060;
}

static bool getAXRectForRange(AXUIElementRef elem, CFIndex loc, CFIndex len, CGRect& outRect) {
    CFRange r = CFRangeMake(loc, len);
    AXValueRef rangeVal = AXValueCreate(kAXValueTypeCFRange, &r);
    if (!rangeVal) return false;

    CFTypeRef boundsVal = nullptr;
    AXError err = AXUIElementCopyParameterizedAttributeValue(
        elem,
        kAXBoundsForRangeParameterizedAttribute,
        rangeVal,
        &boundsVal
    );
    CFRelease(rangeVal);
    if (err != kAXErrorSuccess || !boundsVal) return false;

    bool ok = false;
    if (CFGetTypeID(boundsVal) == AXValueGetTypeID()) {
        ok = AXValueGetValue(static_cast<AXValueRef>(boundsVal), kAXValueTypeCGRect, &outRect);
    }
    CFRelease(boundsVal);
    if (!ok) return false;
    if (std::isnan(outRect.origin.x) || std::isnan(outRect.origin.y) ||
        std::isinf(outRect.origin.x) || std::isinf(outRect.origin.y)) return false;

    // REJECT dummy/empty bounds returned by Chromium/Electron (e.g. (0.0, 1329.0, 0.0, 0.0))
    if (outRect.origin.x <= 1.0 && outRect.size.width <= 1.0) return false;
    if (outRect.size.width == 0.0 && outRect.size.height == 0.0) return false;

    if (len == 0 && outRect.size.height < 4.0) {
        outRect.size.height = 18.0;
    }
    return outRect.size.height >= 4.0 && outRect.size.height <= 240.0;
}

static bool getWebKitTextMarkerCaretRect(AXUIElementRef elem, CGRect& outRect) {
    if (!elem) return false;
    CFTypeRef markerRange = nullptr;
    AXError err = AXUIElementCopyAttributeValue(
        elem,
        CFSTR("AXSelectedTextMarkerRange"),
        &markerRange
    );
    if (err != kAXErrorSuccess || !markerRange) return false;

    CFTypeRef boundsVal = nullptr;
    err = AXUIElementCopyParameterizedAttributeValue(
        elem,
        CFSTR("AXBoundsForTextMarkerRange"),
        markerRange,
        &boundsVal
    );
    CFRelease(markerRange);
    if (err != kAXErrorSuccess || !boundsVal) return false;

    bool ok = false;
    if (CFGetTypeID(boundsVal) == AXValueGetTypeID()) {
        ok = AXValueGetValue(static_cast<AXValueRef>(boundsVal), kAXValueTypeCGRect, &outRect);
    }
    CFRelease(boundsVal);
    if (!ok) return false;
    if (std::isnan(outRect.origin.x) || std::isnan(outRect.origin.y) ||
        std::isinf(outRect.origin.x) || std::isinf(outRect.origin.y)) return false;

    // Reject dummy uninitialized bounds
    if (outRect.origin.x <= 1.0 && outRect.size.width <= 1.0) return false;

    if (outRect.size.height < 4.0) {
        outRect.size.height = 18.0;
    }
    return outRect.size.height >= 4.0 && outRect.size.height <= 240.0;
}

static bool getElementFrame(AXUIElementRef elem, CGRect& outFrame) {
    CFTypeRef posVal = nullptr;
    CFTypeRef sizeVal = nullptr;
    CGPoint pt = CGPointZero;
    CGSize sz = CGSizeZero;
    bool ok = false;
    if (AXUIElementCopyAttributeValue(elem, kAXPositionAttribute, &posVal) == kAXErrorSuccess && posVal) {
        if (CFGetTypeID(posVal) == AXValueGetTypeID()) {
            AXValueGetValue(static_cast<AXValueRef>(posVal), kAXValueTypeCGPoint, &pt);
        }
        CFRelease(posVal);
    } else {
        return false;
    }
    if (AXUIElementCopyAttributeValue(elem, kAXSizeAttribute, &sizeVal) == kAXErrorSuccess && sizeVal) {
        if (CFGetTypeID(sizeVal) == AXValueGetTypeID()) {
            ok = AXValueGetValue(static_cast<AXValueRef>(sizeVal), kAXValueTypeCGSize, &sz);
        }
        CFRelease(sizeVal);
    }
    if (!ok || sz.width <= 10.0 || sz.height <= 6.0) return false;
    outFrame = CGRectMake(pt.x, pt.y, sz.width, sz.height);
    return true;
}

std::optional<CaretRect> MacCaretTracker::getCaretScreenRect() {
    auto g = getCaretGeometry();
    if (!g) return std::nullopt;
    return g->caret;
}

std::optional<CaretGeometry> MacCaretTracker::getCaretGeometry() {
    @autoreleasepool {
        // 1. Get frontmost application and warm up accessibility immediately (essential for Chromium/Electron)
        NSRunningApplication* frontApp = [[NSWorkspace sharedWorkspace] frontmostApplication];
        pid_t frontPid = frontApp ? [frontApp processIdentifier] : 0;
        if (frontPid > 0) {
            enableChromiumAccessibilityIfNeeded(static_cast<int>(frontPid));
        }

        // 2. Query focused UI element: try application directly first (much faster in Chromium), then focused window, then system-wide
        AXUIElementRef elem = nullptr;
        if (frontPid > 0) {
            AXUIElementRef appElem = AXUIElementCreateApplication(frontPid);
            if (appElem) {
                AXUIElementSetMessagingTimeout(appElem, 0.08f);
                CFTypeRef appFocusVal = nullptr;
                if (AXUIElementCopyAttributeValue(appElem, kAXFocusedUIElementAttribute, &appFocusVal) == kAXErrorSuccess && appFocusVal) {
                    if (CFGetTypeID(appFocusVal) == AXUIElementGetTypeID()) {
                        elem = static_cast<AXUIElementRef>(appFocusVal);
                    } else {
                        CFRelease(appFocusVal);
                    }
                }
                // If app-level focus had no value (common in Electron before full tree binding), check focused window
                if (!elem) {
                    CFTypeRef focusedWinVal = nullptr;
                    if (AXUIElementCopyAttributeValue(appElem, kAXFocusedWindowAttribute, &focusedWinVal) == kAXErrorSuccess && focusedWinVal) {
                        if (CFGetTypeID(focusedWinVal) == AXUIElementGetTypeID()) {
                            AXUIElementRef winElem = static_cast<AXUIElementRef>(focusedWinVal);
                            CFTypeRef winFocusVal = nullptr;
                            if (AXUIElementCopyAttributeValue(winElem, kAXFocusedUIElementAttribute, &winFocusVal) == kAXErrorSuccess && winFocusVal) {
                                if (CFGetTypeID(winFocusVal) == AXUIElementGetTypeID()) {
                                    elem = static_cast<AXUIElementRef>(winFocusVal);
                                } else {
                                    CFRelease(winFocusVal);
                                }
                            }
                        }
                        CFRelease(focusedWinVal);
                    }
                }
                CFRelease(appElem);
            }
        }

        if (!elem) {
            AXUIElementRef sys = AXUIElementCreateSystemWide();
            if (!sys) return std::nullopt;
            AXUIElementSetMessagingTimeout(sys, 0.12f);
            CFTypeRef sysFocusVal = nullptr;
            AXError err = AXUIElementCopyAttributeValue(sys, kAXFocusedUIElementAttribute, &sysFocusVal);
            CFRelease(sys);
            if (err == kAXErrorSuccess && sysFocusVal) {
                if (CFGetTypeID(sysFocusVal) == AXUIElementGetTypeID()) {
                    elem = static_cast<AXUIElementRef>(sysFocusVal);
                } else {
                    CFRelease(sysFocusVal);
                }
            }
        }

        if (!elem) return std::nullopt;
        AXUIElementSetMessagingTimeout(elem, 0.08f);

        pid_t pid = 0;
        AXUIElementGetPid(elem, &pid);
        if (pid > 0 && pid != frontPid) {
            enableChromiumAccessibilityIfNeeded(static_cast<int>(pid));
        }

        // 3. Role & Subrole check (reject password/secure fields immediately)
        std::string roleStr, subroleStr;
        CFTypeRef roleVal = nullptr;
        if (AXUIElementCopyAttributeValue(elem, kAXRoleAttribute, &roleVal) == kAXErrorSuccess && roleVal) {
            if (CFGetTypeID(roleVal) == CFStringGetTypeID()) {
                roleStr = cfStringToStdString(static_cast<CFStringRef>(roleVal));
            }
            CFRelease(roleVal);
        }
        CFTypeRef subroleVal = nullptr;
        if (AXUIElementCopyAttributeValue(elem, kAXSubroleAttribute, &subroleVal) == kAXErrorSuccess && subroleVal) {
            if (CFGetTypeID(subroleVal) == CFStringGetTypeID()) {
                subroleStr = cfStringToStdString(static_cast<CFStringRef>(subroleVal));
            }
            CFRelease(subroleVal);
        }

        if (isSecureFieldRole(roleStr, subroleStr)) {
            CFRelease(elem);
            return std::nullopt;
        }

        // 4. Get text container frame (intersected with parent AXScrollArea if present in AppKit)
        CGRect fieldFrame = CGRectZero;
        bool hasFieldFrame = getElementFrame(elem, fieldFrame);
        if (hasFieldFrame) {
            CFTypeRef parentVal = nullptr;
            if (AXUIElementCopyAttributeValue(elem, kAXParentAttribute, &parentVal) == kAXErrorSuccess && parentVal) {
                if (CFGetTypeID(parentVal) == AXUIElementGetTypeID()) {
                    AXUIElementRef parentElem = static_cast<AXUIElementRef>(parentVal);
                    CFTypeRef parentRoleVal = nullptr;
                    if (AXUIElementCopyAttributeValue(parentElem, kAXRoleAttribute, &parentRoleVal) == kAXErrorSuccess && parentRoleVal) {
                        std::string pRole = (CFGetTypeID(parentRoleVal) == CFStringGetTypeID())
                            ? cfStringToStdString(static_cast<CFStringRef>(parentRoleVal)) : "";
                        CFRelease(parentRoleVal);
                        if (pRole == "AXScrollArea") {
                            CGRect parentFrame = CGRectZero;
                            if (getElementFrame(parentElem, parentFrame)) {
                                CGRect inter = CGRectIntersection(fieldFrame, parentFrame);
                                if (!CGRectIsNull(inter) && inter.size.width > 20.0 && inter.size.height > 8.0) {
                                    fieldFrame = inter;
                                }
                            }
                        }
                    }
                }
                CFRelease(parentVal);
            }
        }

        // 5. Query selected text range
        CFTypeRef selRangeVal = nullptr;
        CFRange selRange = CFRangeMake(0, 0);
        bool hasSelRange = false;
        if (AXUIElementCopyAttributeValue(elem, kAXSelectedTextRangeAttribute, &selRangeVal) == kAXErrorSuccess && selRangeVal) {
            if (CFGetTypeID(selRangeVal) == AXValueGetTypeID()) {
                hasSelRange = AXValueGetValue(static_cast<AXValueRef>(selRangeVal), kAXValueTypeCFRange, &selRange);
            }
            CFRelease(selRangeVal);
        }

        // If user has highlighted a span of text (selection > 0), do not show ghost predictions
        if (hasSelRange && selRange.length > 0) {
            CFRelease(elem);
            return std::nullopt;
        }

        CaretGeometry geom;
        geom.pid = static_cast<int>(pid);
        geom.isSingleLine = (roleStr == "AXTextField" || roleStr == "AXComboBox" || subroleStr == "AXSearchField");
        geom.atEditableEnd = true;
        geom.caretAtEOF = true;

        // 6. Query recent text tail and font attributes
        if (hasSelRange) {
            CFIndex tailStart = std::max<CFIndex>(0, selRange.location - 160);
            CFIndex tailLen = selRange.location - tailStart;
            if (tailLen > 0) {
                CFRange tailR = CFRangeMake(tailStart, tailLen);
                AXValueRef tailVal = AXValueCreate(kAXValueTypeCFRange, &tailR);
                if (tailVal) {
                    CFTypeRef strRef = nullptr;
                    if (AXUIElementCopyParameterizedAttributeValue(elem, kAXStringForRangeParameterizedAttribute, tailVal, &strRef) == kAXErrorSuccess && strRef) {
                        if (CFGetTypeID(strRef) == CFStringGetTypeID()) {
                            geom.textTail = cfStringToWString(static_cast<CFStringRef>(strRef));
                        }
                        CFRelease(strRef);
                    }
                    CFRelease(tailVal);
                }
            }

            // Fallback for Chromium / Electron web inputs where kAXStringForRange is not supported:
            if (geom.textTail.empty()) {
                CFTypeRef fullVal = nullptr;
                if (AXUIElementCopyAttributeValue(elem, kAXValueAttribute, &fullVal) == kAXErrorSuccess && fullVal) {
                    if (CFGetTypeID(fullVal) == CFStringGetTypeID()) {
                        std::wstring fullStr = cfStringToWString(static_cast<CFStringRef>(fullVal));
                        if (selRange.location <= static_cast<CFIndex>(fullStr.size())) {
                            size_t start = (selRange.location > 160) ? (selRange.location - 160) : 0;
                            geom.textTail = fullStr.substr(start, selRange.location - start);

                            // Strip invisible zero-width formatting characters commonly inserted by Slate.js / Lexical / Electron
                            while (!geom.textTail.empty() && isZeroWidthOrFormatting(geom.textTail.back())) {
                                geom.textTail.pop_back();
                            }

                            bool onlyTrailingFormatting = true;
                            for (size_t i = selRange.location; i < fullStr.size(); ++i) {
                                if (!isZeroWidthOrWhitespace(fullStr[i])) {
                                    onlyTrailingFormatting = false;
                                    break;
                                }
                            }
                            if (onlyTrailingFormatting) {
                                geom.atEditableEnd = true;
                                geom.caretAtEOF = true;
                            } else {
                                geom.atEditableEnd = false;
                                geom.caretAtEOF = false;
                            }
                        }
                    }
                    CFRelease(fullVal);
                }
            }

            if (selRange.location > 0) {
                CFRange attrR = CFRangeMake(selRange.location - 1, 1);
                AXValueRef attrRangeVal = AXValueCreate(kAXValueTypeCFRange, &attrR);
                if (attrRangeVal) {
                    CFTypeRef attrStrRef = nullptr;
                    if (AXUIElementCopyParameterizedAttributeValue(elem, kAXAttributedStringForRangeParameterizedAttribute, attrRangeVal, &attrStrRef) == kAXErrorSuccess && attrStrRef) {
                        if (CFGetTypeID(attrStrRef) == CFAttributedStringGetTypeID()) {
                            CFAttributedStringRef attrStr = static_cast<CFAttributedStringRef>(attrStrRef);
                            if (CFAttributedStringGetLength(attrStr) > 0) {
                                CFDictionaryRef fontDict = static_cast<CFDictionaryRef>(
                                    CFAttributedStringGetAttribute(attrStr, 0, kAXFontTextAttribute, nullptr)
                                );
                                if (fontDict && CFGetTypeID(fontDict) == CFDictionaryGetTypeID()) {
                                    CFStringRef fname = static_cast<CFStringRef>(CFDictionaryGetValue(fontDict, kAXFontNameKey));
                                    if (fname && CFGetTypeID(fname) == CFStringGetTypeID()) {
                                        geom.fontName = cfStringToStdString(fname);
                                    }
                                    CFNumberRef fsize = static_cast<CFNumberRef>(CFDictionaryGetValue(fontDict, kAXFontSizeKey));
                                    if (fsize && CFGetTypeID(fsize) == CFNumberGetTypeID()) {
                                        double sz = 0.0;
                                        if (CFNumberGetValue(fsize, kCFNumberDoubleType, &sz) && sz >= 6.0 && sz <= 144.0) {
                                            geom.fontSize = sz;
                                        }
                                    }
                                }
                            }
                        }
                        CFRelease(attrStrRef);
                    }
                    CFRelease(attrRangeVal);
                }
            }
        }

        // 7. Caret coordinate extraction:
        CGRect caretRect = CGRectZero;
        bool gotCaret = false;
        wchar_t lastChar = geom.textTail.empty() ? L'\0' : geom.textTail.back();

        // 7a. Try WebKit / Blink TextMarker first (Chromium, Electron, Discord, Slack, Chrome, Vivaldi, VS Code, Safari)
        CGRect wkRect = CGRectZero;
        if (getWebKitTextMarkerCaretRect(elem, wkRect)) {
            double caretX = (wkRect.size.width <= 4.0) ? wkRect.origin.x : (wkRect.origin.x + wkRect.size.width);
            double caretH = (wkRect.size.height >= 8.0 && wkRect.size.height <= 200.0) ? wkRect.size.height : 18.0;
            caretRect = CGRectMake(caretX, wkRect.origin.y, 2.5, caretH);
            gotCaret = true;
        }

        // Try WebKit TextMarker on parent web area if child element holds marker on parent
        if (!gotCaret) {
            CFTypeRef parentVal = nullptr;
            if (AXUIElementCopyAttributeValue(elem, kAXParentAttribute, &parentVal) == kAXErrorSuccess && parentVal) {
                if (CFGetTypeID(parentVal) == AXUIElementGetTypeID()) {
                    if (getWebKitTextMarkerCaretRect(static_cast<AXUIElementRef>(parentVal), wkRect)) {
                        double caretX = (wkRect.size.width <= 4.0) ? wkRect.origin.x : (wkRect.origin.x + wkRect.size.width);
                        double caretH = (wkRect.size.height >= 8.0 && wkRect.size.height <= 200.0) ? wkRect.size.height : 18.0;
                        caretRect = CGRectMake(caretX, wkRect.origin.y, 2.5, caretH);
                        gotCaret = true;
                    }
                }
                CFRelease(parentVal);
            }
        }

        // 7b. Native AppKit Range Ladder (Notes, TextEdit, Pages, Messages, Mail)
        auto tryRangeLadder = [&]() -> bool {
            if (!hasSelRange) return false;

            // Insertion point rect: exact cursor line in NSTextView
            CGRect zeroRect = CGRectZero;
            bool hasZero = getAXRectForRange(elem, selRange.location, 0, zeroRect) &&
                           zeroRect.origin.x > 1.0 && zeroRect.size.width <= 14.0 &&
                           zeroRect.size.height >= 8.0;

            if (hasZero) {
                caretRect = CGRectMake(
                    zeroRect.origin.x,
                    zeroRect.origin.y,
                    2.5,
                    zeroRect.size.height
                );
                return true;
            }

            if (selRange.location > 0 && lastChar != L'\n' && lastChar != L'\r') {
                CGRect prevCharRect = CGRectZero;
                bool hasPrev = getAXRectForRange(elem, selRange.location - 1, 1, prevCharRect) &&
                               prevCharRect.origin.x > 1.0 &&
                               prevCharRect.size.width > 0.0 &&
                               prevCharRect.size.width <= 160.0;
                if (hasPrev) {
                    double rightX = prevCharRect.origin.x + prevCharRect.size.width;
                    double topY = prevCharRect.origin.y;
                    double h = std::clamp((double)prevCharRect.size.height, 14.0, 200.0);

                    if (lastChar == L' ' && prevCharRect.size.width < 2.5) {
                        double spaceW = std::max(4.0, (geom.fontSize > 0.0 ? geom.fontSize : 13.0) * 0.32);
                        rightX += spaceW;
                    }

                    caretRect = CGRectMake(rightX, topY, 2.5, h);
                    return true;
                }
            }

            CGRect nextCharRect = CGRectZero;
            if (getAXRectForRange(elem, selRange.location, 1, nextCharRect) &&
                nextCharRect.origin.x > 1.0 && nextCharRect.size.width <= 160.0) {
                caretRect = CGRectMake(
                    nextCharRect.origin.x,
                    nextCharRect.origin.y,
                    2.5,
                    std::clamp((double)nextCharRect.size.height, 14.0, 200.0)
                );
                return true;
            }

            return false;
        };

        if (!gotCaret) {
            gotCaret = tryRangeLadder();
        }

        // 7c. Fallback for empty text field (e.g. search fields, single-line inputs)
        if (!gotCaret && hasSelRange && selRange.location == 0 && hasFieldFrame && fieldFrame.size.height <= 80.0) {
            double h = std::clamp((double)fieldFrame.size.height - 8.0, 14.0, 24.0);
            caretRect = CGRectMake(fieldFrame.origin.x + 6.0, fieldFrame.origin.y + (fieldFrame.size.height - h) * 0.5, 2.5, h);
            gotCaret = true;
        }

        if (!gotCaret || caretRect.origin.x <= 1.0 || caretRect.origin.y <= 1.0 ||
            caretRect.size.height < 6.0 || caretRect.size.height > 200.0) {
            CFRelease(elem);
            return std::nullopt;
        }

        geom.caret = {caretRect.origin.x, caretRect.origin.y, std::max(2.0, (double)caretRect.size.width), caretRect.size.height};

        // If fieldFrame was reported and contains the caret, use it. Otherwise, synthesize field bounds anchored at caretRect
        // so window-local coordinate offsets in Chromium/Electron web views never cause caret dismissal.
        if (hasFieldFrame) {
            CGRect expanded = CGRectInset(fieldFrame, -32.0, -32.0);
            if (CGRectIntersectsRect(expanded, caretRect)) {
                geom.field = {fieldFrame.origin.x, fieldFrame.origin.y, fieldFrame.size.width, fieldFrame.size.height};
            } else {
                geom.field = {caretRect.origin.x - 20.0, caretRect.origin.y, std::max(520.0, (double)fieldFrame.size.width), std::max(200.0, (double)fieldFrame.size.height)};
            }
        } else {
            geom.field = {caretRect.origin.x - 20.0, caretRect.origin.y, 520.0, 200.0};
        }
        geom.lineStartX = geom.field.x + 6.0;
        geom.lineHeight = geom.caret.height;

        if (hasSelRange) {
            CFIndex probeIdx = (selRange.location > 0) ? (selRange.location - 1) : 0;
            CFNumberRef idxNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &probeIdx);
            if (idxNum) {
                CFTypeRef lineNumVal = nullptr;
                if (AXUIElementCopyParameterizedAttributeValue(elem, kAXLineForIndexParameterizedAttribute, idxNum, &lineNumVal) == kAXErrorSuccess && lineNumVal) {
                    CFTypeRef lineRangeVal = nullptr;
                    if (AXUIElementCopyParameterizedAttributeValue(elem, kAXRangeForLineParameterizedAttribute, lineNumVal, &lineRangeVal) == kAXErrorSuccess && lineRangeVal) {
                        CFRange lineRange = CFRangeMake(0, 0);
                        if (CFGetTypeID(lineRangeVal) == AXValueGetTypeID() &&
                            AXValueGetValue(static_cast<AXValueRef>(lineRangeVal), kAXValueTypeCFRange, &lineRange)) {
                            CGRect firstCharRect = CGRectZero;
                            if (getAXRectForRange(elem, lineRange.location, 1, firstCharRect) && firstCharRect.origin.x > 0.0) {
                                geom.lineStartX = firstCharRect.origin.x;
                            }
                        }
                        CFRelease(lineRangeVal);
                    }
                    CFRelease(lineNumVal);
                }
                CFRelease(idxNum);
            }

            CFRange afterR = CFRangeMake(selRange.location, 1);
            AXValueRef afterVal = AXValueCreate(kAXValueTypeCFRange, &afterR);
            if (afterVal) {
                CFTypeRef afterStrRef = nullptr;
                if (AXUIElementCopyParameterizedAttributeValue(elem, kAXStringForRangeParameterizedAttribute, afterVal, &afterStrRef) == kAXErrorSuccess && afterStrRef) {
                    if (CFGetTypeID(afterStrRef) == CFStringGetTypeID()) {
                        std::wstring afterStr = cfStringToWString(static_cast<CFStringRef>(afterStrRef));
                        if (!afterStr.empty()) {
                            bool onlyFormatting = true;
                            for (wchar_t ch : afterStr) {
                                if (!isZeroWidthOrWhitespace(ch)) {
                                    onlyFormatting = false;
                                    break;
                                }
                            }
                            if (!onlyFormatting) {
                                geom.atEditableEnd = false;
                                geom.caretAtEOF = false;
                            }
                        }
                    }
                    CFRelease(afterStrRef);
                }
                CFRelease(afterVal);
            }
        }

        CFRelease(elem);
        return geom;
    }
}
