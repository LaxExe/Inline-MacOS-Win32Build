#include "platform/mac/ghost/ghost_overlay.h"
#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>
#include <vector>

static NSFont* resolveHostFont(const std::string& fontName, double fontSize) {
    CGFloat sz = (fontSize >= 6.0 && fontSize <= 144.0) ? static_cast<CGFloat>(fontSize) : 13.0;
    if (!fontName.empty()) {
        NSString* nsName = [NSString stringWithUTF8String:fontName.c_str()];
        if (nsName && nsName.length > 0) {
            NSFont* f = [NSFont fontWithName:nsName size:sz];
            if (f) return f;
        }
    }
    return [NSFont systemFontOfSize:sz];
}

static NSString* wstringToNSString(const std::wstring& ws) {
    if (ws.empty()) return @"";
    std::vector<UniChar> chars;
    chars.reserve(ws.size());
    for (wchar_t wc : ws) {
        chars.push_back(static_cast<UniChar>(wc));
    }
    return [[NSString alloc] initWithCharacters:chars.data() length:chars.size()];
}

@interface GhostTextRenderView : NSView {
    GhostLayoutResult _layoutData;
}
@property (nonatomic, assign) BOOL darkAppearance;

- (void)setLayoutData:(const GhostLayoutResult&)layout;
- (const GhostLayoutResult&)layoutData;
@end

@implementation GhostTextRenderView

- (void)setLayoutData:(const GhostLayoutResult&)layout {
    _layoutData = layout;
}

- (const GhostLayoutResult&)layoutData {
    return _layoutData;
}

- (BOOL)isFlipped {
    return YES; // Top-left origin inside the view matches AX line run offsets
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    [[NSColor clearColor] setFill];
    NSRectFill(self.bounds);

    if (!_layoutData.valid || _layoutData.lines.empty()) return;

    NSFont* font = resolveHostFont(_layoutData.fontName, _layoutData.fontSize);

    CGFloat alpha = 0.65;
    if (_layoutData.styleMode == 0) alpha = 0.45;      // Subtle
    else if (_layoutData.styleMode == 2) alpha = 0.85; // High contrast

    uint32_t colorHex = (_layoutData.textColorRGBA != 0) ? _layoutData.textColorRGBA : 0x1F2937FF;
    CGFloat r = ((colorHex >> 24) & 0xFF) / 255.0;
    CGFloat g = ((colorHex >> 16) & 0xFF) / 255.0;
    CGFloat b = ((colorHex >> 8) & 0xFF) / 255.0;
    NSColor* textColor = [NSColor colorWithCalibratedRed:r green:g blue:b alpha:alpha];

    bool isDarkText = (r * 0.299 + g * 0.587 + b * 0.114) < 0.5;
    NSShadow* shadow = [[NSShadow alloc] init];
    [shadow setShadowBlurRadius:1.5];
    [shadow setShadowOffset:NSMakeSize(0.0, -0.5)];
    if (isDarkText) {
        [shadow setShadowColor:[NSColor colorWithCalibratedWhite:1.0 alpha:0.35]];
    } else {
        [shadow setShadowColor:[NSColor colorWithCalibratedWhite:0.0 alpha:0.45]];
    }

    NSDictionary* attrs = @{
        NSFontAttributeName: font,
        NSForegroundColorAttributeName: textColor,
        NSShadowAttributeName: shadow
    };

    const double baseX = _layoutData.boundingBox.x;
    const double baseY = _layoutData.boundingBox.y;

    for (const auto& run : _layoutData.lines) {
        if (run.text.empty()) continue;
        NSString* str = wstringToNSString(run.text);
        CGFloat relX = static_cast<CGFloat>(run.x - baseX);
        CGFloat relY = static_cast<CGFloat>(run.y - baseY);
        CGFloat fontAscent = font.ascender;
        CGFloat fontDescent = font.descender; // negative
        CGFloat fontHeight = fontAscent - fontDescent;
        CGFloat diff = static_cast<CGFloat>(run.height) - fontHeight;
        if (diff > 0.0) {
            relY += std::floor(diff * 0.5);
        } else if (diff < -1.0) {
            relY += std::ceil(diff * 0.5);
        }
        [str drawAtPoint:NSMakePoint(relX, relY) withAttributes:attrs];
    }
}

@end

static NSPanel* gGhostPanel = nil;
static GhostTextRenderView* gGhostView = nil;

static void ensurePanelOnMainThread() {
    if (gGhostPanel != nil) return;

    NSRect initialFrame = NSMakeRect(0, 0, 200, 24);
    gGhostPanel = [[NSPanel alloc] initWithContentRect:initialFrame
                                             styleMask:(NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel)
                                               backing:NSBackingStoreBuffered
                                                 defer:NO];
    [gGhostPanel setOpaque:NO];
    [gGhostPanel setBackgroundColor:[NSColor clearColor]];
    [gGhostPanel setHasShadow:NO];
    [gGhostPanel setIgnoresMouseEvents:YES];
    [gGhostPanel setFloatingPanel:YES];
    [gGhostPanel setHidesOnDeactivate:NO];
    [gGhostPanel setLevel:NSStatusWindowLevel];
    [gGhostPanel setCollectionBehavior:(NSWindowCollectionBehaviorCanJoinAllSpaces |
                                        NSWindowCollectionBehaviorFullScreenAuxiliary |
                                        NSWindowCollectionBehaviorStationary |
                                        NSWindowCollectionBehaviorIgnoresCycle)];
    [gGhostPanel setSharingType:NSWindowSharingNone];
    [gGhostPanel setAccessibilityElement:NO];

    gGhostView = [[GhostTextRenderView alloc] initWithFrame:initialFrame];
    [gGhostView setAccessibilityElement:NO];
    [gGhostPanel setContentView:gGhostView];
}

static std::atomic<uint64_t> gLayoutSequence{0};

GhostOverlay::GhostOverlay() = default;

GhostOverlay::~GhostOverlay() {
    hide();
}

void GhostOverlay::showLayout(const GhostLayoutResult& layout, uint64_t generation) {
    (void)generation;
    if (!layout.valid || layout.lines.empty()) {
        hide();
        return;
    }

    uint64_t seq = gLayoutSequence.fetch_add(1, std::memory_order_relaxed) + 1;
    GhostLayoutResult copy = layout;
    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            try {
                if (seq < gLayoutSequence.load(std::memory_order_relaxed)) {
                    return; // Dropped: superseded by newer layout or hide
                }

                ensurePanelOnMainThread();
                NSScreen* primaryScreen = [[NSScreen screens] firstObject];
                if (!primaryScreen) return;
                CGFloat primaryHeight = primaryScreen.frame.size.height;

                CGFloat w = std::max<CGFloat>(16.0, static_cast<CGFloat>(copy.boundingBox.width + 8.0));
                CGFloat h = std::max<CGFloat>(14.0, static_cast<CGFloat>(copy.boundingBox.height));
                CGFloat x = static_cast<CGFloat>(copy.boundingBox.x);
                CGFloat y = primaryHeight - static_cast<CGFloat>(copy.boundingBox.y) - h;

                // Reject invalid/offscreen coords
                if (x < -100.0 || y < -100.0 || y > primaryHeight + 100.0) {
                    return;
                }

                [gGhostView setLayoutData:copy];

                [NSAnimationContext beginGrouping];
                [[NSAnimationContext currentContext] setDuration:0.0];
                [gGhostPanel setFrame:NSMakeRect(x, y, w, h) display:YES];
                [gGhostView setFrame:NSMakeRect(0, 0, w, h)];
                [gGhostView setNeedsDisplay:YES];
                [gGhostPanel orderFrontRegardless];
                [NSAnimationContext endGrouping];
            } catch (const std::exception& e) {
                // Safeguard against any C++ exception
            } catch (...) {
            }
        }
    });
}

void GhostOverlay::hide() {
    gLayoutSequence.fetch_add(1, std::memory_order_relaxed);
    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            try {
                if (gGhostPanel && [gGhostPanel isVisible]) {
                    [gGhostPanel orderOut:nil];
                }
            } catch (...) {
            }
        }
    });
}

void GhostOverlay::advanceLayout(double deltaX, const std::wstring& remainingText) {
    if (deltaX <= 0.0 || remainingText.empty()) return;

    // Explicit deep value copy for asynchronous block capture
    std::wstring textCopy = remainingText;

    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            try {
                if (!gGhostPanel || ![gGhostPanel isVisible] || !gGhostView) return;

                GhostLayoutResult updated = [gGhostView layoutData];
                if (!updated.lines.empty()) {
                    updated.lines[0].text = textCopy;
                    updated.lines[0].x += deltaX;
                    updated.boundingBox.x += deltaX;
                    updated.boundingBox.width = std::max(4.0, updated.boundingBox.width - deltaX);
                    [gGhostView setLayoutData:updated];
                }

                NSRect curFrame = [gGhostPanel frame];
                curFrame.origin.x += static_cast<CGFloat>(deltaX);
                curFrame.size.width = std::max<CGFloat>(16.0, curFrame.size.width - static_cast<CGFloat>(deltaX));

                [NSAnimationContext beginGrouping];
                [[NSAnimationContext currentContext] setDuration:0.0];
                [gGhostPanel setFrame:curFrame display:YES];
                [gGhostView setFrame:NSMakeRect(0, 0, curFrame.size.width, curFrame.size.height)];
                [gGhostView setNeedsDisplay:YES];
                [gGhostView displayIfNeeded];
                [NSAnimationContext endGrouping];
            } catch (const std::exception& e) {
                // Safeguard against any C++ exception
            } catch (...) {
            }
        }
    });
}

double GhostOverlay::measureTextWidth(const std::wstring& text, const std::string& fontName, double fontSize) {
    if (text.empty()) return 0.0;
    @autoreleasepool {
        try {
            NSFont* font = resolveHostFont(fontName, fontSize);
            NSString* str = wstringToNSString(text);
            NSDictionary* attrs = @{ NSFontAttributeName: font };
            NSSize sz = [str sizeWithAttributes:attrs];
            return static_cast<double>(sz.width);
        } catch (...) {
            double sz = (fontSize >= 6.0 && fontSize <= 144.0) ? fontSize : 13.0;
            return static_cast<double>(text.size()) * sz * 0.55;
        }
    }
}
