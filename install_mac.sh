#!/usr/bin/env bash
# ==============================================================================
# Inline - One-Click macOS Installer
# ==============================================================================
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m' # No Color

echo -e "${BOLD}${BLUE}=== Installing Inline for macOS ===${NC}\n"

# 1. Check for Xcode / Clang
if ! command -v clang++ >/dev/null 2>&1; then
    echo -e "${RED}Error: Clang / Xcode Command Line Tools not found.${NC}"
    echo "Please run: xcode-select --install"
    exit 1
fi

# 2. Check for CMake
if ! command -v cmake >/dev/null 2>&1; then
    echo -e "${RED}Error: CMake is not installed.${NC}"
    echo "Install via Homebrew: brew install cmake"
    exit 1
fi

# 3. Locate Qt6
QT_PATH=""
if [ -d "/opt/homebrew/opt/qt" ]; then
    QT_PATH="/opt/homebrew/opt/qt"
elif [ -d "/usr/local/opt/qt" ]; then
    QT_PATH="/usr/local/opt/qt"
elif [ -d "/opt/homebrew/opt/qt6" ]; then
    QT_PATH="/opt/homebrew/opt/qt6"
elif [ -d "/usr/local/opt/qt6" ]; then
    QT_PATH="/usr/local/opt/qt6"
fi

CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Release"
if command -v ninja >/dev/null 2>&1; then
    CMAKE_FLAGS="$CMAKE_FLAGS -G Ninja"
fi
if [ -n "$QT_PATH" ]; then
    echo -e "Found Qt6 at: ${GREEN}$QT_PATH${NC}"
    CMAKE_FLAGS="$CMAKE_FLAGS -DCMAKE_PREFIX_PATH=$QT_PATH"
fi

# 4. Generate modern app icon if not present
if [ ! -f "resources/AppIcon.icns" ]; then
    echo "Generating application icon..."
    clang++ -std=c++17 -framework Cocoa tools/generate_app_icon.mm -o tools/generate_app_icon
    ./tools/generate_app_icon
fi

# 5. Fetch bundled language model & llama.cpp runtime if not present
./tools/fetch_model.sh

# 6. Build Release bundle
echo -e "\n${BOLD}Building Inline (Release)...${NC}"
cmake -B build_release $CMAKE_FLAGS
cmake --build build_release -j$(sysctl -n hw.ncpu || echo 4)

APP_SRC="build_release/gui/Inline.app"
if [ ! -d "$APP_SRC" ]; then
    echo -e "${RED}Build failed: $APP_SRC not found.${NC}"
    exit 1
fi

# 6. Install to Applications
TARGET_DIR="/Applications"
if [ ! -w "$TARGET_DIR" ]; then
    TARGET_DIR="$HOME/Applications"
    mkdir -p "$TARGET_DIR"
fi

DEST_APP="$TARGET_DIR/Inline.app"
echo -e "\nInstalling to: ${GREEN}$DEST_APP${NC}"
rm -rf "$DEST_APP"
cp -R "$APP_SRC" "$DEST_APP"

# 7. Ad-hoc codesign with bundle identifier
codesign --force --deep --sign - -i com.inline.mac "$DEST_APP" 2>/dev/null || true

echo -e "\n${BOLD}${GREEN}✔ Installation successful!${NC}"
echo -e "Inline is installed at: ${BOLD}$DEST_APP${NC}\n"

# 8. Launch prompt
read -p "Would you like to open Inline now? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    open "$DEST_APP"
    echo -e "${GREEN}Inline launched in your menu bar!${NC}"
fi
