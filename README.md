# Inline (macOS)

A high-performance, ultra-lightweight native macOS menu bar typing engine built with C++20, Qt6, and native macOS APIs (`CGEventTap`). **Inline** delivers instant typo correction, context-aware disambiguation, run-together word splitting, and inline **Ghost Text predictive continuations** with next-word and one-word completion modes—all running 100% on-device with zero telemetry and minimal memory footprint (<120 MB RAM active, ~0.0% idle CPU).

> **Looking for Windows?** The native Windows version (`WH_KEYBOARD_LL`, `SendInput`) is maintained on the [**`windows-branch`**](https://github.com/LaxExe/Inline-MacOS-Win32Build/tree/windows-branch).

---

## Table of Contents

- [Website & Direct DMG Download](#website--direct-dmg-download)
- [Installation](#installation)
- [System Requirements & Permissions](#system-requirements--permissions)
- [How to Use Inline](#how-to-use-inline)
- [Ghost Writer & One Word Mode](#ghost-writer--one-word-mode)
- [How It Works](#how-it-works)
- [Key Features](#key-features)
- [Configuration & Settings](#configuration--settings)
- [Building from Source](#building-from-source)
- [Packaging & Releasing](#packaging--releasing)
- [Uninstalling](#uninstalling)
- [License](#license)

---

## Website & Direct DMG Download

Visit the official landing page to explore live interactive scenarios or download the latest release:
- [**https://laxexe.github.io/Inline-MacOS-Win32Build/**](https://laxexe.github.io/Inline-MacOS-Win32Build/)

---

## Installation

To ensure macOS allows Inline to run smoothly without Gatekeeper quarantine warnings, install Inline directly via Terminal using **Homebrew** or the **one-line installer**.

### Option 1: Install with Homebrew (Recommended)

Run this single command in Terminal:

```bash
brew install LaxExe/tap/inline
```

To update Inline in the future:
```bash
brew upgrade --cask inline
```

---

### Option 2: One-Line Terminal Installer (No Homebrew Required)

```bash
curl -fsSL https://raw.githubusercontent.com/LaxExe/Inline-MacOS-Win32Build/mac-branch/install.sh | bash
```

This script automatically downloads the latest release, installs it to `/Applications`, clears the macOS quarantine flag, and launches the app.

---

### Option 3: Build from Source

```bash
git clone https://github.com/LaxExe/Inline-MacOS-Win32Build.git
cd Inline-MacOS-Win32Build
git checkout mac-branch
chmod +x install_mac.sh
./install_mac.sh
```

---

## System Requirements & Permissions

- **Operating System**: macOS 12 Monterey, macOS 13 Ventura, macOS 14 Sonoma, or macOS 15 Sequoia
- **Architecture**: Apple Silicon (M1/M2/M3/M4) or Intel (x86_64)

### macOS Privacy Permissions

Because Inline operates system-wide to detect typing mistakes and display non-intrusive cursor suggestions in real time, macOS requires two standard permissions:

1. **Accessibility**: Allows Inline to synthesize backspace/replacement keystrokes and inspect caret position for ghost text placement.
2. **Input Monitoring**: Allows Inline's low-latency keyboard tap to monitor keystrokes across applications.

#### Granting Permissions:
When launched for the first time, Inline displays a guided setup window:
1. Open **System Settings** -> **Privacy & Security**.
2. Select **Accessibility**, click `+`, and add `/Applications/Inline.app` (enable the toggle).
3. Select **Input Monitoring**, click `+`, and add `/Applications/Inline.app` (enable the toggle).

If permissions ever need to be reset:
```bash
tccutil reset Accessibility com.inline.mac
tccutil reset ListenEvent com.inline.mac
```

---

## How to Use Inline

Once launched, Inline lives discreetly in your macOS menu bar with an adaptive status indicator (`|▪`).

### Everyday Typing Workflow

- **Automatic Correction on Space**: Type naturally in any application. When you press the **Spacebar**, Inline evaluates the completed word. If a typo is recognized, it immediately replaces it with the corrected word.
- **Predictive Ghost Text on Tab**: As you type, Inline shows predictive word suggestions in soft gray inline text immediately to the right of your cursor. Press **Tab** to insert the suggestion.
- **One Word Mode**: When you prefer to complete only the active word being typed (e.g. typing `tha` suggests `nk`), enable One Word Mode. Pressing Tab finishes only that word without chaining additional words.
- **Single-Key Undo via Backspace**: If Inline makes a correction you did not want, simply press **Backspace** once. Inline immediately reverts to your exact raw typed text.
- **Enter / Return Never Autocorrects**: Pressing `Enter` or `Return` (`\n`) commits whatever text you entered as-is, immediately clears the internal buffer, and passes the Return key straight through.
- **Run-Together Word Splitting**: Concatenated words (e.g. `whenI` or `twowords`) are automatically split into separate words (`when I`, `two words`).
- **Sentence Capitalization**: Capitalizes the first letter of sentences and quotes following punctuation (`.`, `!`, `?`).

### Global Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| **`Cmd + Option + Shift + K`** | Toggle Autocorrect Engine On / Off |
| **`Cmd + Option + Shift + G`** | Toggle Ghost Text Predictions On / Off |
| **`Tab`** | Accept visible Ghost Text suggestion |
| **`Backspace`** | Revert last autocorrection or accepted suggestion (Single-Key Undo) |

---

## Ghost Writer & One Word Mode

Inline includes a dual-engine prediction pipeline:

1. **Layer 0 (Sub-Millisecond N-Gram & Prefix Predictor)**:
   - Evaluates active word completions and bigram continuation phrases in $< 100\,\mu\text{s}$.
   - Positioned pixel-accurately immediately adjacent to your text caret across Apple Cocoa controls, Chromium browsers, and Electron text boxes.

2. **Layer 1 (On-Device Neural Model)**:
   - Uses an isolated helper process running quantized `SmolLM2-135M` via `llama.cpp` locally on 2 Apple Silicon efficiency cores (~15 ms latency).
   - Zero external apps (no Ollama, no Docker, no cloud API keys).
   - Automatic thermal throttling and battery protection.

3. **One Word Mode**:
   - In Settings under **Suggestion Length**, select **One word only (current word)** or set **Accept Granularity** to **One word**.
   - Ensures suggestions only finish the word currently being typed at your cursor.

---

## How It Works

```
                              +-----------------------+
                              | Global Keyboard Hook  |
                              |   (Mac CGEventTap)    |
                              +-----------+-----------+
                                          |
                                          v
                              +-----------+-----------+
                              |   Synthetic Filter    |  (Rejects injected loopbacks)
                              +-----------+-----------+
                                          |
                                          v
                              +-----------+-----------+
                              | Ephemeral Word Buffer |  (Single-word, in-memory)
                              +-----------+-----------+
                                    |           |
               (On Word Delimiter)  |           |  (As You Type)
                                    v           v
                        +---------------+   +---------------+
                        | Tier 1 Engine |   | Ghost Writer  |
                        | (SymSpell 82K)|   | (N-gram + LM) |
                        +---------------+   +---------------+
                                    |           |
                                    v           v
                        +---------------+   +---------------+
                        | Atomic Replace|   | Floating View |
                        | (Backspaces)  |   | (Tab Accept)  |
                        +---------------+   +---------------+
```

### Supported Applications
Tested and verified across daily macOS workflows:
- **Apple Native**: Notes, Pages, Safari, Mail, TextEdit, Finder, Reminders.
- **Web Browsers**: Google Chrome, Vivaldi, Brave, Microsoft Edge.
- **Developer & Chat Tools**: Discord, Slack, Visual Studio Code, Antigravity, Notion, Obsidian.

---

## Key Features

- **Microsecond Response Time**: Event tap keystroke inspection completes in $\le 2\,\mu\text{s}$.
- **Context-Aware Bigram Model**: Disambiguates homophones using preceding context words.
- **Single-Key Undo**: Immediate one-touch undo via Backspace to restore raw input.
- **Enter Key Pass-Through**: Return or Enter never triggers autocorrect.
- **180+ Protected Slang & Jargon Acronyms**: Protects `gng`, `brb`, `ttyl`, `idk`, `ngl`, `tbh`, `repo`, `dev`, `pr`, `cli`, `sdk`, `api`, `async`, etc.
- **Dynamic Menu Bar Contrast**: Adaptive white vertical bar on dark menu bars, dark charcoal on light menu bars, with an emerald status indicator.
- **Privacy & Security First**: Completely offline, zero network access, zero disk logging, automatic security suspension in password fields.
- **Modern macOS System Settings UI**: Built to match Apple's modern System Settings with rounded card containers, responsive layouts, and light/dark theme modes.

---

## Configuration & Settings

Open Inline Settings from the menu bar to adjust your preferences:

- **Autocorrect Engine**: Enable or disable system-wide autocorrect.
- **Ghost Text Predictions**: Toggle inline cursor suggestions.
- **Accept Prediction Key**: Select `Tab` or `Right Arrow`.
- **Accept Granularity**: Choose `Whole suggestion` or `One word (Current word only)`.
- **Suggestion Length**: Choose `One word only (current word)`, `Short (1-2 words)`, `Balanced (2-3 words)`, or `Long (up to 5 words)`.
- **Overlay Contrast**: Adjust between `Subtle`, `Standard`, and `High contrast`.
- **Single-Key Undo**: Toggle instant Backspace undo.
- **Launch at Login**: Automatically start Inline when your Mac boots.
- **Custom Ignored Words**: Add proprietary terms, names, or abbreviations that should never be modified.
- **Disabled Applications**: Specify bundle identifiers where Inline should stay dormant.
- **Theme & Appearance**: Switch between `System`, `Light`, and `Dark` appearances.

---

## Building from Source

### Prerequisites

- macOS 12.0 or newer
- Xcode Command Line Tools (`xcode-select --install`)
- CMake (>= 3.20), Ninja, and Qt 6 (`brew install cmake ninja qt6`)

### Build Commands

```bash
# 1. Configure release build
cmake -B build_release -DCMAKE_BUILD_TYPE=Release -G Ninja

# 2. Compile application bundle and prediction helper
cmake --build build_release --target Inline InlinePredictor

# 3. Ad-hoc codesign bundle
codesign --force --sign - build_release/gui/Inline.app/Contents/MacOS/InlinePredictor
codesign --force --deep --sign - -i com.inline.mac build_release/gui/Inline.app

# 4. Copy to Applications & launch
cp -R build_release/gui/Inline.app /Applications/
open /Applications/Inline.app
```

### Running Unit Tests

```bash
# Run all unit tests
ctest --test-dir build_release --output-on-failure

# Or run individual test suites:
./build_release/tests/test_tier1
./build_release/tests/test_ghost_text
./build_release/tests/test_settings_binding
./build_release/tests/test_word_boundary
./build_release/tests/test_undo_state_machine
```

---

## Packaging & Releasing

To create a distributable `.dmg` package with drag-and-drop installer:

```bash
./tools/create_dmg.sh
```

Outputs compressed `dist/Inline-v1.2-macOS.dmg`.

---

## Uninstalling

```bash
# Quit running instance
killall Inline InlinePredictor 2>/dev/null || true

# Remove application bundle
rm -rf /Applications/Inline.app ~/Applications/Inline.app

# Optional: Reset system permissions
tccutil reset Accessibility com.inline.mac
tccutil reset ListenEvent com.inline.mac

# Optional: Remove saved user preferences
rm -rf ~/Library/Application\ Support/Inline
```

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
