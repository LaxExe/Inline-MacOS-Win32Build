# Inline (macOS)

A high-performance, ultra-lightweight native macOS menu bar autocorrect engine built in C++20, Qt6, and native macOS APIs (`CGEventTap`). **Inline** provides instant, sub-millisecond typo correction, context-aware disambiguation, run-together word splitting, and smart single-key undo recovery directly across all macOS applications with minimal CPU and memory footprint (<5 MB RAM, ~0.0% idle CPU).

> **Looking for Windows?** The native Windows version (`WH_KEYBOARD_LL`, `SendInput`) is maintained on the [**`windows-branch`**](https://github.com/LaxExe/Inline-mac-c/tree/windows-branch).

---

## Download

[**Download Inline for macOS (.dmg)**](https://github.com/LaxExe/Inline-mac-c/releases/latest)

---

## Features

- ⚡ **Ultra-Lightweight & Sub-Millisecond Autocorrection**: Instant unigram dictionary lookups and frequency-weighted SymSpell candidate ranking with zero background overhead (<100 microseconds per query).
- 🧠 **Context-Aware Bigram Disambiguation**: Intelligently scores candidates against preceding conversational context via a 240,000+ bigram corpus (e.g. typing `helo` naturally becomes `help` after "need" or "please", but `hello` after "say" or at the start of a sentence).
- 🧩 **Run-Together Word Splitting**: Automatically detects and splits concatenated words (e.g. `theclass` → `the class`, `ofevening` → `of evening`, `timerighte` → `time right`).
- 🔄 **Single-Key Revert on Backspace**: If a word is autocorrected and you press Backspace, Inline seamlessly reverts the word back to your exact typed input.
- 🔐 **First-Run Permission Onboarding**: Interactive permission setup window guiding you through Input Monitoring and Accessibility with live verification.
- 🧹 **Automatic & Manual Buffer Purging**: Automatically clears all keystroke buffers after 30 seconds of inactivity, on menu bar interaction, via manual menu action, and on application quit.
- 🛡️ **Zero Keystroke Dumps on Teardown**: Ensures clean event queue draining and immediate hook disabling on quit, preventing any buffered keystrokes from dumping to active apps.
- 🔒 **Privacy & Secure Field Protection**: Password and credential fields are automatically excluded from logging, buffering, and corrections.
- 🎛️ **Minimal Native Menu Bar UI**: Sits discreetly in the macOS Menu Bar with a clean status indicator, live testing playground, and hotkey controls.
- 🧪 **100% Test Coverage**: Complete GoogleTest unit test suite covering hooks, injectors, buffers, priority queues, and undo state machines.

---

<details>
<summary><b>Build from Source</b></summary>

### Prerequisites
```bash
xcode-select --install
brew install cmake qt6
```

### Build & Run
```bash
git clone https://github.com/LaxExe/Inline-mac-c.git
cd Inline-mac-c
git checkout mac-branch
chmod +x install_mac.sh
./install_mac.sh
```

</details>

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
