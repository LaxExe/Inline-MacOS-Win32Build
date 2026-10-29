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

    // 2. Base Surface: Solid, flat deep industrial obsidian (no gradients, no glow)
    CGContextSetRGBFillColor(ctx, 0.08f, 0.08f, 0.09f, 1.0f); // #141417
    CGContextFillRect(ctx, rect);

    // 3. Crisp 1px Architectural Perimeter Rule
    CGContextSetRGBStrokeColor(ctx, 0.20f, 0.20f, 0.23f, 1.0f); // #33333A
    CGContextSetLineWidth(ctx, s * 0.012);
    CGContextAddPath(ctx, clipPath);
    CGContextStrokePath(ctx);

    // 4. High-Contrast Editorial Geometric Monogram for "Inline"
    // Left Element: Crisp vertical cursor rule
    CGFloat ruleW = s * 0.075;
    CGFloat ruleH = s * 0.48;
    CGFloat ruleX = s * 0.38 - ruleW * 0.5;
    CGFloat ruleY = s * 0.26;

    CGRect ruleRect = CGRectMake(ruleX, ruleY, ruleW, ruleH);
    CGContextSetRGBFillColor(ctx, 0.98f, 0.98f, 0.98f, 1.0f); // Pure paper white #FAFAFA
    CGContextFillRect(ctx, ruleRect);

    // Baseline Accent Rule (horizontal editorial typographic line)
    CGFloat baseW = s * 0.22;
    CGFloat baseH = s * 0.045;
    CGFloat baseX = ruleX;
    CGFloat baseY = ruleY + ruleH - baseH;
    CGContextFillRect(ctx, CGRectMake(baseX, baseY, baseW, baseH));

    // Right Element: High-contrast geometric red precision indicator dot (1 intentional accent color)
    CGFloat dotSize = s * 0.085;
    CGFloat dotX = s * 0.62;
    CGFloat dotY = s * 0.44;
    CGRect dotRect = CGRectMake(dotX, dotY, dotSize, dotSize);

    CGContextSetRGBFillColor(ctx, 0.88f, 0.16f, 0.18f, 1.0f); // Warm vermilion #E11D48
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
            std::cout << "Successfully created restrained architectural resources/AppIcon.icns!" << std::endl;
        } else {
            std::cerr << "iconutil failed with code " << res << std::endl;
        }
    }
    return 0;
}
