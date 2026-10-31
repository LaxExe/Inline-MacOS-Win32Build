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
- [Complete System Architecture & How It Works](#complete-system-architecture--how-it-works)
  - [High-Level Architecture Diagram](#high-level-architecture-diagram)
  - [Pipeline 1: Sub-Microsecond Keystroke Interception & Security Gating](#pipeline-1-sub-microsecond-keystroke-interception--security-gating)
  - [Pipeline 2: Universal Multi-Tier Caret Localization & Blink Sync](#pipeline-2-universal-multi-tier-caret-localization--blink-sync)
  - [Pipeline 3: Dual-Tier Hybrid Prediction Pipeline](#pipeline-3-dual-tier-hybrid-prediction-pipeline)
  - [Pipeline 4: Ghost Text Layout, Typography & Sub-Pixel Rendering](#pipeline-4-ghost-text-layout-typography--sub-pixel-rendering)
  - [Pipeline 5: Tier 1 Autocorrect Engine & Language Modeling](#pipeline-5-tier-1-autocorrect-engine--language-modeling)
  - [Pipeline 6: Single-Key Undo State Machine](#pipeline-6-single-key-undo-state-machine)
  - [Pipeline 7: Out-of-Process Neural Service & IPC](#pipeline-7-out-of-process-neural-service--ipc)
  - [Pipeline 8: Cross-Platform Abstraction Layer](#pipeline-8-cross-platform-abstraction-layer)
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

## Complete System Architecture & How It Works

Inline is architected as an ultra-low-latency, zero-telemetry native OS service. The entire engine is designed around one strict requirement: **never degrade the user's typing experience**. Every keystroke evaluation, caret localization probe, and layout calculation completes well within the human tactile threshold ($\le 2\,\mu\text{s}$ interception, $< 100\,\mu\text{s}$ Tier 0 prediction, ~15 ms neural model inference).

---

### High-Level Architecture Diagram

```
+====================================================================================================+
|                                    OPERATING SYSTEM KERNEL                                         |
+====================================================================================================+
                                                  |
                         Hardware Keystrokes      | (Raw Scan Codes & Modifier Bits)
                                                  v
+====================================================================================================+
|                              PIPELINE 1: LOW-LATENCY KEYBOARD TAP                                  |
|   macOS: CGEventTap (kCGSessionEventTap)  |  Windows: WH_KEYBOARD_LL (Low-Level Keyboard Hook)     |
+====================================================================================================+
                                                  |
                                                  | Latency <= 2 microseconds
                                                  v
                     +-------------------------------------------------------+
                     |                 Synthetic Loopback Gate               |
                     |  Rejects injected synthetic keystrokes (Event Source) |
                     +-------------------------------------------------------+
                                                  |
                                                  v
                     +-------------------------------------------------------+
                     |             Security & Password Gatekeeper            |
                     |  Suspends on AXSecureTextField, PINs, or blacklists   |
                     +-------------------------------------------------------+
                                                  |
                                                  | Active typing stream
                                                  v
+====================================================================================================+
|                              MAIN TRAY APPLICATION ORCHESTRATOR                                    |
|                             (Lock-Free Ephemeral Word Buffer)                                      |
+====================================================================================================+
             |                                                  |
             | On Word Delimiter (Space)                        | On Every Character Typed
             v                                                  v
+-------------------------------+             +====================================================+
|  PIPELINE 5: TIER 1 ENGINE    |             |        PIPELINE 2: UNIVERSAL CARET TRACKER         |
|  - SymSpell 82K Frequency Dict|             |  - Multi-tier macOS Accessibility (AX) Ladder      |
|  - 243K Bigram Context Matrix |             |  - Chromium Selection Sync Probing (effectiveLoc)  |
|  - Damerau-Levenshtein Dist 2 |             |  - Single-line search bar vertical baseline center |
|  - Run-together word splitting|             |  - Forward monotonicity coordinate lock            |
|  - Sentence auto-capitalization             +====================================================+
+-------------------------------+                                       |
             |                                                          | Exact (X, Y, LineH, Font)
             v                                                          v
+-------------------------------+             +====================================================+
|   PIPELINE 6: UNDO STATE      |             |     PIPELINE 3: DUAL-TIER PREDICTIVE ENGINE        |
|  - Revert on single Backspace |             |                                                    |
|  - Restores exact raw input   |             |  [Tier 0: Sub-Millisecond N-Gram / Bigram Graph]   |
|  - Zero cursor shift          |             |  - Trie prefix lookup (< 100 microseconds)         |
+-------------------------------+             |  - One Word mode filter (active word ending)       |
             |                                |                                                    |
             v                                |  [Tier 1: Out-of-Process Neural Helper (IPC)]      |
+-------------------------------+             |  - SmolLM2-135M (Q8_0) quantized model             |
|   SYNTHETIC TEXT INJECTOR     |             |  - llama.cpp inference on 2 CPU efficiency cores   |
|  - Atomic Backspaces          |             |  - Prefix KV-cache reuse & token healing           |
|  - Replacement string emit    |             +====================================================+
+-------------------------------+                                       |
                                                                        | Suggested continuation
                                                                        v
                                              +====================================================+
                                              |        PIPELINE 4: SUB-PIXEL GHOST OVERLAY         |
                                              |  - Borderless click-through NSPanel                |
                                              |  - Sub-pixel CoreText typography matching host app |
                                              |  - advanceLayout: < 0.2 ms type-through shift      |
                                              |  - Exact vertical baseline alignment (+- 0.5 pt)   |
                                              |  - Smart word-boundary leading space preservation  |
                                              |  - Swallows Tab to accept continuation             |
                                              +====================================================+
```

---

### Pipeline 1: Sub-Microsecond Keystroke Interception & Security Gating

1. **Native Hook Mechanism**:
   - **macOS**: Installed at `kCGSessionEventTap` using `kCGHeadInsertEventTap` and listening for `kCGEventKeyDown`. Inspection takes $\le 2\,\mu\text{s}$ per key on the dedicated event dispatch loop.
   - **Windows**: Handled via `SetWindowsHookExW(WH_KEYBOARD_LL, ...)`, processing low-level `KBDLLHOOKSTRUCT` structures.
2. **Synthetic Loopback Barrier**:
   - When Inline replaces a typo or inserts a prediction, it emits synthetic keystrokes. To prevent recursive feedback loops, synthetic events carry an identification signature (on macOS via `kCGEventSourceUserData` or `CGEventGetIntegerValueField(kCGEventSourceStateCombinedSessionState)`; on Windows via `LLKHF_INJECTED`). Inline discards these immediately.
3. **Security & Privacy Gating**:
   - Inline checks the focused UI element's accessibility attributes (`kAXRoleAttribute` and `kAXSubroleAttribute`).
   - If the element is an `AXSecureTextField`, password input, credit card field, or PIN box, Inline enters a dormant bypass state: no buffer is maintained, no predictions are generated, and all events pass through unmodified.
   - Active process bundle IDs are evaluated against a user-configurable exclusion list.
   - **Strict Zero-Disk Policy**: Keystrokes are held exclusively in ephemeral RAM during the active word session and immediately overwritten. Zero keystroke history is ever written to disk.

---

### Pipeline 2: Universal Multi-Tier Caret Localization & Blink Sync

Ghost text must sit precisely at the insertion point across native AppKit editors, Chromium browsers, Electron wrappers, and web search boxes. Inline implements a 4-tier Accessibility (AX) ladder with sub-pixel localization:

```
+-----------------------------------------------------------------------------------------+
|                              CARET LOCALIZATION DECISION LADDER                         |
+-----------------------------------------------------------------------------------------+
                                             |
                                             v
                           +-----------------------------------+
                           | Tier 1: Native AppKit Text Range  |
                           |   kAXSelectedTextRangeAttribute   |
                           |   kAXBoundsForRangeParameterized  |
                           +-----------------------------------+
                                             |
                                             | If Chromium / Blink web input
                                             v
                           +-----------------------------------+
                           | Tier 2: End-of-Text Range Ladder  |
                           |   Probe effectiveLoc = fullStr.len|
                           |   Range(loc-1, 1) & Range(loc, 0) |
                           +-----------------------------------+
                                             |
                                             | If custom rich editor
                                             v
                           +-----------------------------------+
                           | Tier 3: WebKit TextMarker Range   |
                           |   AXSelectedTextMarkerRange       |
                           |   Ancestor traversal (depth <= 8) |
                           +-----------------------------------+
                                             |
                                             | If AX coordinates unsupported
                                             v
                           +-----------------------------------+
                           | Tier 4: Fallback 7c Search Center |
                           |   Single-line baseline center     |
                           |   Font-scaled text offset         |
                           +-----------------------------------+
```

- **Chromium Selection Delay Resolution (`effectiveLoc`)**:
  - In Chromium and Blink web inputs (Google search, YouTube search, web `<input>` / `<textarea>`), Blink's `kAXSelectedTextRangeAttribute` frequently reports `location: 0, length: 0` asynchronously while typing at the end of the input box.
  - Inline queries `kAXValueAttribute` up front to obtain `fullStr`. When `selRange.location == 0` but `fullStr.size() > 0`, Inline probes `effectiveLoc = fullStr.size()` against `kAXBoundsForRangeParameterizedAttribute`. This extracts the ground-truth insertion point at the end of the typed characters, preventing suggestions from sticking at the field start and eliminating typing overlap.
- **Single-Line Baseline Centering**:
  - In web search bars (`fieldFrame.size.height <= 72.0`), if an accessibility query returns coordinates hugging the container's top border (`caretRect.origin.y < fieldCenterY - 6.0`), Inline automatically aligns to the centered line baseline:
    $$\text{baseline } Y = \text{fieldFrame}.y + \frac{\text{fieldFrame}.h - \text{caretRect}.h}{2} - 1.5\,\text{pt}$$
- **Forward Monotonicity Enforcement**:
  - While typing on the same line, `caret.x` is clamped so it never moves backwards behind `minExpectedX`. If an asynchronous accessibility frame reports a stale position, Inline pauses for up to 9 ms and re-queries to ensure lockstep motion with the cursor.

---

### Pipeline 3: Dual-Tier Hybrid Prediction Pipeline

Inline couples instant deterministic lookups with deep local neural inference:

| Layer | Engine | Latency | Scope | Execution |
| :--- | :--- | :--- | :--- | :--- |
| **Tier 0** | Unigram & Bigram Graph | $< 100\,\mu\text{s}$ | Word completions, 2-word idioms | Main thread in-process |
| **Tier 1** | `SmolLM2-135M (Q8_0)` | ~15 ms | Full sentence continuations, context completions | Out-of-process helper (`InlinePredictor`) |

1. **Tier 0 (Instant N-Gram & Prefix Graph)**:
   - When a letter is typed, the active word prefix is evaluated against an in-memory prefix trie derived from 82,765 English unigrams and 243,342 bigrams.
   - Returns instantaneous suggestions within microseconds without spawning threads or performing allocations.
   - In **One Word Mode**, suggestions are clamped strictly to the active word boundary (`tha` $\rightarrow$ `nk`), preventing sentence chaining when the user only wants word autocompletion.
2. **Tier 1 (On-Device Neural Model)**:
   - Bundles `SmolLM2-135M (Q8_0)` running via `llama.cpp` locally on 2 Apple Silicon efficiency CPU threads.
   - Reuses prefix Key-Value (KV) cache entries across typing keystrokes, avoiding redundant context re-evaluation.
   - Employs **Token Healing** (`TokenHealing`): If the user's cursor is halfway through a word (e.g. `unneces`), token healing backs up to the last clean token boundary, generates the continuation, and strips the redundant prefix to prevent token fragmentation errors.
3. **Safety & Filter Barrier (`SuggestionFilter`)**:
   - Scrubbing blocks profanity (unless already present in user context) and strictly suppresses PII patterns (social security numbers, credit cards, telephone numbers, emails).

---

### Pipeline 4: Ghost Text Layout, Typography & Sub-Pixel Rendering

1. **Window Architecture**:
   - Rendered using an `NSPanel` on macOS (`WS_EX_LAYERED | WS_EX_TRANSPARENT` on Windows) at `NSFloatingWindowLevel`.
   - Attributes: borderless, non-activating (`NSWindowStyleMaskBorderless`), transparent background, non-movable, click-through (`ignoresMouseEvents = YES`), and joining all virtual spaces (`NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary`).
2. **CoreText Multi-Line Typography**:
   - Measures and renders text using native CoreText / DirectWrite font matching.
   - Baseline vertical positioning is aligned to within $\pm 0.5\,\text{pt}$ of the host application's active line.
   - Dynamic proportional font size scaling:
     - Search bars and tall inputs: `15.0–16.0pt` (matching modern 16px web search font metrics).
     - Standard text inputs: clamped to a minimum of `14.0pt`, strictly preventing microscopic 11pt/12pt superscript rendering.
3. **Sub-Millisecond Type-Through Shift (`advanceLayout`)**:
   - As the user types letters that match the ghost suggestion, `GhostOverlay::advanceLayout` calculates the accepted character width and translates the overlay window frame to the right on the main thread in $< 0.2\,\text{ms}$.
   - The suggestion moves in lockstep with the host application's blinking cursor bar without redrawing or waiting for accessibility tree synchronization.
4. **Smart Word-Boundary Spacing**:
   - If the cursor rests against a completed word (`afterWordChar == true`) and the suggestion is a next-word continuation, exactly one leading space is strictly preserved (`hello| there`), eliminating visual and injection collisions.
   - If the document already ends with whitespace, leading spaces are automatically stripped to prevent accidental double spaces.
5. **Seamless Tab Chaining**:
   - In One Word Mode, accepting a word with `Tab` immediately advances the layout and reveals the next word of the sentence continuation without hiding or flickering the overlay window.

---

### Pipeline 5: Tier 1 Autocorrect Engine & Language Modeling

1. **Dictionary & Distance Metric**:
   - Built on an optimized C++ implementation of `SymSpell` with precomputed delete hashes for Damerau-Levenshtein distance $\le 2$.
   - Evaluates word candidates in $< 100\,\mu\text{s}$ over 82,765 unigram frequencies.
2. **Context-Aware Bigram Disambiguation**:
   - Evaluates the preceding word context using 243,342 real-world bigram pairs.
   - Resolves homophones and common confusion pairs (e.g. `there` vs. `their`, `your` vs. `you're`, `its` vs. `it's`).
3. **Run-Together Word Splitting**:
   - Employs dynamic programming to split accidentally concatenated words (`whenI` $\rightarrow$ `when I`, `twowords` $\rightarrow$ `two words`, `lookat` $\rightarrow$ `look at`) by scoring token combinations against the frequency dictionary.
4. **Automatic Sentence Capitalization**:
   - Detects sentence starts at line beginnings and after terminal punctuation (`.`, `!`, `?`), automatically converting lowercase words to uppercase initial characters, plus standalone `i` $\rightarrow$ `I`.
5. **Case Preservation Matrix**:
   - Automatically detects and replicates the user's typing case pattern:
     - Lowercase: `teh` $\rightarrow$ `the`
     - Title Case: `Teh` $\rightarrow$ `The`
     - Uppercase: `TEH` $\rightarrow$ `THE`
     - Mixed / CamelCase: untouched

---

### Pipeline 6: Single-Key Undo State Machine

Inline features a dedicated finite state machine (`UndoStateMachine`) that eliminates the frustration of fighting with aggressive autocorrect:

```
                            [ Idle ]
                               |
                               | User types character
                               v
                           [ Typing ]
                               |
               +---------------+---------------+
               |                               |
               | Delimiter (Space)             | Tab (Ghost Accept)
               v                               v
     [ Autocorrected ]                 [ AcceptedGhost ]
               |                               |
               | Backspace immediately pressed | Backspace immediately pressed
               v                               v
     [ Revert Correction ]             [ RevertGhostAccept ]
               |                               |
               +---------------+---------------+
                               |
                               | Atomic Keystroke Reversion
                               v
                     [ Raw Keystrokes Restored ]
```

- When an autocorrection or Tab accept occurs, the engine transitions to `Autocorrected` or `AcceptedGhost`, preserving the exact raw keystroke string in an ephemeral state record.
- If the user's very next keystroke is **Backspace**, Inline intercepts the key, synthesizes the exact sequence of backspaces to remove the correction, and injects the original raw typed characters.
- **Enter / Return Pass-Through**: Pressing `Enter` or `Return` (`\n`) **never autocorrects**. It commits whatever raw text the user typed, clears the active buffer, and passes the Return key straight to the application.

---

### Pipeline 7: Out-of-Process Neural Service & IPC

To prevent neural model evaluation from ever blocking the real-time keyboard interception loop:

1. **Process Isolation**:
   - `Inline.app` runs as the lightweight UI and hook process.
   - `InlinePredictor` runs as an independent out-of-process helper process.
2. **IPC Protocol**:
   - Communicates over a private local Unix domain socket using length-prefixed binary message framing.
   - Requests carry an atomic monotonic generation counter (`generation_`).
   - If the user types a new character while an inference task is running in the background, the generation counter increments, and the helper or main thread immediately discards the obsolete response without rendering.
3. **Thermal & Battery Regulation (`PowerMonitor`)**:
   - Continuously inspects macOS thermal state (`NSProcessInfoThermalState`) and Low Power Mode.
   - If the system enters Low Power Mode or thermal state `Serious` / `Critical`, the neural helper is suspended automatically, allowing Tier 0 N-grams to handle all predictions with zero battery penalty.

---

### Pipeline 8: Cross-Platform Abstraction Layer

The repository maintains strict architectural separation between core algorithmic logic and OS platform hooks:

```
Inline-macOS-Win32/
├── core/                           # Platform-Agnostic Engine (C++20)
│   ├── tier1/                      # SymSpell, Word Boundary, Undo State Machine
│   ├── predict/                    # NgramPredictor, SuggestionFilter, TokenHealing
│   ├── ghost/                      # GhostSession, GhostLayout, Layout Math
│   └── config/                     # AppConfig, JSON Persistence
├── platform/
│   ├── mac/                        # macOS Platform Layer (Objective-C++ / AppKit)
│   │   ├── caret/                  # MacCaretTracker (AX Ladder, Blink Sync)
│   │   ├── ghost/                  # GhostOverlay (NSPanel, CoreText)
│   │   ├── hook/                   # MacKeyboardHook (CGEventTap)
│   │   ├── injector/               # MacTextInjector (CGEventPost)
│   │   └── power/                  # PowerMonitor (IOPowerSources, Thermal)
│   └── win/                        # Windows Platform Layer (Win32 API)
│       ├── caret/                  # WinCaretTracker (UI Automation)
│       ├── ghost/                  # WinGhostOverlay (Layered Transparent Window)
│       └── hook/                   # WinKeyboardHook (WH_KEYBOARD_LL)
├── predictor_service/              # Standalone Neural Helper (llama.cpp)
└── gui/                            # Native System Settings UI (Qt6)
```

- **Branch Structure**:
  - `mac-branch`: Primary macOS branch with full Apple Silicon & Intel optimizations.
  - `windows-branch`: Native Windows implementation with Win32 hooks and UI Automation.
  - `gh-pages`: Live interactive documentation and web demo.

---

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

Outputs compressed `dist/Inline-v1.3-macOS.dmg`.

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
