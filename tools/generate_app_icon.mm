#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <iostream>
#include <filesystem>
#include <vector>

void drawAppIcon(CGContextRef ctx, CGFloat size) {
    CGFloat s = size;
    CGRect rect = CGRectMake(0, 0, s, s);

    // 1. Continuous Apple Squircle
    CGFloat cornerRadius = s * 0.2237;
    CGRect insetRect = CGRectInset(rect, s * 0.04, s * 0.04);
    CGPathRef clipPath = CGPathCreateWithRoundedRect(insetRect, cornerRadius, cornerRadius, NULL);
    CGContextAddPath(ctx, clipPath);
    CGContextClip(ctx);

    // 2. Base Surface: Solid, flat industrial obsidian
    CGContextSetRGBFillColor(ctx, 0.08f, 0.08f, 0.09f, 1.0f); // #141417
    CGContextFillRect(ctx, rect);

    // 3. Crisp 1px Architectural Perimeter Rule
    CGContextSetRGBStrokeColor(ctx, 0.20f, 0.20f, 0.23f, 1.0f); // #33333A
    CGContextSetLineWidth(ctx, s * 0.012);
    CGContextAddPath(ctx, clipPath);
    CGContextStrokePath(ctx);

    // 4. Exact Menu Bar Emblem (Cursor Rule + Forest Green Precision Indicator)
    // Proportions matching the menu bar glyph:
    // Vertical rule: 2.0w x 12.0h -> aspect ratio 1 : 6
    // Precision dot: 3.0w x 3.0h -> aspect ratio 1 : 1
    // Gap: 2.0
    CGFloat ruleW = s * 0.08;
    CGFloat ruleH = s * 0.48;
    CGFloat gap = s * 0.065;
    CGFloat dotSize = s * 0.12;

    CGFloat totalW = ruleW + gap + dotSize;
    CGFloat startX = (s - totalW) * 0.5;
    CGFloat ruleY = (s - ruleH) * 0.5;

    // A. Vertical Cursor Rule (pure paper white)
    CGRect ruleRect = CGRectMake(startX, ruleY, ruleW, ruleH);
    CGContextSetRGBFillColor(ctx, 0.98f, 0.98f, 0.98f, 1.0f); // #FAFAFA
    CGContextFillRect(ctx, ruleRect);

    // B. Precision Status Square (crisp forest green matching menu bar active state)
    CGFloat dotX = startX + ruleW + gap;
    CGFloat dotY = (s - dotSize) * 0.5; // Centered vertically
    CGRect dotRect = CGRectMake(dotX, dotY, dotSize, dotSize);

    CGContextSetRGBFillColor(ctx, 0.08f, 0.50f, 0.24f, 1.0f); // Forest Green #15803D
    CGContextFillRect(ctx, dotRect);

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

        int res = system("iconutil -c icns resources/AppIcon.iconset -o resources/AppIcon.icns");
        if (res == 0) {
            std::cout << "Successfully created resources/AppIcon.icns with menu bar glyph!" << std::endl;
        } else {
            std::cerr << "iconutil failed with code " << res << std::endl;
        }
    }
    return 0;
}
