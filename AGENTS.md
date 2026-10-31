# Inline (macOS) - AI Agent Guide & Command Reference

This file serves as the definitive reference for AI assistants working on this codebase. It documents all commands, workflows, design constraints, and architectural guidelines for **Inline**.

---

## 1. Quick Command Cheat Sheet

### Build & Run
| Action | Command |
| :--- | :--- |
| **One-Click Build & Install** | `./install_mac.sh` |
| **Configure Build** | `cmake -B build_release -DCMAKE_BUILD_TYPE=Release -G Ninja` |
| **Compile App Bundle** | `cmake --build build_release --target Inline` |
| **Codesign Bundle** | `codesign --force --deep --sign - -i com.inline.mac build_release/gui/Inline.app` |
| **Copy to /Applications** | `cp -R build_release/gui/Inline.app /Applications/` |
| **Launch App** | `open /Applications/Inline.app` |

### Testing & Benchmarking
| Action | Command |
| :--- | :--- |
| **Run All Unit Tests** | `ctest --test-dir build_release --output-on-failure` |
| **Run Tier 1 Autocorrect Tests** | `./build_release/tests/test_tier1` |
| **Run Settings & Toggle Tests** | `./build_release/tests/test_settings_binding` |
| **Run Word Boundary Tests** | `./build_release/tests/test_word_boundary` |
| **Run Undo State Machine Tests** | `./build_release/tests/test_undo_state_machine` |
| **Run Ghost Text Unit Tests** | `./build_release/tests/test_ghost_text` |
| **Run Performance Benchmark** | `./build_release/bench_corrector` |
| **Run Predictor Benchmark** | `./build_release/bench_predictor` |

### Packaging & Releasing
| Action | Command |
| :--- | :--- |
| **Build macOS DMG Installer** | `./tools/create_dmg.sh` *(generates `dist/Inline-vX.X-macOS.dmg`)* |
| **Mount & Test DMG** | `hdiutil attach dist/Inline-v1.1-macOS.dmg` |
| **Unmount DMG** | `hdiutil detach /Volumes/Inline` |
| **Reset macOS TCC Cache** | `tccutil reset Accessibility com.inline.mac && tccutil reset ListenEvent com.inline.mac` |

---

## 2. How to Release a New Version (e.g. v1.2)

### Automated GitHub Release Workflow:
1. Update version strings:
   - [`Info.plist`](Info.plist): Update `CFBundleShortVersionString` and `CFBundleVersion` to `1.2.0`.
   - [`gui/settings_dialog.cpp`](gui/settings_dialog.cpp): Update version label (`Inline macOS • v1.2`).
2. Commit and push:
   ```bash
   git add -A
   git commit -m "Release v1.2"
   git push origin mac-branch
   ```
3. Tag and push tag:
   ```bash
   git tag -a v1.2 -m "Release v1.2"
   git push origin v1.2
   ```
4. GitHub Actions [`.github/workflows/release.yml`](.github/workflows/release.yml) automatically runs on Apple Silicon runners, builds the DMG, and publishes the release on GitHub.

### Local DMG Generation:
```bash
./tools/create_dmg.sh
```
Outputs compressed `dist/Inline-vX.X-macOS.dmg` with `/Applications` drag-and-drop symlink.

---

## 3. Core Architectural Rules & Design Mandates

### 1. Zero Emojis Anywhere
- **Strictly enforce**: Absolutely no emojis in the entire codebase, UI strings, logs, settings dialogs, or onboarding panels.
- All icons must be pure vector outlines rendered via `QPainter` or native AppKit bezier paths.

### 2. Modern macOS System Settings Visual Language
- All dialogs ([`SettingsDialog`](gui/settings_dialog.h), [`PermissionDialog`](gui/permission_dialog.h)) must match Apple's modern System Settings:
  - Rounded card containers (`border-radius: 12px; background: #FFFFFF; border: 1px solid #E5E7EB;`).
  - Monochrome icon badges (`34x34`, background `#F3F4F6`, border `#E5E7EB`, dark ink strokes `#111827`).
  - No amber, orange, or yellow backgrounds on badges.
  - Responsive layout: Text areas (`ignoreWordsEdit_`, `disabledAppsEdit_`, `liveLogEdit_`) must expand dynamically with window resizing (`QSizePolicy::Expanding`).

### 3. Default Engine & Toggle States
- The following features must **always be enabled by default** on initial launch:
  - **Autocorrect Engine** (`masterEnabled = true`)
  - **Ghost Text Predictions** (`ghostTextEnabled = true`, `smartPredictionsEnabled = true`)
  - **Single-Key Undo on Backspace** (`singleKeyUndoEnabled = true`)
  - **Run-Together Word Splitting** (`splitRunTogetherWords = true`)
  - **Sentence Capitalization** (`sentenceCorrectionEnabled = true`)
  - **Launch at Login** (`launchAtLogin = true`)
  - **Menu Bar Icon Visibility** (permanently visible; no toggle switch)

### 4. Adaptive Menu Bar Contrast
- The menu bar icon (`|▪`) must adapt dynamically to the wallpaper and system appearance:
  - **Dark / Black Menu Bar / Dark Space**: Vertical line is **pure white (`#FFFFFF`)** with green dot (`#22C55E`).
  - **Light Menu Bar**: Vertical line is **dark charcoal (`#18181B`)** with green dot (`#22C55E`).
  - Implemented natively via `updateNativeStatusBarIcon()` in [`platform/mac/permissions.mm`](platform/mac/permissions.mm) using `NSImage imageWithSize:flipped:drawingHandler:`.

### 5. Delimiter & Key Behavior
- **Space (` `)**: Triggers word evaluation and autocorrection.
- **Tab (`\t`, `kVK_Tab`)**: If Ghost Text is currently visible, swallows `Tab` and injects the predicted continuation. If no Ghost Text is visible, **NEVER autocorrects** — clears buffer and passes `Tab` straight through to the app.
- **Enter / Return (`\n`, `\r`, `kVK_Return`, keypad enter)**: **NEVER autocorrects**. Commits whatever raw text the user typed, clears buffer, starts sentence flag, and passes the Enter key straight through to the app.
- **Backspace (`kVK_Delete`)**: If pressed immediately following an autocorrection or a Ghost Text accept, reverts the text back to raw keystrokes (Single-Key Undo). If pressed during typing, pops the last character.

### 6. Slang & Jargon Whitelist
- Protect 180+ internet slang acronyms (`gng`, `brb`, `ttyl`, `idk`, `ngl`, `tbh`, etc.) in [`frequency_dictionary_en_82_765.txt`](dictionaries/frequency_dictionary_en_82_765.txt).
- Protect developer terms (`repo`, `dev`, `pr`, `cli`, `sdk`, `api`, `auth`, `async`, etc.) and support context bigrams in [`frequency_bigramdictionary_en_243_342.txt`](dictionaries/frequency_bigramdictionary_en_243_342.txt).

### 7. Performance & Latency Constraints
- Keystroke interception via `CGEventTap` must complete in **$\le 2\,\mu\text{s}$** per keystroke.
- Active memory footprint must remain **$< 120\,\text{MB}$ RSS** and **$\sim 0.0\%$ idle CPU**.
- Never perform disk I/O, regex, or large allocations inside `onRawKeyEvent`.
