# Inline (macOS & Windows)

A high-performance, ultra-lightweight native cross-platform system tray / menu bar autocorrect engine built in C++20, Qt6, and native platform APIs. **Inline** provides instant, sub-millisecond typo correction, run-together word splitting, and smart single-key undo recovery directly across all macOS and Windows applications with minimal CPU and memory footprint (<5 MB RAM, ~0.0% idle CPU).

---

## Features

- ⚡ **Ultra-Lightweight & Sub-Millisecond Autocorrection**: Instant unigram dictionary lookups and frequency-weighted SymSpell candidate ranking with zero background overhead (<100 microseconds per query).
- 🧠 **Context-Aware Bigram Disambiguation**: Intelligently scores candidates against preceding conversational context via a 240,000+ bigram corpus (e.g. typing `helo` naturally becomes `help` after "need" or "please", but `hello` after "say" or at the start of a sentence).
- 🧩 **Run-Together Word Splitting**: Automatically detects and splits concatenated words (e.g. `theclass` → `the class`, `ofevening` → `of evening`, `timerighte` → `time right`).
- 🔄 **Single-Key Revert on Backspace**: If a word is autocorrected and you press Backspace, Inline seamlessly reverts the word back to your exact typed input.
- 🪟 **Windows & macOS Multi-Platform Support**: Modular platform abstraction layer (`platform/mac` & `platform/win`) supporting macOS (`CGEventTap`) and Windows (`SetWindowsHookEx` & `SendInput`).
- 🧹 **Automatic & Manual Buffer Purging**: Automatically clears all keystroke buffers after 30 seconds of inactivity, on menu bar/tray interaction, via manual menu action, and on application quit.
- 🛡️ **Zero Keystroke Dumps on Teardown**: Ensures clean event queue draining and immediate hook disabling on quit, preventing any buffered keystrokes from dumping to active apps.
- 🔒 **Privacy & Secure Field Protection**: Password and credential fields are automatically excluded from logging, buffering, and corrections.
- 🎛️ **Native Tray App**: Sits discreetly in the system tray / menu bar with real-time status indication, live test playground, and hotkey controls.
- 🧪 **Cross-Platform Test Coverage**: Complete GoogleTest unit test suite covering hooks, injectors, buffers, priority queues, and undo state machines.

---

## Architecture

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
                                  |   Tier 1 Corrector    |  (SymSpell 82K + Expansions)
                                  +-----------+-----------+
                                              |
                                              v
                                  +-----------+-----------+
                                  |    Injection Queue    |  (Atomic REPLACE_TEXT)
                                  +-----------+-----------+
```

### Key Modules

- **`core/tier1`**: Damerau-Levenshtein distance calculation, 82,000+ word unigram frequency dictionary, common English contraction expansions, and run-together compound splitter.
- **`core/buffers`**: Ephemeral single-word buffering, delimiter detection, and boundary segmentation.
- **`core/injection`**: Priority queue for injection tokens, atomic text replacement engine, and event pacing.
- **`core/undo`**: Deterministic state machine tracking replacement history for single-keystroke undo operations.
- **`core/reliability`**: Circuit breaker monitoring injection latency, error rates, and automatic fallback modes.
- **`platform/mac`**: Native macOS hooks (`CGEventTapCreate`), event synthesis (`CGEventPost`), and permissions management.
- **`platform/win`**: Native Windows low-level keyboard hooks (`SetWindowsHookEx`), text injection (`SendInput`), global hotkeys (`RegisterHotKey`), and Windows registry startup integration.
- **`gui`**: System tray application (`QSystemTrayIcon`), settings panel, live test playground, and autostart manager.

---

## Requirements

### macOS
- **OS**: macOS 12.0 (Monterey) or later (Apple Silicon & Intel supported)
- **Compiler**: Clang supporting C++20
- **Build System**: CMake 3.20+ & Qt6
- **Permissions**: Accessibility & Input Monitoring

### Windows
- **OS**: Windows 10 / 11 (x64 / ARM64)
- **Compiler**: MSVC 2019+ or MinGW supporting C++20
- **Build System**: CMake 3.20+ & Qt6

---

## Branches & Platform Downloads

| Platform | Recommended Branch | 1-Click Installer | Details |
| :--- | :--- | :--- | :--- |
| **🍎 macOS** | [`mac-branch`](https://github.com/LaxExe/Inline-mac-c/tree/mac-branch) | `./install_mac.sh` | Optimized for macOS (`CGEventTap`, permission onboarding, menu bar) |
| **🪟 Windows** | [`windows-branch`](https://github.com/LaxExe/Inline-mac-c/tree/windows-branch) | `install_win.bat` | Optimized for Windows (`WH_KEYBOARD_LL`, `SendInput`, system tray) |

---

## Quick Install (1-Click)

### 🍎 macOS Quick Install
```bash
git checkout mac-branch
./install_mac.sh
```
This builds the native Release bundle, configures application icons and dictionaries, and automatically installs to `/Applications/Inline.app`.

### 🪟 Windows Quick Install
Double-click `install_win.bat` or run:
```cmd
git checkout windows-branch
install_win.bat
```
*(Or via PowerShell: `.\install_win.ps1`)*

---

## Manual Build Instructions

### macOS Manual Build
```bash
cmake -B build_release -DCMAKE_BUILD_TYPE=Release
cmake --build build_release -j8
ctest --test-dir build_release --output-on-failure
open build_release/gui/TypingAssistant.app
```

### Windows Manual Build
```cmd
cmake -B build_win -DBUILD_WINDOWS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_win --config Release -j8
ctest --test-dir build_win --output-on-failure
build_win\gui\TypingAssistant.exe
```


---

## License

This project is licensed under the MIT License.
