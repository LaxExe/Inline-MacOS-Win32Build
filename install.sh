#!/usr/bin/env bash
# ==============================================================================
# Inline - Quick Terminal Installer for macOS
# ==============================================================================
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "\n${BOLD}${BLUE}=== Installing Inline for macOS ===${NC}\n"

# Verify macOS
if [ "$(uname -s)" != "Darwin" ]; then
    echo -e "${RED}Error: Inline is only supported on macOS.${NC}"
    exit 1
fi

# Determine download URL for latest DMG release
REPO="LaxExe/Inline-MacOS-Win32Build"
FALLBACK_URL="https://github.com/${REPO}/releases/download/v1.2/Inline-v1.2-macOS.dmg"

echo "Locating latest release package..."
LATEST_DMG=$(curl -sSL "https://api.github.com/repos/${REPO}/releases/latest" 2>/dev/null | grep "browser_download_url.*\.dmg" | head -n 1 | cut -d '"' -f 4 || true)

if [ -z "$LATEST_DMG" ]; then
    LATEST_DMG="$FALLBACK_URL"
fi

TMP_DMG=$(mktemp /tmp/Inline-Installer-XXXXXX.dmg)
trap 'rm -f "$TMP_DMG"' EXIT

echo "Downloading Inline from GitHub..."
curl -fL --progress-bar "$LATEST_DMG" -o "$TMP_DMG"

# Close any running instance
if pgrep -x "Inline" >/dev/null 2>&1; then
    echo "Closing running instance of Inline..."
    killall Inline 2>/dev/null || true
    sleep 1
fi

# Mount DMG
echo "Mounting installation image..."
MOUNT_OUTPUT=$(hdiutil attach "$TMP_DMG" -nobrowse -readonly)
MOUNT_DIR=$(echo "$MOUNT_OUTPUT" | grep "/Volumes" | awk -F '\t' '{print $NF}')

if [ -z "$MOUNT_DIR" ] || [ ! -d "$MOUNT_DIR/Inline.app" ]; then
    echo -e "${RED}Error: Failed to mount disk image.${NC}"
    exit 1
fi

# Copy app to /Applications
echo "Installing to /Applications/Inline.app..."
rm -rf /Applications/Inline.app
cp -R "$MOUNT_DIR/Inline.app" /Applications/

# Unmount disk image
hdiutil detach "$MOUNT_DIR" -quiet

# Clear quarantine attribute so Gatekeeper allows immediate launch
echo "Configuring permissions and clearing quarantine..."
xattr -cr /Applications/Inline.app

# Launch application
echo "Starting Inline..."
open /Applications/Inline.app

echo -e "\n${BOLD}${GREEN}Installation complete.${NC}"
echo "Inline is now running in your macOS menu bar."
echo "If prompted, please allow Accessibility and Input Monitoring permissions in System Settings."
