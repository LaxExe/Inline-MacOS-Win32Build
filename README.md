# Inline (macOS)

A high-performance, ultra-lightweight native macOS menu bar autocorrect engine built in C++17 and Objective-C++. **Inline** provides instant, sub-millisecond typo correction, run-together word splitting, and smart single-key undo recovery directly across all macOS applications with minimal CPU and memory footprint (<5 MB RAM, ~0.0% idle CPU).

---

## Features

- ⚡ **Ultra-Lightweight & Sub-Millisecond Autocorrection**: Instant unigram dictionary lookups and frequency-weighted SymSpell candidate ranking with zero background overhead.
- 🧩 **Run-Together Word Splitting**: Automatically detects and splits concatenated words (e.g. `theclass` → `the class`, `ofevening` → `of evening`, `timerighte` → `time right`).
- 🔄 **Single-Key Revert on Backspace**: If a word is autocorrected and you press Backspace, Inline seamlessly reverts the word back to your exact typed input.
- 🧹 **Automatic & Manual Buffer Purging**: Automatically clears all keystroke buffers after 30 seconds of inactivity, on menu bar interaction, via manual menu action, and on application quit.
- 🛡️ **Zero Keystroke Dumps on Teardown**: Ensures clean event queue draining and immediate `CGEventTap` disabling on quit, preventing any buffered keystrokes from dumping to active apps.
- 🔒 **Privacy & Secure Field Protection**: Password and credential fields are automatically excluded from logging, buffering, and corrections.
- 🎛️ **Native macOS Menu Bar App**: Sits discreetly in the macOS menu bar with real-time status indication, live test playground, and hotkey controls (⌘⌥⇧K).
- 🧪 **100% Test Coverage**: Complete GoogleTest unit and integration test suite covering hooks, injectors, buffers, priority queues, and undo state machines.

---

## Architecture

```
                                  +-----------------------+
                                  |   macOS Event Tap     |
                                  | (kCGSessionEventTap)  |
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
- **`gui`**: Native macOS status bar tray item (`QSystemTrayIcon`), settings panel, live test playground, and login item manager.

---

## Requirements

- **macOS**: 12.0 (Monterey) or later (Apple Silicon & Intel supported)
- **Compiler**: Apple Clang supporting C++17
- **Build System**: CMake 3.20+
- **macOS Permissions**: 
  - **Accessibility** (`AXIsProcessTrusted`)
  - **Input Monitoring** (for global `CGEventTap`)

---

## Building and Running

### 1. Build from Source

```bash
# Clone the repository
git clone https://github.com/LaxExe/Inline-mac-c.git
cd Inline-mac-c

# Configure and build Release bundle
cmake -B build_release -DCMAKE_BUILD_TYPE=Release
cmake --build build_release -j8
```

### 2. Run the Test Suite

```bash
ctest --test-dir build_release --output-on-failure
```

### 3. Launch the Application

```bash
open build_release/gui/TypingAssistant.app
```

On first launch, macOS will prompt for **Accessibility** and **Input Monitoring** permissions in *System Settings → Privacy & Security*.

---

## License

This project is licensed under the MIT License.
