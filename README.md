# Inline (macOS)

A high-performance, native macOS menu bar typing assistant built in C++17 and Objective-C++. **Inline** provides real-time, low-latency autocorrection, run-together word splitting, smart typo recovery, and optional predictive ghost text directly within macOS applications.

---

## Features

- ⚡ **Sub-Millisecond Autocorrection (Tier 1)**: Instant dictionary lookups and frequency-weighted SymSpell candidate ranking for common typos, transpositions, and dropped characters.
- 🧩 **Run-Together Word Splitting**: Automatically detects and splits concatenated words (e.g. `theclass` → `the class`, `ofevening` → `of evening`).
- 🔄 **Character-Accurate Stream & Single-Key Undo**: Maintains exact synchronization with your screen buffer. Pressing Backspace immediately reverts any auto-correction back to your exact typed input.
- 🛡️ **Zero Event Dump on App Teardown**: Uses a persistent, reusable session event source (`kCGEventSourceStateCombinedSessionState`) posting to `kCGSessionEventTap` to guarantee no queued characters or Mach port leaks when closing the app.
- 🔒 **Privacy & Secure Field Suppression**: Automatically detects password and credential fields via macOS Accessibility (`AXUIElement`) and suppresses all hooking and buffer collection.
- 🎛️ **Native Cocoa Menu Bar App**: Sits discreetly in the macOS menu bar with real-time status indication, preferences dialog, engine mode toggles, and hotkey controls.
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
+-------------------------+       +-----------+-----------+
|  Accessibility Caret    |       |   Synthetic Filter    |  (Rejects injected loopbacks)
|  & Secure Classifier    |<------+-----------------------+
+------------+------------+                   |
             |                                v
             |                    +-----------+-----------+
             |                    |   Text Stream Buffer  |  (Accurate char-level history)
             |                    +-----------+-----------+
             |                                |
             |                                v
             |                    +-----------+-----------+
             |                    |    Tier 1 Corrector   |  (SymSpell 82K + Expansions)
             |                    +-----------+-----------+
             |                                |
             |                                v
             |                    +-----------+-----------+
             |                    |    Injection Queue    |  (Atomic REPLACE_TEXT)
             |                    +-----------+-----------+
             |                                |
             |                                v
             |                    +-----------+-----------+
             +------------------->|     Ghost Overlay     |  (Optional inline predictions)
                                  +-----------------------+
```

### Key Modules

- **`core/tier1`**: Damerau-Levenshtein distance calculation, 82,000+ word unigram frequency dictionary, common English contraction expansions, and run-together compound splitter.
- **`core/buffers`**: Continuous character history stream, word boundary segmentation, and trailing-word extraction.
- **`core/injection`**: Priority queue for injection tokens, atomic text replacement engine, and event pacing.
- **`core/undo`**: Deterministic state machine tracking replacement history for single-keystroke undo operations.
- **`core/reliability`**: Circuit breaker monitoring injection latency, error rates, and automatic fallback modes.
- **`platform/mac`**: Native macOS hooks (`CGEventTapCreate`), event synthesis (`CGEventPost`), AX caret geometry tracking (`AXUIElementCopyAttributeValue`), and overlay windows.
- **`gui`**: Native macOS status bar tray item (`NSStatusBar`), settings panel, and login item manager.

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

## Configuration

Settings are saved to `~/Library/Application Support/TypingAssistant/config.json`:

```json
{
  "engineMode": 0,
  "maxDictionaryEditDistance": 2,
  "sentenceCorrectionEnabled": false,
  "ghostTextEnabled": false,
  "ghostTextDelayMs": 250,
  "maxBufferTokens": 100,
  "ignoreWords": []
}
```

---

## License

This project is licensed under the MIT License.
