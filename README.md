# Inline (macOS)

A high-performance, ultra-lightweight native macOS menu bar autocorrect engine built in C++20, Qt6, and native macOS APIs (`CGEventTap`). **Inline** provides instant, sub-millisecond typo correction, context-aware disambiguation (e.g., `helo` → `help` vs `hello`), run-together word splitting, and smart single-key undo recovery directly across all macOS applications with minimal CPU and memory footprint (<5 MB RAM, ~0.0% idle CPU).

> **Looking for Windows?** The native Windows version (`WH_KEYBOARD_LL`, `SendInput`) is maintained on the [**`windows-branch`**](https://github.com/LaxExe/Inline-mac-c/tree/windows-branch).

---

## Features

- ⚡ **Ultra-Lightweight & Sub-Millisecond Autocorrection**: Instant unigram dictionary lookups and frequency-weighted SymSpell candidate ranking with zero background overhead (<100 microseconds per query).
- 🧠 **Context-Aware Bigram Disambiguation**: Intelligently scores candidates against preceding conversational context via a 240,000+ bigram corpus (e.g. typing `helo` naturally becomes `help` after "need" or "please", but `hello` after "say" or at the start of a sentence).
- 🧩 **Run-Together Word Splitting**: Automatically detects and splits concatenated words (e.g. `theclass` → `the class`, `ofevening` → `of evening`, `timerighte` → `time right`).
- 🔄 **Single-Key Revert on Backspace**: If a word is autocorrected and you press Backspace, Inline seamlessly reverts the word back to your exact typed input.
- 🚀 **1-Click Native Installer**: Automated `./install_mac.sh` script to build, package icons, and install directly to `/Applications/Inline.app`.
- 🔐 **First-Run Permission Onboarding**: Interactive permission setup window guiding you through Input Monitoring and Accessibility with live verification.
- 🧹 **Automatic & Manual Buffer Purging**: Automatically clears all keystroke buffers after 30 seconds of inactivity, on menu bar interaction, via manual menu action, and on application quit.
- 🛡️ **Zero Keystroke Dumps on Teardown**: Ensures clean event queue draining and immediate hook disabling on quit, preventing any buffered keystrokes from dumping to active apps.
- 🔒 **Privacy & Secure Field Protection**: Password and credential fields are automatically excluded from logging, buffering, and corrections.
- 🎛️ **Minimal Native Menu Bar UI**: Sits discreetly in the macOS Menu Bar with a clean status indicator, live testing playground, and hotkey controls.
- 🧪 **100% Test Coverage**: Complete GoogleTest unit test suite covering hooks, injectors, buffers, priority queues, and undo state machines.

---

## Installation & Setup Instructions

### 1. Prerequisites
Install Xcode Command Line Tools, CMake, and Qt6 via Homebrew:
```bash
# Install Xcode Command Line Tools if not already installed
xcode-select --install

# Install CMake and Qt6 via Homebrew
brew install cmake qt6
```

### 2. Clone and Switch to `mac-branch`
```bash
git clone https://github.com/LaxExe/Inline-mac-c.git
cd Inline-mac-c
git checkout mac-branch
```

### 3. Run 1-Click Installer
```bash
chmod +x install_mac.sh
./install_mac.sh
```
This script will:
1. Verify Clang, CMake, and Qt6 paths.
2. Build the optimized Release bundle.
3. Install the application to `/Applications/Inline.app` (or `~/Applications/Inline.app`).
4. Ad-hoc codesign the `.app` bundle.
5. Prompt you to launch Inline into your menu bar immediately.

### 4. macOS Permissions
On first launch, macOS requires two permissions for system-wide autocorrection:
- **Accessibility** (`System Settings > Privacy & Security > Accessibility`)
- **Input Monitoring** (`System Settings > Privacy & Security > Input Monitoring`)

Toggle **Inline** to **ON** in both sections when prompted.

---

## Manual Build & Run Instructions

```bash
# Configure Release build
cmake -B build_release -DCMAKE_BUILD_TYPE=Release

# Compile using all available CPU cores
cmake --build build_release -j$(sysctl -n hw.ncpu)

# Run test suite
ctest --test-dir build_release --output-on-failure

# Launch the app
open build_release/gui/Inline.app
```

---

## Architecture Overview

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

---

## Removing Old Installed Versions

To cleanly remove old installations before reinstalling:
```bash
killall Inline 2>/dev/null || true
rm -rf /Applications/Inline.app ~/Applications/Inline.app
```

---

## License

This project is licensed under the MIT License.
