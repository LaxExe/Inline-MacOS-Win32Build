#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include <cmath>

void drawAppIcon(CGContextRef ctx, CGFloat size) {
    CGFloat s = size;
    CGRect rect = CGRectMake(0, 0, s, s);

    // 1. Continuous Apple Squircle
    CGFloat cornerRadius = s * 0.2237;
    CGRect insetRect = CGRectInset(rect, s * 0.04, s * 0.04);
    CGPathRef clipPath = CGPathCreateWithRoundedRect(insetRect, cornerRadius, cornerRadius, NULL);
    CGContextAddPath(ctx, clipPath);
    CGContextClip(ctx);

    // 2. Base Gradient: Deep Slate Midnight to Obsidian Black
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGFloat bgComponents[8] = {
        0.08f, 0.11f, 0.18f, 1.0f, // Top: Slate Midnight
        0.02f, 0.03f, 0.06f, 1.0f  // Bottom: Obsidian
    };
    CGFloat bgLocations[2] = {0.0f, 1.0f};
    CGGradientRef bgGrad = CGGradientCreateWithColorComponents(colorSpace, bgComponents, bgLocations, 2);
    CGContextDrawLinearGradient(ctx, bgGrad, CGPointMake(0, s), CGPointMake(0, 0), 0);
    CGGradientRelease(bgGrad);

    // 3. Central Ambient Radial Glow (Electric Cyan / Royal Blue)
    CGFloat glowComponents[8] = {
        0.20f, 0.50f, 0.98f, 0.30f, // Core Glow
        0.08f, 0.11f, 0.18f, 0.00f  // Falloff
    };
    CGFloat glowLocations[2] = {0.0f, 1.0f};
    CGGradientRef glowGrad = CGGradientCreateWithColorComponents(colorSpace, glowComponents, glowLocations, 2);
    CGContextDrawRadialGradient(ctx, glowGrad,
                                CGPointMake(s * 0.50, s * 0.50), 0,
                                CGPointMake(s * 0.50, s * 0.50), s * 0.45,
                                kCGGradientDrawsAfterEndLocation);
    CGGradientRelease(glowGrad);

    // 4. Subtle Inner Rim Highlight (Apple Glassmorphic Edge)
    CGContextSetRGBStrokeColor(ctx, 1.0f, 1.0f, 1.0f, 0.14f);
    CGContextSetLineWidth(ctx, s * 0.015);
    CGContextAddPath(ctx, clipPath);
    CGContextStrokePath(ctx);

    // 5. Minimal "Inline" Glyph:
    // Left Element: Sleek Glowing Caret / Cursor Bar
    CGFloat barW = s * 0.085;
    CGFloat barH = s * 0.44;
    CGFloat barX = s * 0.38 - barW * 0.5;
    CGFloat barY = s * 0.50 - barH * 0.5;
    CGFloat barRadius = barW * 0.5; // Fully rounded capsule

    CGRect barRect = CGRectMake(barX, barY, barW, barH);
    CGPathRef barPath = CGPathCreateWithRoundedRect(barRect, barRadius, barRadius, NULL);

    // Draw Caret Glow
    CGContextSaveGState(ctx);
    CGContextSetShadowWithColor(ctx, CGSizeMake(0, 0), s * 0.04,
                                CGColorCreateGenericRGB(0.25f, 0.65f, 1.0f, 0.5f));
    CGContextAddPath(ctx, barPath);
    CGContextSetRGBFillColor(ctx, 1.0f, 1.0f, 1.0f, 0.98f);
    CGContextFillPath(ctx);
    CGContextRestoreGState(ctx);

    // Caret Gradient Fill
    CGContextSaveGState(ctx);
    CGContextAddPath(ctx, barPath);
    CGContextClip(ctx);
    CGFloat barComponents[8] = {
        1.00f, 1.00f, 1.00f, 1.00f, // Crisp White Top
        0.55f, 0.85f, 1.00f, 1.00f  // Cyan Tint Bottom
    };
    CGGradientRef barGrad = CGGradientCreateWithColorComponents(colorSpace, barComponents, bgLocations, 2);
    CGContextDrawLinearGradient(ctx, barGrad, CGPointMake(0, barY + barH), CGPointMake(0, barY), 0);
    CGGradientRelease(barGrad);
    CGContextRestoreGState(ctx);
    CGPathRelease(barPath);

    // Right Element: Elegant Geometric 4-Point Autocorrect Spark
    CGFloat sparkCenterX = s * 0.63;
    CGFloat sparkCenterY = s * 0.50;
    CGFloat sparkRadius = s * 0.16;
    CGFloat sparkInner = s * 0.045;

    CGMutablePathRef sparkPath = CGPathCreateMutable();
    // 4 points (Top, Right, Bottom, Left) with smooth concave curved transitions
    CGPathMoveToPoint(sparkPath, NULL, sparkCenterX, sparkCenterY + sparkRadius);
    CGPathAddQuadCurveToPoint(sparkPath, NULL, sparkCenterX + sparkInner, sparkCenterY + sparkInner, sparkCenterX + sparkRadius, sparkCenterY);
    CGPathAddQuadCurveToPoint(sparkPath, NULL, sparkCenterX + sparkInner, sparkCenterY - sparkInner, sparkCenterX, sparkCenterY - sparkRadius);
    CGPathAddQuadCurveToPoint(sparkPath, NULL, sparkCenterX - sparkInner, sparkCenterY - sparkInner, sparkCenterX - sparkRadius, sparkCenterY);
    CGPathAddQuadCurveToPoint(sparkPath, NULL, sparkCenterX - sparkInner, sparkCenterY + sparkInner, sparkCenterX, sparkCenterY + sparkRadius);
    CGPathCloseSubpath(sparkPath);

    // Draw Spark Glow
    CGContextSaveGState(ctx);
    CGContextSetShadowWithColor(ctx, CGSizeMake(0, 0), s * 0.05,
                                CGColorCreateGenericRGB(0.20f, 0.85f, 0.70f, 0.6f));
    CGContextAddPath(ctx, sparkPath);
    CGContextSetRGBFillColor(ctx, 0.40f, 0.95f, 0.75f, 1.0f);
    CGContextFillPath(ctx);
    CGContextRestoreGState(ctx);

    // Spark Gradient Fill (Emerald Green to Electric Cyan)
    CGContextSaveGState(ctx);
    CGContextAddPath(ctx, sparkPath);
    CGContextClip(ctx);
    CGFloat sparkComponents[8] = {
        0.35f, 0.95f, 0.75f, 1.0f, // Radiant Emerald
        0.20f, 0.65f, 1.00f, 1.0f  // Electric Cyan
    };
    CGGradientRef sparkGrad = CGGradientCreateWithColorComponents(colorSpace, sparkComponents, bgLocations, 2);
    CGContextDrawLinearGradient(ctx, sparkGrad,
                                CGPointMake(sparkCenterX - sparkRadius, sparkCenterY + sparkRadius),
                                CGPointMake(sparkCenterX + sparkRadius, sparkCenterY - sparkRadius), 0);
    CGGradientRelease(sparkGrad);
    CGContextRestoreGState(ctx);
    CGPathRelease(sparkPath);

    CGColorSpaceRelease(colorSpace);
    CGPathRelease(clipPath);
}

void exportIconPNG(const std::string& path, int size) {
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(NULL, size, size, 8, size * 4, colorSpace, kCGImageAlphaPremultipliedLast);

    drawAppIcon(ctx, size);

    CGImageRef image = CGBitmapContextCreateImage(ctx);
    NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL((__bridge CFURLRef)url, CFSTR("public.png"), 1, NULL);
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
            std::cout << "Successfully created minimal modern resources/AppIcon.icns!" << std::endl;
        } else {
            std::cerr << "iconutil failed with code " << res << std::endl;
        }
    }
    return 0;
}
