#include "platform/mac/caret/mac_caret_tracker.h"
#include "platform/mac/caret/secure_field_classifier.h"
#include "platform/mac/ghost/ghost_overlay.h"
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
    if (!elem) return false;
    pid_t elemPid = 0;
    AXUIElementGetPid(elem, &elemPid);
    if (elemPid <= 0 || elemPid == getpid()) return false;

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
    pid_t elemPid = 0;
    AXUIElementGetPid(elem, &elemPid);
    if (elemPid <= 0 || elemPid == getpid()) return false;

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

    // Reject dummy uninitialized bounds (e.g. Chrome returning web area origin with w=0, h=0)
    if (outRect.origin.x <= 1.0 && outRect.size.width <= 1.0) return false;
    if (outRect.size.width == 0.0 && outRect.size.height == 0.0) return false;

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
        AXUIElementRef elem = nullptr;
        try {
            // 1. Get frontmost application and warm up accessibility immediately (essential for Chromium/Electron)
            NSRunningApplication* frontApp = [[NSWorkspace sharedWorkspace] frontmostApplication];
            pid_t frontPid = frontApp ? [frontApp processIdentifier] : 0;
            // Never track or query accessibility on Inline itself — querying in-process Qt widgets across threads causes fatal crash
            if (frontPid <= 0 || frontPid == getpid()) {
                return std::nullopt;
            }
            enableChromiumAccessibilityIfNeeded(static_cast<int>(frontPid));

            // 2. Query focused UI element: try application directly first (much faster in Chromium), then focused window, then system-wide
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
        if (pid <= 0 || pid == getpid()) {
            CFRelease(elem);
            return std::nullopt;
        }
        if (pid != frontPid) {
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

        auto isCaretInsideField = [&](const CGRect& r) -> bool {
            if (r.origin.x <= 1.0 || r.origin.y <= 1.0) return false;
            if (r.size.height < 6.0 || r.size.height > 240.0) return false;
            if (hasFieldFrame) {
                CGRect expanded = CGRectInset(fieldFrame, -24.0, -24.0);
                if (!CGRectContainsPoint(expanded, CGPointMake(r.origin.x, r.origin.y))) {
                    return false;
                }
            }
            return true;
        };

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

        // Validate range bounds to prevent negative or corrupt values from custom app AX handlers
        if (!hasSelRange || selRange.location < 0 || selRange.length < 0) {
            hasSelRange = false;
            selRange = CFRangeMake(0, 0);
        }

        // If user has highlighted a span of text (selection > 0), do not show ghost predictions
        if (hasSelRange && selRange.length > 0) {
            CFRelease(elem);
            return std::nullopt;
        }

        // 5b. Query full element value upfront (Chromium, Electron, web search fields, Safari, inputs)
        CFTypeRef fullVal = nullptr;
        std::wstring fullStr;
        if (AXUIElementCopyAttributeValue(elem, kAXValueAttribute, &fullVal) == kAXErrorSuccess && fullVal) {
            if (CFGetTypeID(fullVal) == CFStringGetTypeID()) {
                fullStr = cfStringToWString(static_cast<CFStringRef>(fullVal));
            }
            CFRelease(fullVal);
        }

        // Determine effective caret location. In Chromium / Blink web inputs (Google search, YouTube),
        // selRange is frequently delayed or unpopulated (loc = 0, len = 0) while typing at the end of the input.
        // If loc == 0 but fullStr has text, probe whether fullStr.size() provides the true caret position:
        CFIndex effectiveLoc = selRange.location;
        if (hasSelRange && effectiveLoc == 0 && fullStr.size() > 0) {
            CGRect testEndRect = CGRectZero;
            CFIndex endIdx = static_cast<CFIndex>(fullStr.size());
            bool validEnd = (getAXRectForRange(elem, endIdx, 0, testEndRect) ||
                             getAXRectForRange(elem, endIdx - 1, 1, testEndRect)) &&
                            isCaretInsideField(testEndRect);
            if (validEnd) {
                effectiveLoc = endIdx;
            }
        } else if (!hasSelRange && fullStr.size() > 0) {
            CGRect testEndRect = CGRectZero;
            CFIndex endIdx = static_cast<CFIndex>(fullStr.size());
            bool validEnd = (getAXRectForRange(elem, endIdx, 0, testEndRect) ||
                             getAXRectForRange(elem, endIdx - 1, 1, testEndRect)) &&
                            isCaretInsideField(testEndRect);
            if (validEnd) {
                effectiveLoc = endIdx;
                hasSelRange = true;
            }
        }

        CaretGeometry geom;
        geom.pid = static_cast<int>(pid);
        std::string lowerSub = subroleStr;
        for (char& sc : lowerSub) sc = static_cast<char>(std::tolower(sc));
        geom.isSingleLine = (roleStr == "AXTextField" || roleStr == "AXComboBox" ||
                             subroleStr == "AXSearchField" ||
                             lowerSub.find("search") != std::string::npos ||
                             lowerSub.find("url") != std::string::npos ||
                             (hasFieldFrame && fieldFrame.size.height <= 72.0));
        geom.atEditableEnd = true;
        geom.caretAtEOF = true;

        // 6. Query recent text tail and font attributes
        if (!fullStr.empty()) {
            if (effectiveLoc <= static_cast<CFIndex>(fullStr.size())) {
                size_t l = static_cast<size_t>(effectiveLoc);
                size_t start = (l > 160) ? (l - 160) : 0;
                geom.textTail = fullStr.substr(start, l - start);
            } else {
                size_t start = (fullStr.size() > 160) ? (fullStr.size() - 160) : 0;
                geom.textTail = fullStr.substr(start);
            }
            while (!geom.textTail.empty() && isZeroWidthOrFormatting(geom.textTail.back())) {
                geom.textTail.pop_back();
            }

            // Check if non-whitespace text follows the caret
            if (effectiveLoc < static_cast<CFIndex>(fullStr.size())) {
                for (size_t i = static_cast<size_t>(effectiveLoc); i < fullStr.size(); ++i) {
                    if (!isZeroWidthOrWhitespace(fullStr[i])) {
                        geom.atEditableEnd = false;
                        geom.caretAtEOF = false;
                        break;
                    }
                }
            }
        }

        if (geom.textTail.empty() && hasSelRange && effectiveLoc > 0) {
            CFIndex tailStart = std::max<CFIndex>(0, effectiveLoc - 160);
            CFIndex tailLen = effectiveLoc - tailStart;
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
        }

        // 6a. Check document character count and probe text following caret to verify atEditableEnd (for AppKit NSTextView)
        CFTypeRef numCharsVal = nullptr;
        CFIndex totalDocChars = -1;
        if (AXUIElementCopyAttributeValue(elem, kAXNumberOfCharactersAttribute, &numCharsVal) == kAXErrorSuccess && numCharsVal) {
            if (CFGetTypeID(numCharsVal) == CFNumberGetTypeID()) {
                CFNumberGetValue(static_cast<CFNumberRef>(numCharsVal), kCFNumberCFIndexType, &totalDocChars);
            }
            CFRelease(numCharsVal);
        }

        if (fullStr.empty() && totalDocChars > 0 && effectiveLoc < totalDocChars) {
            CFIndex probeLen = std::min<CFIndex>(120, totalDocChars - effectiveLoc);
            CFRange afterR = CFRangeMake(effectiveLoc, probeLen);
            AXValueRef afterVal = AXValueCreate(kAXValueTypeCFRange, &afterR);
            if (afterVal) {
                CFTypeRef afterStrRef = nullptr;
                if (AXUIElementCopyParameterizedAttributeValue(elem, kAXStringForRangeParameterizedAttribute, afterVal, &afterStrRef) == kAXErrorSuccess && afterStrRef) {
                    if (CFGetTypeID(afterStrRef) == CFStringGetTypeID()) {
                        std::wstring afterStr = cfStringToWString(static_cast<CFStringRef>(afterStrRef));
                        for (wchar_t ch : afterStr) {
                            if (!isZeroWidthOrWhitespace(ch)) {
                                geom.atEditableEnd = false;
                                geom.caretAtEOF = false;
                                break;
                            }
                        }
                    }
                    CFRelease(afterStrRef);
                }
                CFRelease(afterVal);
            }
        } else if (fullStr.empty() && totalDocChars == -1 && effectiveLoc > 0) {
            // If total character count is not reported by the host app, probe up to 80 chars after caret
            CFRange afterR = CFRangeMake(effectiveLoc, 80);
            AXValueRef afterVal = AXValueCreate(kAXValueTypeCFRange, &afterR);
            if (afterVal) {
                CFTypeRef afterStrRef = nullptr;
                if (AXUIElementCopyParameterizedAttributeValue(elem, kAXStringForRangeParameterizedAttribute, afterVal, &afterStrRef) == kAXErrorSuccess && afterStrRef) {
                    if (CFGetTypeID(afterStrRef) == CFStringGetTypeID()) {
                        std::wstring afterStr = cfStringToWString(static_cast<CFStringRef>(afterStrRef));
                        for (wchar_t ch : afterStr) {
                            if (!isZeroWidthOrWhitespace(ch)) {
                                geom.atEditableEnd = false;
                                geom.caretAtEOF = false;
                                break;
                            }
                        }
                    }
                    CFRelease(afterStrRef);
                }
                CFRelease(afterVal);
            }
        }

        // 6c. Query font attributes safely
        if (effectiveLoc > 0 || totalDocChars > 0 || !fullStr.empty()) {
            CFIndex fontProbeIdx = (effectiveLoc > 0) ? (effectiveLoc - 1) : 0;
            CFRange attrR = CFRangeMake(fontProbeIdx, 1);
            AXValueRef attrRangeVal = AXValueCreate(kAXValueTypeCFRange, &attrR);
            if (attrRangeVal) {
                CFTypeRef attrStrRef = nullptr;
                if (AXUIElementCopyParameterizedAttributeValue(elem, kAXAttributedStringForRangeParameterizedAttribute, attrRangeVal, &attrStrRef) == kAXErrorSuccess && attrStrRef) {
                    if (CFGetTypeID(attrStrRef) == CFAttributedStringGetTypeID()) {
                        CFAttributedStringRef attrStr = static_cast<CFAttributedStringRef>(attrStrRef);
                        if (CFAttributedStringGetLength(attrStr) > 0) {
                            // Try AppKit NSFontAttributeName directly
                            CFTypeRef fontObj = CFAttributedStringGetAttribute(attrStr, 0, (CFStringRef)@"NSFont", nullptr);
                            if (fontObj && [(__bridge id)fontObj isKindOfClass:[NSFont class]]) {
                                NSFont* nsFont = (__bridge NSFont*)fontObj;
                                geom.fontSize = [nsFont pointSize];
                                if (nsFont.fontName && nsFont.fontName.length > 0) {
                                    geom.fontName = [nsFont.fontName UTF8String];
                                }
                            } else {
                                // Fallback to WebKit / AXFont dictionary
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

                            // Extract text foreground color if specified (NSColor or CGColorRef in Chrome)
                            CFTypeRef colorObj = CFAttributedStringGetAttribute(attrStr, 0, (CFStringRef)@"NSColor", nullptr);
                            if (!colorObj) {
                                colorObj = CFAttributedStringGetAttribute(attrStr, 0, (CFStringRef)@"NSForegroundColor", nullptr);
                            }
                            if (!colorObj) {
                                colorObj = CFAttributedStringGetAttribute(attrStr, 0, (CFStringRef)@"AXForegroundColor", nullptr);
                            }
                            if (colorObj) {
                                if ([(__bridge id)colorObj isKindOfClass:[NSColor class]]) {
                                    NSColor* col = [(__bridge NSColor*)colorObj colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
                                    if (col) {
                                        uint32_t r = static_cast<uint32_t>(std::clamp([col redComponent] * 255.0, 0.0, 255.0));
                                        uint32_t g = static_cast<uint32_t>(std::clamp([col greenComponent] * 255.0, 0.0, 255.0));
                                        uint32_t b = static_cast<uint32_t>(std::clamp([col blueComponent] * 255.0, 0.0, 255.0));
                                        geom.textColorRGBA = (r << 24) | (g << 16) | (b << 8) | 0xFF;
                                    }
                                } else if (CFGetTypeID(colorObj) == CGColorGetTypeID()) {
                                    CGColorRef cgCol = (CGColorRef)colorObj;
                                    const CGFloat* comps = CGColorGetComponents(cgCol);
                                    size_t numComps = CGColorGetNumberOfComponents(cgCol);
                                    if (comps && numComps >= 3) {
                                        uint32_t r = static_cast<uint32_t>(std::clamp(comps[0] * 255.0, 0.0, 255.0));
                                        uint32_t g = static_cast<uint32_t>(std::clamp(comps[1] * 255.0, 0.0, 255.0));
                                        uint32_t b = static_cast<uint32_t>(std::clamp(comps[2] * 255.0, 0.0, 255.0));
                                        geom.textColorRGBA = (r << 24) | (g << 16) | (b << 8) | 0xFF;
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

        // 7. Caret coordinate extraction:
        CGRect caretRect = CGRectZero;
        bool gotCaret = false;
        wchar_t lastChar = geom.textTail.empty() ? L'\0' : geom.textTail.back();

        // 7a. Native AppKit & Chromium Range Ladder (Notes, TextEdit, Pages, Chrome web fields, Amazon, Google search, YouTube)
        auto tryRangeLadder = [&]() -> bool {
            if (!hasSelRange) return false;

            CGRect prevCharRect = CGRectZero;
            bool hasPrev = false;
            if (effectiveLoc > 0 && lastChar != L'\n' && lastChar != L'\r') {
                hasPrev = getAXRectForRange(elem, effectiveLoc - 1, 1, prevCharRect) &&
                          prevCharRect.origin.x > 1.0 &&
                          prevCharRect.size.width > 0.0 &&
                          prevCharRect.size.width <= 160.0 &&
                          isCaretInsideField(prevCharRect);
            }

            // Insertion point rect: exact cursor line in NSTextView and Chromium text inputs
            CGRect zeroRect = CGRectZero;
            bool hasZero = getAXRectForRange(elem, effectiveLoc, 0, zeroRect) &&
                           zeroRect.origin.x > 1.0 && zeroRect.size.width <= 14.0 &&
                           zeroRect.size.height >= 8.0 &&
                           isCaretInsideField(zeroRect);

            if (hasZero && hasPrev) {
                double prevRight = prevCharRect.origin.x + prevCharRect.size.width;
                if (lastChar == L' ' && prevCharRect.size.width < 2.5) {
                    double spaceW = std::max(4.0, (geom.fontSize > 0.0 ? geom.fontSize : 13.0) * 0.32);
                    prevRight = prevCharRect.origin.x + spaceW;
                }
                // Caret X must never lag behind the preceding glyph's right edge during fast typing
                double finalX = std::max((double)zeroRect.origin.x, prevRight);
                // In AppKit and Blink, prevCharRect is the exact glyph typed on the active line.
                // Always lock baseline Y and line height to prevCharRect so ghost text never floats above the typed line!
                double finalY = prevCharRect.origin.y;
                double finalH = prevCharRect.size.height;
                caretRect = CGRectMake(finalX, finalY, 2.0, finalH);
                return true;
            }

            if (hasZero) {
                caretRect = CGRectMake(zeroRect.origin.x, zeroRect.origin.y, 2.0, zeroRect.size.height);
                return true;
            }

            if (hasPrev) {
                double rightX = prevCharRect.origin.x + prevCharRect.size.width;
                double topY = prevCharRect.origin.y;
                double h = std::clamp((double)prevCharRect.size.height, 14.0, 200.0);
                if (lastChar == L' ' && prevCharRect.size.width < 2.5) {
                    double spaceW = std::max(4.0, (geom.fontSize > 0.0 ? geom.fontSize : 13.0) * 0.32);
                    rightX += spaceW;
                }
                caretRect = CGRectMake(rightX, topY, 2.0, h);
                return true;
            }

            CGRect nextCharRect = CGRectZero;
            if (getAXRectForRange(elem, effectiveLoc, 1, nextCharRect) &&
                nextCharRect.origin.x > 1.0 && nextCharRect.size.width <= 160.0 &&
                isCaretInsideField(nextCharRect)) {
                caretRect = CGRectMake(
                    nextCharRect.origin.x,
                    nextCharRect.origin.y,
                    2.0,
                    std::clamp((double)nextCharRect.size.height, 14.0, 200.0)
                );
                return true;
            }

            return false;
        };

        if (hasSelRange) {
            gotCaret = tryRangeLadder();
        }

        // 7b. Try WebKit / Blink TextMarker (Chromium, Electron, Discord, Slack, VS Code, Safari)
        if (!gotCaret) {
            CGRect wkRect = CGRectZero;
            if (getWebKitTextMarkerCaretRect(elem, wkRect) && isCaretInsideField(wkRect)) {
                if (wkRect.size.width <= 24.0) {
                    double caretX = (wkRect.size.width <= 4.0) ? wkRect.origin.x : (wkRect.origin.x + wkRect.size.width);
                    double caretH = (wkRect.size.height >= 8.0 && wkRect.size.height <= 200.0) ? wkRect.size.height : 18.0;
                    caretRect = CGRectMake(caretX, wkRect.origin.y, 2.0, caretH);
                    gotCaret = true;
                }
            }
        }

        // Try WebKit TextMarker on ancestors (Chromium inputs can be nested inside AXGroup elements or forms)
        if (!gotCaret) {
            AXUIElementRef curAncestor = elem;
            CFRetain(curAncestor);
            for (int depth = 0; depth < 8 && !gotCaret; ++depth) {
                CFTypeRef parentVal = nullptr;
                if (AXUIElementCopyAttributeValue(curAncestor, kAXParentAttribute, &parentVal) == kAXErrorSuccess && parentVal) {
                    if (CFGetTypeID(parentVal) == AXUIElementGetTypeID()) {
                        AXUIElementRef parentElem = static_cast<AXUIElementRef>(parentVal);
                        CGRect wkRect = CGRectZero;
                        if (getWebKitTextMarkerCaretRect(parentElem, wkRect) && isCaretInsideField(wkRect)) {
                            if (wkRect.size.width <= 24.0) {
                                double caretX = (wkRect.size.width <= 4.0) ? wkRect.origin.x : (wkRect.origin.x + wkRect.size.width);
                                double caretH = (wkRect.size.height >= 8.0 && wkRect.size.height <= 200.0) ? wkRect.size.height : 18.0;
                                caretRect = CGRectMake(caretX, wkRect.origin.y, 2.0, caretH);
                                gotCaret = true;
                            }
                        }
                        CFRelease(curAncestor);
                        curAncestor = parentElem; // retain transferred
                    } else {
                        CFRelease(parentVal);
                        break;
                    }
                } else {
                    break;
                }
            }
            CFRelease(curAncestor);
        }

        // 7c. Fallback for text fields and search bars (when AX range and marker queries are unsupported)
        if (!gotCaret && hasFieldFrame && fieldFrame.size.height <= 80.0) {
            double h = std::clamp((double)fieldFrame.size.height - 8.0, 14.0, 24.0);
            // In web search bars, vertical center of text line is ~2.5pt higher than fieldFrame physical center
            double y = fieldFrame.origin.y + (fieldFrame.size.height - h) * 0.5 - 2.5;
            double effectiveFontSize = 14.0;
            if (geom.fontSize >= 10.0 && geom.fontSize <= 36.0) {
                effectiveFontSize = geom.fontSize;
            } else if (fieldFrame.size.height <= 32.0) {
                effectiveFontSize = 13.0;
            } else if (fieldFrame.size.height >= 48.0) {
                effectiveFontSize = 16.0;
            }
            geom.fontSize = effectiveFontSize;
            double textW = 0.0;
            if (!geom.textTail.empty()) {
                textW = GhostOverlay::measureTextWidth(geom.textTail, geom.fontName, effectiveFontSize);
            }
            bool isWideSearch = (geom.isSingleLine && (subroleStr == "AXSearchField" || lowerSub.find("search") != std::string::npos) && fieldFrame.size.width >= 400.0);
            double padL = isWideSearch ? 36.0 : (geom.isSingleLine ? 10.0 : 8.0);
            double x = std::min(fieldFrame.origin.x + fieldFrame.size.width - 12.0, fieldFrame.origin.x + padL + textW);
            caretRect = CGRectMake(x, y, 2.5, h);
            gotCaret = true;
        }

        // Align single-line inputs & search bars to visual vertical line center if host app hugged container top edge
        if (gotCaret && geom.isSingleLine && hasFieldFrame && fieldFrame.size.height <= 72.0) {
            double fieldCenterY = fieldFrame.origin.y + (fieldFrame.size.height - caretRect.size.height) * 0.5 - 1.5;
            if (caretRect.origin.y < fieldCenterY - 6.0) {
                caretRect.origin.y = fieldCenterY;
            }
            if (fieldFrame.size.height >= 36.0 && caretRect.size.height < 16.0) {
                caretRect.size.height = 18.0;
            }
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
            CFIndex probeIdx = (effectiveLoc > 0) ? (effectiveLoc - 1) : 0;
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
        }

        if (geom.textColorRGBA == 0 && frontApp) {
            NSString* bid = [frontApp bundleIdentifier];
            if (bid) {
                std::string lowerBid = [bid UTF8String];
                for (char& c : lowerBid) c = static_cast<char>(std::tolower(c));
                if (lowerBid.find("terminal") != std::string::npos ||
                    lowerBid.find("iterm") != std::string::npos ||
                    lowerBid.find("warp") != std::string::npos ||
                    lowerBid.find("kitty") != std::string::npos ||
                    lowerBid.find("alacritty") != std::string::npos ||
                    lowerBid.find("vscode") != std::string::npos ||
                    lowerBid.find("cursor") != std::string::npos) {
                    geom.textColorRGBA = 0xE0E0E0FF; // Crisp light grey for dark terminals and IDEs
                } else {
                    geom.textColorRGBA = 0x1F2937FF; // Dark charcoal (#1F2937) for web browsers, Google search, Notes, TextEdit
                }
            } else {
                geom.textColorRGBA = 0x1F2937FF;
            }
        }

        if (elem) CFRelease(elem);
        return geom;
    } catch (...) {
        if (elem) CFRelease(elem);
        return std::nullopt;
    }
    }
}
