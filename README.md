# Inline (Windows)

A high-performance, ultra-lightweight native Windows system tray autocorrect engine built in C++20, Qt6, and low-level Win32 APIs (`WH_KEYBOARD_LL`, `SendInput`). **Inline** provides instant, sub-millisecond typo correction, context-aware disambiguation (e.g., `helo` → `help` vs `hello`), run-together word splitting, and smart single-key undo recovery directly across all Windows applications with minimal CPU and memory footprint (<5 MB RAM, ~0.0% idle CPU).

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
  - In the Visual Studio Installer, ensure the **Desktop development with C++** workload is checked and installed.
- **CMake** (version 3.20 or newer):
  - Download from [cmake.org](https://cmake.org/download/) or install via Windows Terminal / PowerShell:
    ```powershell
    winget install Kitware.CMake
    ```

#### B. Qt 6 (Free & Open Source - LGPLv3)
> [!IMPORTANT]
> **Qt is Free and Open Source under the LGPLv3 license.**
> The main Qt homepage frequently pushes users toward commercial trials and paid subscriptions. **You do NOT need to buy a license or enter payment information.**

Choose any of the three free installation options:

* **Option 1: Official Free Open-Source Online Installer (Recommended GUI)**
  1. Open the [Qt Open-Source Download Portal](https://www.qt.io/download-open-source).
  2. Scroll to the bottom and click **Download the Qt Online Installer**.
  3. Run the installer and sign in with a free personal Qt Community account (or create one for free).
  4. On the license agreement screen, check the box confirming you are using Qt under the **LGPLv3 open-source license**.
  5. In the component selection tree:
     - Expand **Qt 6.x** (e.g., 6.6 or 6.7).
     - Check **MSVC 2022 64-bit** (if using Visual Studio) or **MinGW 64-bit**.
  6. Finish installation. (Optional: add `C:\Qt\6.x.x\msvc2022_64\bin` to your system `PATH`).

* **Option 2: Using vcpkg (No Qt Account Required)**
  Open Command Prompt or PowerShell:
  ```powershell
  git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
  cd C:\vcpkg
  .\bootstrap-vcpkg.bat
  .\vcpkg.exe install qtbase:x64-windows
  ```

* **Option 3: Using aqtinstall (Fast CLI Open-Source Downloader via Python)**
  If Python is installed on your machine:
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

Double-click `install_win.bat` in File Explorer, or run in Command Prompt:
```cmd
install_win.bat
```
*(Or in PowerShell: `.\install_win.ps1`)*

The installer script will:
1. Detect your Visual Studio environment (`vcvarsall.bat`), CMake, and Qt6 paths.
2. Generate and configure a Release build directory (`build_win`).
3. Compile `Inline.exe` in parallel across all CPU cores.
4. Run `windeployqt` to bundle required Qt runtime DLLs next to the executable.
5. Launch `Inline.exe` directly into your Windows System Tray.

---

## Manual Build & Run Instructions

To build and run manually using the command line:

```cmd
:: 1. Open the "x64 Native Tools Command Prompt for VS 2022"
:: 2. Navigate to your cloned repository folder:
cd C:\path\to\Inline-mac-c

:: 3. Configure Release build (pass your Qt path to CMAKE_PREFIX_PATH if not on PATH):
cmake -B build_win -DBUILD_WINDOWS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.6.2\msvc2022_64"

:: 4. Compile the Release binary:
cmake --build build_win --config Release -j%NUMBER_OF_PROCESSORS%

:: 5. Run the test suite:
ctest --test-dir build_win --output-on-failure

:: 6. Deploy Qt dependencies:
windeployqt build_win\gui\Release\Inline.exe

:: 7. Launch Inline:
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

To terminate running instances and remove build artifacts:
```cmd
taskkill /IM Inline.exe /F 2>nul
rmdir /s /q build_win
```

---

## License

This project is licensed under the MIT License.
