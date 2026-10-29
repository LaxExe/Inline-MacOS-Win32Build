# Inline

A high-performance, ultra-lightweight native system tray / menu bar autocorrect engine built in C++20, Qt6, and low-level native OS APIs. **Inline** provides instant, sub-millisecond typo correction, context-aware disambiguation (e.g., `helo` → `help` vs `hello`), run-together word splitting, and smart single-key undo recovery directly across all applications with minimal CPU and memory footprint (<5 MB RAM, ~0.0% idle CPU).

---

## Features

- ⚡ **Ultra-Lightweight & Sub-Millisecond Autocorrection**: Instant unigram dictionary lookups and frequency-weighted SymSpell candidate ranking with zero background overhead (<100 microseconds per query).
- 🧠 **Context-Aware Bigram Disambiguation**: Intelligently scores candidates against preceding conversational context via a 240,000+ bigram corpus (e.g. typing `helo` naturally becomes `help` after "need" or "please", but `hello` after "say" or at the start of a sentence).
- 🧩 **Run-Together Word Splitting**: Automatically detects and splits concatenated words (e.g. `theclass` → `the class`, `ofevening` → `of evening`, `timerighte` → `time right`).
- 🔄 **Single-Key Revert on Backspace**: If a word is autocorrected and you press Backspace, Inline seamlessly reverts the word back to your exact typed input.
- 🚀 **1-Click Native Installers**: One command build & installation scripts for both macOS (`./install_mac.sh`) and Windows (`install_win.bat`).
- 🔐 **First-Run Permission Handling**: Interactive permission guide ensuring Accessibility and Input Monitoring permissions are active on macOS.
- 🧹 **Automatic & Manual Buffer Purging**: Automatically clears all keystroke buffers after 30 seconds of inactivity, on menu bar/tray interaction, via manual menu action, and on application quit.
- 🛡️ **Zero Keystroke Dumps on Teardown**: Ensures clean event queue draining and immediate hook disabling on quit, preventing any buffered keystrokes from dumping to active apps.
- 🔒 **Privacy & Secure Field Protection**: Password and credential fields are automatically excluded from logging, buffering, and corrections.
- 🎛️ **Minimal Native UI**: Sits discreetly in the macOS Menu Bar or Windows System Tray with status indicators, settings, and a live testing playground.
- 🧪 **100% Test Coverage**: Complete GoogleTest unit test suite covering hooks, injectors, buffers, priority queues, and undo state machines.

---

## Branches & Download Matrix

Inline is organized across dedicated branches optimized for each operating system:

| Platform | Branch | Native Hook / API | Installer Script | Target Output |
| :--- | :--- | :--- | :--- | :--- |
| **🍎 macOS** | [**`mac-branch`**](https://github.com/LaxExe/Inline-MacOS-Win32Build/tree/mac-branch) | `CGEventTap`, Cocoa, Ad-Hoc Sign | `./install_mac.sh` | `/Applications/Inline.app` |
| **🪟 Windows** | [**`windows-branch`**](https://github.com/LaxExe/Inline-MacOS-Win32Build/tree/windows-branch) | `WH_KEYBOARD_LL`, `SendInput` | `install_win.bat` | `Inline.exe` (System Tray) |

---

## Installation & Setup Instructions

### 🍎 macOS Installation

#### 1. Prerequisites
Install Xcode Command Line Tools, CMake, and Qt6 (e.g. via Homebrew):
```bash
# Install Xcode Command Line Tools if not already installed
xcode-select --install

# Install CMake and Qt6 via Homebrew
brew install cmake qt6
```

#### 2. Clone and Switch to `mac-branch`
```bash
git clone https://github.com/LaxExe/Inline-MacOS-Win32Build.git
cd Inline-MacOS-Win32Build
git checkout mac-branch
```

#### 3. Run 1-Click Installer
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

#### 4. macOS Permissions
On first launch, macOS requires two permissions for system-wide autocorrection:
- **Accessibility** (`System Settings > Privacy & Security > Accessibility`)
- **Input Monitoring** (`System Settings > Privacy & Security > Input Monitoring`)

Toggle **Inline** to **ON** in both sections when prompted.

---

### 🪟 Windows Installation

#### 1. Prerequisites
- **Visual Studio 2019/2022** (with "Desktop development with C++" workload) or MinGW-w64.
- **CMake** (3.20 or newer): [Download from cmake.org](https://cmake.org/download/) or via `winget install Kitware.CMake`.
- **Qt6** (Qt 6.2+ MinGW or MSVC): [Download Qt](https://www.qt.io/download).

#### 2. Clone and Switch to `windows-branch`
Open Command Prompt (`cmd.exe`) or PowerShell:
```cmd
git clone https://github.com/LaxExe/Inline-MacOS-Win32Build.git
cd Inline-MacOS-Win32Build
git checkout windows-branch
```

#### 3. Run 1-Click Installer
Double-click `install_win.bat` in File Explorer, or run in Command Prompt:
```cmd
install_win.bat
```
*(Or in PowerShell: `.\install_win.ps1`)*

The script builds `Inline.exe` in Release mode using available CPU cores and allows you to launch it straight into your System Tray.

---

## Manual Build Instructions

### macOS
```bash
cmake -B build_release -DCMAKE_BUILD_TYPE=Release
cmake --build build_release -j$(sysctl -n hw.ncpu)
ctest --test-dir build_release --output-on-failure
open build_release/gui/Inline.app
```

### Windows
```cmd
cmake -B build_win -DBUILD_WINDOWS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_win --config Release -j%NUMBER_OF_PROCESSORS%
ctest --test-dir build_win --output-on-failure
build_win\gui\Release\Inline.exe
```

---

## Architecture Overview

```
                                  +-----------------------+
                                  | Global Keyboard Hook  |
                                  |  (Mac CGEventTap /    |
                                  |   Win WH_KEYBOARD_LL) |
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

### macOS
To cleanly remove old installations before reinstalling:
```bash
killall Inline 2>/dev/null || true
rm -rf /Applications/Inline.app ~/Applications/Inline.app
```

### Windows
To exit and remove:
```cmd
taskkill /IM Inline.exe /F 2>nul
rem Delete the compiled build folder or installed executable directory
```

---

## License

This project is licensed under the MIT License.
