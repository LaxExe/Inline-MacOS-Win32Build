# Inline (macOS)

A high-performance, ultra-lightweight native macOS menu bar autocorrect engine built with C++20, Qt6, and native macOS APIs (`CGEventTap`). **Inline** delivers instant, sub-millisecond typo correction, context-aware disambiguation, run-together word splitting, and smart single-key undo recovery directly across macOS applications with minimal CPU and memory footprint (<5 MB RAM, ~0.0% idle CPU).

> **Looking for Windows?** The native Windows version (`WH_KEYBOARD_LL`, `SendInput`) is maintained on the [**`windows-branch`**](https://github.com/LaxExe/Inline-MacOS-Win32Build/tree/windows-branch).

---

## Table of Contents

- [Installation](#installation)
- [System Requirements & Permissions](#system-requirements--permissions)
- [How to Use Inline](#how-to-use-inline)
- [How It Works](#how-it-works)
- [Key Features](#key-features)
- [Configuration & Settings](#configuration--settings)
- [Building from Source](#building-from-source)
- [Uninstalling](#uninstalling)
- [License](#license)

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

If you do not have Homebrew installed, copy and paste this command into Terminal:

```bash
curl -fsSL https://raw.githubusercontent.com/LaxExe/Inline-MacOS-Win32Build/mac-branch/install.sh | bash
```

This script automatically downloads the latest release, installs it to `/Applications`, clears the macOS quarantine flag, and launches the app.

---

### Option 3: Build from Source

If you prefer building from source with Xcode and CMake:

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

Because Inline operates system-wide to detect typing mistakes and replace text in real time, macOS requires two permissions:

1. **Accessibility**: Allows Inline to synthesize backspace and replacement keystrokes to fix typos.
2. **Input Monitoring**: Allows Inline's low-latency keyboard tap to monitor keystrokes across applications.

#### Granting Permissions:
When launched for the first time, Inline displays a guided setup window:
1. Open **System Settings** -> **Privacy & Security**.
2. Select **Accessibility**, click the `+` icon, and add `/Applications/Inline.app` (enable the toggle).
3. Select **Input Monitoring**, click the `+` icon, and add `/Applications/Inline.app` (enable the toggle).
4. Restart Inline if prompted by macOS.

If permissions need to be reset at any point:
```bash
tccutil reset Accessibility com.inline.mac
tccutil reset ListenEvent com.inline.mac
```

---

## How to Use Inline

Once launched, Inline lives discreetly in your macOS menu bar with an adaptive status indicator (`|▪`).

### Everyday Typing Workflow

- **Automatic Correction on Space**: Type naturally in any application (Notes, Slack, Safari, Messages, Xcode, Terminal, etc.). When you press the **Spacebar**, Inline evaluates the word you just completed. If a typo is recognized, it immediately replaces it with the corrected word.
- **Single-Key Undo via Backspace**: If Inline makes a correction you did not want, simply press **Backspace** once. Inline immediately undoes the correction and restores your exact raw typed text.
- **Enter / Return Never Autocorrects**: Pressing `Enter` or `Return` (`\n`) commits whatever text you entered as-is, immediately clears the internal buffer, and passes the Return key straight through.
- **Run-Together Word Splitting**: If you accidentally omit a space between words (e.g. typing `theclass` or `ofevening`), Inline automatically splits them (`the class`, `of evening`).
- **Sentence Capitalization**: The first word of a sentence is capitalized automatically following sentence-ending punctuation (`.`, `!`, `?`).

### Menu Bar Controls

Click the Inline menu bar item to access quick controls:
- **Status Indicator**: Displays whether the engine is active and running.
- **Settings...**: Opens the configuration dialog.
- **Clear Keystroke Buffer**: Manually empties the typing buffer.
- **Quit Inline**: Cleanly disables all hooks and exits without residual input dumps.

---

## How It Works

Inline operates through a multi-stage pipeline designed for extreme speed, strict privacy, and zero perceptible latency.

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
                              | Ephemeral Word Buffer |  (Single-word, 30s auto-purge)
                              +-----------+-----------+
                                          |
                                          v
                              +-----------+-----------+
                              |   Tier 1 Corrector    |  (SymSpell 82K + Bigrams)
                              +-----------+-----------+
                                          |
                                          v
                              +-----------+-----------+
                              |    Injection Queue    |  (Atomic text replacement)
                              +-----------+-----------+
```

### 1. Low-Latency Interception Layer
Inline installs an event tap via macOS `CGEventTap` (`kCGHIDEventTap`). Every keystroke is evaluated in under 2 microseconds. Keystrokes generated synthetically by Inline are tagged with a custom event source marker (`0x494E4C4E` / `'INLN'`) and skipped immediately to prevent recursion or feedback loops.

### 2. Ephemeral In-Memory Buffering
Inline stores only the current word being typed in volatile memory. 
- No typing history is ever saved to disk.
- Password and credential fields are detected and excluded from buffering.
- The buffer is automatically cleared after 30 seconds of typing inactivity, when clicking the menu bar, or switching applications.

### 3. Tier 1 Correction & Context Engine
When a word delimiter (`Space`) is pressed:
- **Unigram Dictionary Lookup**: Matches the word against an 82,000+ entry English frequency dictionary using precomputed symmetric deletion indexes (SymSpell), completing queries in under 100 microseconds.
- **Bigram Context Disambiguation**: Uses a 243,000+ entry bigram language model to disambiguate homophones and context-dependent corrections. For example, typing `helo` naturally becomes `help` after words like "need" or "please", but becomes `hello` after "say" or when starting a sentence.
- **Slang and Technical Jargon Whitelist**: Over 180 internet slang abbreviations (`gng`, `brb`, `ttyl`, `idk`, `ngl`, `tbh`, `rn`, `imo`, etc.) and developer terms (`repo`, `dev`, `pr`, `cli`, `sdk`, `api`, `auth`, `async`, `env`, etc.) are protected from false-positive corrections.

### 4. Atomic Keystroke Replacement
When a correction is triggered:
- Inline calculates the number of backspaces required to remove the misspelled word.
- Synthesizes backspaces followed by the replacement characters using native `CGEventCreateKeyboardEvent`.
- Caches the substitution event in an internal Undo State Machine. If Backspace is pressed next, the substitution is reverted seamlessly.

---

## Key Features

- **Sub-Millisecond Response Time**: Typo lookups and ranking complete in under 100 microseconds with zero background CPU load (~0.0% idle).
- **Context-Aware Bigram Model**: Disambiguates candidate words using preceding context for accurate suggestions.
- **Run-Together Word Splitting**: Intelligently detects and separates words typed without spaces (e.g. `timerighte` -> `time right`).
- **Single-Key Undo**: Immediate one-touch undo via Backspace to restore the exact raw input if a correction is not desired.
- **Enter Key Pass-Through**: Pressing Return or Enter never triggers autocorrect, ensuring code blocks, commands, and short lines remain untouched.
- **180+ Slang & Jargon Terms Protected**: Built-in protection for modern slang, abbreviations, and engineering acronyms.
- **Dynamic Menu Bar Contrast**: The menu bar icon adapts to dark and light wallpapers, displaying in pure white on dark menu bars and dark charcoal on light menu bars, with an emerald status indicator.
- **Privacy & Security First**:
  - No internet connection required; completely offline operation.
  - Zero keystroke logs written to disk.
  - Ephemeral in-memory buffer auto-purged after 30 seconds.
  - Password and secure entry fields are ignored.
- **Native macOS Settings Dialog**: Apple System Settings-inspired UI with rounded cards, monochrome vector icons, ignored words management, disabled applications list, and a live testing playground.

---

## Configuration & Settings

Open Inline Settings from the menu bar to adjust your preferences:

- **Master Autocorrect Engine**: Enable or disable system-wide autocorrect on the fly.
- **Single-Key Undo**: Toggle instant Backspace undo behavior.
- **Run-Together Word Splitting**: Toggle automatic space insertion for joined words.
- **Sentence Capitalization**: Toggle automatic capitalization for sentences.
- **Launch at Login**: Automatically start Inline when your Mac boots or logs in.
- **Custom Ignored Words**: Add proprietary terms, names, or custom abbreviations that should never be corrected.
- **Disabled Applications**: Specify bundle identifiers or application names where Inline should stay disabled (e.g. specific IDEs, terminals, or games).
- **Interactive Playground**: Test corrections, undo behavior, and delimiter handling inside a dedicated test sandbox.

---

## Building from Source

### Prerequisites

- macOS 12.0 or newer
- Xcode Command Line Tools:
  ```bash
  xcode-select --install
  ```
- CMake (>= 3.20), Ninja, and Qt 6:
  ```bash
  brew install cmake ninja qt6
  ```

### Build Commands

```bash
# 1. Configure release build
cmake -B build_release -DCMAKE_BUILD_TYPE=Release -G Ninja

# 2. Compile application bundle
cmake --build build_release --target Inline

# 3. Ad-hoc codesign bundle
codesign --force --deep --sign - -i com.inline.mac build_release/gui/Inline.app

# 4. Copy to Applications
cp -R build_release/gui/Inline.app /Applications/

# 5. Launch application
open /Applications/Inline.app
```

### Running Unit Tests

Inline includes a comprehensive GoogleTest suite:

```bash
# Run all unit tests
ctest --test-dir build_release --output-on-failure

# Or run individual test suites:
./build_release/tests/test_tier1
./build_release/tests/test_settings_binding
./build_release/tests/test_word_boundary
./build_release/tests/test_undo_state_machine
```

### Packaging DMG

To create a distributable `.dmg` package with drag-and-drop installer:

```bash
./tools/create_dmg.sh
```

The resulting package will be placed in `dist/Inline-vX.X-macOS.dmg`.

---

## Uninstalling

To remove Inline completely from your system:

```bash
# Quit running instance
killall Inline 2>/dev/null || true

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
