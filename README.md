# Inline (Windows)

A high-performance, ultra-lightweight native Windows system tray autocorrect engine built in C++20, Qt6, and low-level Win32 APIs (`WH_KEYBOARD_LL`, `SendInput`). **Inline** provides instant, sub-millisecond typo correction, context-aware disambiguation (e.g., `helo` → `help` vs `hello`), run-together word splitting, and smart single-key undo recovery directly across all Windows applications with minimal CPU and memory footprint (<5 MB RAM, ~0.0% idle CPU).

> **Looking for macOS?** The native macOS version (`CGEventTap`, Menu Bar) is maintained on the [**`mac-branch`**](https://github.com/LaxExe/Inline-mac-c/tree/mac-branch).

---

## Features

- ⚡ **Ultra-Lightweight & Sub-Millisecond Autocorrection**: Instant unigram dictionary lookups and frequency-weighted SymSpell candidate ranking with zero background overhead (<100 microseconds per query).
- 🧠 **Context-Aware Bigram Disambiguation**: Intelligently scores candidates against preceding conversational context via a 240,000+ bigram corpus (e.g. typing `helo` naturally becomes `help` after "need" or "please", but `hello` after "say" or at the start of a sentence).
- 🧩 **Run-Together Word Splitting**: Automatically detects and splits concatenated words (e.g. `theclass` → `the class`, `ofevening` → `of evening`, `timerighte` → `time right`).
- 🔄 **Single-Key Revert on Backspace**: If a word is autocorrected and you press Backspace, Inline seamlessly reverts the word back to your exact typed input.
- 🚀 **1-Click Native Installer**: Automated batch and PowerShell build scripts (`install_win.bat` / `install_win.ps1`).
- 🧹 **Automatic & Manual Buffer Purging**: Automatically clears all keystroke buffers after 30 seconds of inactivity, on system tray interaction, via manual tray action, and on application quit.
- 🛡️ **Zero Keystroke Dumps on Teardown**: Ensures clean event queue draining and immediate hook unhooking on quit, preventing any buffered keystrokes from dumping to active apps.
- 🔒 **Privacy & Secure Field Protection**: Password and credential fields are automatically excluded from logging, buffering, and corrections.
- 🎛️ **Minimal Native UI**: Sits discreetly in the Windows System Tray with status indicators, settings dialog, and a live testing playground.
- 🧪 **100% Test Coverage**: Complete GoogleTest unit test suite covering hooks, injectors, buffers, priority queues, and undo state machines.

---

## Prerequisites & Installation Guide

### 1. Install Prerequisites

#### A. C++ Compiler & Build Tools
- **Visual Studio 2022** (or 2019):
  - Download the free [Visual Studio Community Edition](https://visualstudio.microsoft.com/vs/community/).
  - In the installer, select the **Desktop development with C++** workload.
- **CMake** (3.20 or newer):
  - Download from [cmake.org](https://cmake.org/download/) or install via Windows Terminal / PowerShell:
    ```powershell
    winget install Kitware.CMake
    ```

#### B. Qt 6 (Free & Open Source)
> [!IMPORTANT]
> **Qt is Free and Open Source (LGPLv3)**. The main Qt landing page often redirects users toward commercial trials and paid subscriptions, which creates confusion. **You do NOT need to buy a license.**

Choose one of the three free installation methods:

* **Method 1: Official Free Open-Source Online Installer (GUI - Recommended)**
  1. Open the [Qt Open-Source Download Portal](https://www.qt.io/download-open-source).
  2. Scroll down to the bottom and download the **Qt Online Installer for Windows**.
  3. Run the installer. Create or log into a free personal community account.
  4. When prompted about licensing, check the box confirming you are using Qt under the **LGPLv3 open-source license**.
  5. Under component selection, expand **Qt 6.x** and select the build matching your compiler (e.g., **MSVC 2022 64-bit** or **MinGW 64-bit**).
  6. Add the Qt `bin` directory (e.g. `C:\Qt\6.6.2\msvc2022_64\bin`) to your system `PATH` (or let CMake detect `CMAKE_PREFIX_PATH=C:\Qt\6.6.2\msvc2022_64`).

* **Method 2: Using vcpkg (No Qt Account Needed)**
  Open Command Prompt or PowerShell:
  ```powershell
  git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
  cd C:\vcpkg
  .\bootstrap-vcpkg.bat
  .\vcpkg.exe install qtbase:x64-windows
  ```

* **Method 3: aqtinstall (Fast CLI Open-Source Qt Downloader)**
  If you have Python installed:
  ```powershell
  pip install aqtinstall
  aqt install-qt windows desktop 6.6.2 win64_msvc2022_64
  ```

---

### 2. Clone and Switch to `windows-branch`

Open Command Prompt (`cmd.exe`) or PowerShell:
```cmd
git clone https://github.com/LaxExe/Inline-mac-c.git
cd Inline-mac-c
git checkout windows-branch
```

---

### 3. Run 1-Click Installer

Double-click `install_win.bat` in File Explorer, or execute in Command Prompt:
```cmd
install_win.bat
```
*(Or in PowerShell: `.\install_win.ps1`)*

The installer will:
1. Detect Visual Studio environment (`vcvarsall.bat`), CMake, and Qt6 installation.
2. Configure a Release build using CMake.
3. Compile `Inline.exe` using all available CPU threads.
4. Run `windeployqt` to bundle required Qt runtime DLLs.
5. Launch `Inline.exe` directly into your Windows System Tray.

---

## Manual Build & Run Instructions

If you prefer building manually from the command line:

```cmd
:: 1. Open the "x64 Native Tools Command Prompt for VS 2022"
:: 2. Navigate to project root:
cd C:\path\to\Inline-mac-c

:: 3. Configure Release build (specify Qt prefix if not on PATH):
cmake -B build_win -DBUILD_WINDOWS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.6.2\msvc2022_64"

:: 4. Build Release binary:
cmake --build build_win --config Release -j%NUMBER_OF_PROCESSORS%

:: 5. Run test suite:
ctest --test-dir build_win --output-on-failure

:: 6. Deploy Qt dependencies to the output folder:
windeployqt build_win\gui\Release\Inline.exe

:: 7. Run Inline:
build_win\gui\Release\Inline.exe
```

---

## Architecture Overview

```
                                  +-----------------------+
                                  | Global Keyboard Hook  |
                                  | (Win WH_KEYBOARD_LL)  |
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
                                  |    Injection Queue    |  (Atomic SendInput replacement)
                                  +-----------+-----------+
```

---

## Stopping or Uninstalling

To terminate running instances and clean up:
```cmd
taskkill /IM Inline.exe /F 2>nul
rmdir /s /q build_win
```

---

## License

This project is licensed under the MIT License.
