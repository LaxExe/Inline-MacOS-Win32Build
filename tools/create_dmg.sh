#!/usr/bin/env bash
# ==============================================================================
# Build and Package Inline macOS DMG (v1.2)
# ==============================================================================
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo -e "${BOLD}${BLUE}=== Building and Packaging Inline v1.2 for macOS ===${NC}\n"

# 0. Ensure bundled GGUF model & llama.cpp are fetched
./tools/fetch_model.sh

# 1. Build Release
echo -e "${BOLD}Building Inline (Release)...${NC}"
cmake -B build_release -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build_release --target Inline InlinePredictor

APP_SRC="build_release/gui/Inline.app"
if [ ! -d "$APP_SRC" ]; then
    echo -e "${RED}Error: Build failed, $APP_SRC not found.${NC}"
    exit 1
fi

# 2. Bundle Qt frameworks via macdeployqt
echo -e "\n${BOLD}Deploying Qt dependencies via macdeployqt...${NC}"
MACDEPLOYQT=$(which macdeployqt || find /opt/homebrew /usr/local -name macdeployqt 2>/dev/null | head -n 1)
if [ -n "$MACDEPLOYQT" ]; then
    "$MACDEPLOYQT" "$APP_SRC" -always-overwrite || true
else
    echo -e "${RED}Warning: macdeployqt not found. App bundle may lack embedded Qt frameworks.${NC}"
fi

# 3. Codesign with explicit bundle identifier (helper first, then outer app bundle)
echo -e "\n${BOLD}Codesigning Inline.app...${NC}"
if [ -f "$APP_SRC/Contents/MacOS/InlinePredictor" ]; then
    codesign --force --sign - "$APP_SRC/Contents/MacOS/InlinePredictor"
fi
codesign --force --deep --sign - -i com.inline.mac "$APP_SRC"

# 4. Prepare DMG Staging
DIST_DIR="$ROOT_DIR/dist"
STAGING_DIR="$ROOT_DIR/build_release/dmg_staging"
mkdir -p "$DIST_DIR"
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR"

echo -e "\n${BOLD}Staging application and Applications symlink...${NC}"
cp -R "$APP_SRC" "$STAGING_DIR/"
ln -s /Applications "$STAGING_DIR/Applications"

# 5. Generate Compressed DMG
DMG_NAME="Inline-v1.2-macOS.dmg"
DMG_PATH="$DIST_DIR/$DMG_NAME"
rm -f "$DMG_PATH"

echo -e "\n${BOLD}Generating $DMG_NAME via hdiutil...${NC}"
hdiutil create \
    -volname "Inline" \
    -srcfolder "$STAGING_DIR" \
    -ov \
    -format UDZO \
    "$DMG_PATH"

# 5. Clean staging directory
rm -rf "$STAGING_DIR"

echo -e "\n${BOLD}${GREEN}✔ Successfully created DMG at:${NC} ${BOLD}$DMG_PATH${NC}"
ls -lh "$DMG_PATH"
