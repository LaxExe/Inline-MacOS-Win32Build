#include "platform/mac/overlay/ghost_overlay_window.h"
#import <Cocoa/Cocoa.h>
#include <algorithm>

@interface GhostInlineTextView : NSView
@property (nonatomic, copy) NSString* text;
@end

@implementation GhostInlineTextView
- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        _text = @"";
    }
    return self;
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    NSRect bounds = self.bounds;
    if (bounds.size.width <= 0 || bounds.size.height <= 0 || self.text.length == 0) return;

    // Pure inline ghost text: NO pill, NO border, NO background.
    // Subtle semi-transparent grey text matching modern inline autocomplete.
    NSDictionary* attrs = @{
        NSFontAttributeName: [NSFont systemFontOfSize:13 weight:NSFontWeightRegular],
        NSForegroundColorAttributeName: [NSColor colorWithCalibratedRed:0.55 green:0.58 blue:0.65 alpha:0.75]
    };

    NSSize textSize = [self.text sizeWithAttributes:attrs];
    CGFloat yOffset = (bounds.size.height - textSize.height) / 2.0;
    NSRect textRect = NSMakeRect(0, yOffset, bounds.size.width, textSize.height);
    [self.text drawInRect:textRect withAttributes:attrs];
}
@end

GhostOverlayWindow::GhostOverlayWindow() {
    auto initBlock = ^{
        NSRect frame = NSMakeRect(0, 0, 200, 22);
        NSPanel* window = [[NSPanel alloc] initWithContentRect:frame
                                                     styleMask:NSWindowStyleMaskNonactivatingPanel | NSWindowStyleMaskBorderless
                                                       backing:NSBackingStoreBuffered
                                                         defer:NO];
        window.opaque = NO;
        window.backgroundColor = [NSColor clearColor];
        window.ignoresMouseEvents = YES;
        window.level = NSFloatingWindowLevel;
        window.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorFullScreenAuxiliary;
        window.hasShadow = NO;
        window.releasedWhenClosed = NO;
        [window setHidesOnDeactivate:NO];
        [window setCanHide:NO];

        GhostInlineTextView* textView = [[GhostInlineTextView alloc] initWithFrame:frame];
        [window.contentView addSubview:textView];

        this->nativeWindow_ = (__bridge_retained void*)window;
        this->pillView_ = (__bridge_retained void*)textView;
    };

    if ([NSThread isMainThread]) {
        initBlock();
    } else {
        dispatch_sync(dispatch_get_main_queue(), initBlock);
    }
}

GhostOverlayWindow::~GhostOverlayWindow() {
    auto destroyBlock = ^{
        if (this->nativeWindow_) {
            NSWindow* window = (__bridge_transfer NSWindow*)this->nativeWindow_;
            [window close];
            this->nativeWindow_ = nullptr;
        }
        if (this->pillView_) {
            GhostInlineTextView* textView = (__bridge_transfer GhostInlineTextView*)this->pillView_;
            (void)textView;
            this->pillView_ = nullptr;
        }
    };

    if ([NSThread isMainThread]) {
        destroyBlock();
    } else {
        dispatch_async(dispatch_get_main_queue(), destroyBlock);
    }
}

void GhostOverlayWindow::showGhostText(const std::wstring& text, double screenX, double screenY, double height) {
    if (text.empty()) return;

    std::string utf8;
    for (wchar_t wc : text) {
        if (wc < 128) {
            utf8.push_back(static_cast<char>(wc));
        }
    }
    if (utf8.empty()) return;
    NSString* nsText = [NSString stringWithUTF8String:utf8.c_str()];
    if (!nsText) return;

    dispatch_async(dispatch_get_main_queue(), ^{
        if (!this->nativeWindow_ || !this->pillView_) return;
        NSWindow* window = (__bridge NSWindow*)this->nativeWindow_;
        GhostInlineTextView* textView = (__bridge GhostInlineTextView*)this->pillView_;

        textView.text = nsText;

        NSDictionary* attrs = @{
            NSFontAttributeName: [NSFont systemFontOfSize:13 weight:NSFontWeightRegular]
        };
        NSSize textSize = [nsText sizeWithAttributes:attrs];
        CGFloat width = textSize.width + 4.0;
        CGFloat viewHeight = (height > 10.0) ? height : std::max<CGFloat>(textSize.height, 20.0);

        textView.frame = NSMakeRect(0, 0, width, viewHeight);

        NSScreen* primaryScreen = [NSScreen screens].firstObject;
        CGFloat screenHeight = primaryScreen ? primaryScreen.frame.size.height : 1080.0;
        CGFloat screenWidth = primaryScreen ? primaryScreen.frame.size.width : 1920.0;
        CGFloat caretHeight = (height > 0) ? height : 18.0;
        CGFloat cocoaY = screenHeight - screenY - caretHeight;

        CGFloat safeX = std::max<CGFloat>(0.0, std::min<CGFloat>(screenX + 3.0, screenWidth - width));
        CGFloat safeY = std::max<CGFloat>(0.0, std::min<CGFloat>(cocoaY, screenHeight - viewHeight));

        NSRect newFrame = NSMakeRect(safeX, safeY, width, viewHeight);

        [window setFrame:newFrame display:YES];
        [textView setNeedsDisplay:YES];
        [window orderFrontRegardless];
    });
}

void GhostOverlayWindow::hide() {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!this->nativeWindow_) return;
        NSWindow* window = (__bridge NSWindow*)this->nativeWindow_;
        [window orderOut:nil];
    });
}

bool GhostOverlayWindow::isVisible() const {
    if (!nativeWindow_) return false;
    NSWindow* window = (__bridge NSWindow*)nativeWindow_;
    return [window isVisible];
}
