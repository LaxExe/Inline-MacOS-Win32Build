#import <Cocoa/Cocoa.h>
#include <iostream>
#include <filesystem>

void drawAppIcon(CGContextRef ctx, CGFloat size) {
    CGFloat s = size;
    CGRect rect = CGRectMake(0, 0, s, s);

    // 1. Background Rounded Squircle with Vibrant Gradient
    CGFloat cornerRadius = s * 0.2237;
    CGPathRef clipPath = CGPathCreateWithRoundedRect(CGRectInset(rect, s * 0.05, s * 0.05), cornerRadius, cornerRadius, NULL);
    CGContextAddPath(ctx, clipPath);
    CGContextClip(ctx);

    // Gradient Background (Deep Indigo to Electric Cyan/Violet)
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGFloat components[8] = {
        0.12f, 0.15f, 0.35f, 1.0f, // Deep Midnight Indigo
        0.28f, 0.45f, 0.95f, 1.0f  // Electric Royal Blue / Cyan
    };
    CGFloat locations[2] = {0.0f, 1.0f};
    CGGradientRef grad = CGGradientCreateWithColorComponents(colorSpace, components, locations, 2);
    CGContextDrawLinearGradient(ctx, grad, CGPointMake(0, s), CGPointMake(s, 0), 0);
    CGGradientRelease(grad);

    // 2. Glow effect behind the central symbol
    CGContextSetRGBFillColor(ctx, 1.0f, 1.0f, 1.0f, 0.15f);
    CGContextFillEllipseInRect(ctx, CGRectInset(rect, s * 0.2, s * 0.2));

    // 3. Draw Typing Keyboard / Keypad Plate
    CGRect keyPlate = CGRectMake(s * 0.22, s * 0.28, s * 0.56, s * 0.44);
    CGFloat plateRadius = s * 0.08;
    CGPathRef platePath = CGPathCreateWithRoundedRect(keyPlate, plateRadius, plateRadius, NULL);
    CGContextSetRGBFillColor(ctx, 0.08f, 0.10f, 0.22f, 0.75f);
    CGContextAddPath(ctx, platePath);
    CGContextFillPath(ctx);

    // Border around plate
    CGContextSetRGBStrokeColor(ctx, 0.45f, 0.75f, 1.0f, 0.6f);
    CGContextSetLineWidth(ctx, s * 0.02);
    CGContextAddPath(ctx, platePath);
    CGContextStrokePath(ctx);
    CGPathRelease(platePath);

    // 4. Draw Keyboard Key Caps
    CGFloat keyW = s * 0.10;
    CGFloat keyH = s * 0.08;
    CGFloat keyGap = s * 0.03;
    CGFloat startX = s * 0.27;
    CGFloat startY = s * 0.56;

    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 4; ++col) {
            CGRect keyRect = CGRectMake(startX + col * (keyW + keyGap), startY - row * (keyH + keyGap), keyW, keyH);
            CGPathRef kPath = CGPathCreateWithRoundedRect(keyRect, s * 0.02, s * 0.02, NULL);
            // Highlighted active space/key
            if (row == 0 && col == 1) {
                CGContextSetRGBFillColor(ctx, 0.35f, 0.85f, 0.65f, 0.95f); // Emerald active key
            } else if (row == 1 && col == 2) {
                CGContextSetRGBFillColor(ctx, 0.45f, 0.70f, 1.0f, 0.95f);  // Cyan active key
            } else {
                CGContextSetRGBFillColor(ctx, 0.20f, 0.28f, 0.50f, 0.85f);
            }
            CGContextAddPath(ctx, kPath);
            CGContextFillPath(ctx);
            CGPathRelease(kPath);
        }
    }

    // Spacebar
    CGRect spaceRect = CGRectMake(s * 0.33, s * 0.33, s * 0.34, keyH);
    CGPathRef spacePath = CGPathCreateWithRoundedRect(spaceRect, s * 0.02, s * 0.02, NULL);
    CGContextSetRGBFillColor(ctx, 0.30f, 0.45f, 0.80f, 0.85f);
    CGContextAddPath(ctx, spacePath);
    CGContextFillPath(ctx);
    CGPathRelease(spacePath);

    // 5. Lightning / Fast Cursor Accent in Top Right
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx, s * 0.68, s * 0.85);
    CGContextAddLineToPoint(ctx, s * 0.58, s * 0.68);
    CGContextAddLineToPoint(ctx, s * 0.68, s * 0.68);
    CGContextAddLineToPoint(ctx, s * 0.60, s * 0.48);
    CGContextAddLineToPoint(ctx, s * 0.78, s * 0.68);
    CGContextAddLineToPoint(ctx, s * 0.68, s * 0.68);
    CGContextClosePath(ctx);
    CGContextSetRGBFillColor(ctx, 1.0f, 0.88f, 0.25f, 0.95f); // Amber Gold
    CGContextFillPath(ctx);

    CGColorSpaceRelease(colorSpace);
    CGPathRelease(clipPath);
}

void exportIconPNG(const std::string& path, int size) {
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(NULL, size, size, 8, size * 4, colorSpace, kCGImageAlphaPremultipliedLast);

    drawAppIcon(ctx, size);

    CGImageRef image = CGBitmapContextCreateImage(ctx);
    NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL((__bridge CFURLRef)url, kUTTypePNG, 1, NULL);
    CGImageDestinationAddImage(dest, image, NULL);
    CGImageDestinationFinalize(dest);

    CFRelease(dest);
    CGImageRelease(image);
    CGContextRelease(ctx);
    CGColorSpaceRelease(colorSpace);
}

int main(int argc, char* argv[]) {
    @autoreleasepool {
        std::string iconsetDir = "resources/AppIcon.iconset";
        std::filesystem::create_directories(iconsetDir);

        struct IconSpec {
            std::string name;
            int size;
        };
        std::vector<IconSpec> specs = {
            {"icon_16x16.png", 16},
            {"icon_16x16@2x.png", 32},
            {"icon_32x32.png", 32},
            {"icon_32x32@2x.png", 64},
            {"icon_128x128.png", 128},
            {"icon_128x128@2x.png", 256},
            {"icon_256x256.png", 256},
            {"icon_256x256@2x.png", 512},
            {"icon_512x512.png", 512},
            {"icon_512x512@2x.png", 1024}
        };

        for (const auto& spec : specs) {
            std::string filePath = iconsetDir + "/" + spec.name;
            exportIconPNG(filePath, spec.size);
        }

        // Run iconutil to convert .iconset -> AppIcon.icns
        int res = system("iconutil -c icns resources/AppIcon.iconset -o resources/AppIcon.icns");
        if (res == 0) {
            std::cout << "Successfully created resources/AppIcon.icns!" << std::endl;
        } else {
            std::cerr << "iconutil failed with code " << res << std::endl;
        }
    }
    return 0;
}
