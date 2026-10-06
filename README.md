# ERSA — Dual-VCO Vintage Polysynth (VST3 + Standalone)

> **Disclaimer:** this project is vibe-coded for personal use. It is provided
> as-is, without warranty of any kind. The author is not responsible for
> anything — Use at your own risk.

ERSA is an 8-voice dual-oscillator analog-style polysynth with a vintage
oscillator → mixer → filter → amp signal flow, an extended effects section
(chorus, phaser, tempo-syncable delay, modulated algorithmic reverb,
tape/tube saturator, EQ), an arpeggiator with host-synced divisions, per-voice
analog character (tolerances, drift, free-running oscillators), resonant
low-pass filter (12/24 dB), 256 factory presets in two banks (editable data
files, never compiled in), user preset save/load, and a full preset browser with search.

## License

GPL version 3 — see [LICENSE](LICENSE). This also satisfies the JUCE
framework's licensing terms for this project.

Embedded typefaces (Barlow, Inter, Lato — since replaced across versions) are
SIL Open Font License 1.1; see [Assets/OFL-NOTICE.txt](Assets/OFL-NOTICE.txt).

## Building

Requirements: CMake 3.22+, a C++17 compiler, Ninja (or Make), Git (JUCE is
fetched automatically), and JUCE's Linux dependencies (ALSA, X11, freetype,
etc.) — see the JUCE docs for the full list.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Outputs:

- VST3: `build/ERSA_artefacts/Release/VST3/ERSA.vst3` (+ factory presets under
  `Contents/Resources/Factory/`)
- Standalone: `build/ERSA_artefacts/Release/Standalone/ERSA`
- Headless self-test: `build/ErsaHarness_artefacts/Release/ErsaHarness`
  (runs the full DSP/UI regression suite; exit 0 = all good)

Install the VST3 by copying `ERSA.vst3` to your plugin folder:

| OS      | VST3 folder                          | Notes                                        |
|---------|--------------------------------------|----------------------------------------------|
| Linux   | `~/.vst3/`                           | Rescan plugins in your DAW afterwards        |
| macOS   | `~/Library/Audio/Plug-Ins/VST3/`     | Unsigned build: right-click → Open once      |
| Windows | `C:\Program Files\Common Files\VST3\`| Rescan plugins in your DAW afterwards        |

User presets live in `Documents/ERSA Presets/` (created on first save);
factory overwrites shadow shipped files from `Documents/ERSA Presets/Factory/`
without ever touching the install bundle.

### OS-specific notes

> Only the Linux build has actually been compiled and tested here. The macOS
> and Windows steps below follow the standard JUCE/CMake flow and *should*
> work, but if you hit an OS-specific snag, please file an issue with the
> failing command and its full output.

#### Linux (verified)

1. Install dependencies (Debian/Ubuntu shown; Fedora/Arch: equivalent
   `-devel` packages):
   ```sh
   sudo apt install build-essential cmake ninja-build git pkg-config \
     libasound2-dev libx11-dev libxcomposite-dev libxcursor-dev \
     libxinerama-dev libxrandr-dev libfreetype6-dev libfontconfig1-dev \
     libcurl4-openssl-dev libwebkit2gtk-4.1-dev
   ```
2. Configure + build (JUCE is fetched automatically — needs network access):
   ```sh
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```
3. Run the self-test (must print `ALL OK`):
   ```sh
   ./build/ErsaHarness_artefacts/Release/ErsaHarness
   ```
4. Install: copy `build/ERSA_artefacts/Release/VST3/ERSA.vst3` to `~/.vst3/`
   (factory presets travel inside the bundle), then rescan plugins in your DAW.

#### macOS (not yet built here)

1. Install Xcode command-line tools, CMake, Ninja and Git:
   ```sh
   xcode-select --install
   brew install cmake ninja git
   ```
2. Same configure + build as Linux. For a universal (Intel + Apple Silicon)
   binary, add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` to the configure step.
3. Run `./build/ErsaHarness_artefacts/Release/ErsaHarness` — expect `ALL OK`.
4. Install: copy `ERSA.vst3` to `~/Library/Audio/Plug-Ins/VST3/`. These are
   unsigned personal builds: if macOS refuses to load them, ad-hoc sign with
   `codesign --force --deep -s - <path>` and/or strip the quarantine flag
   with `xattr -dr com.apple.quarantine <path>`. Codesigning/notarisation for
   distribution is out of scope for this project.

#### Windows (not yet built here)

1. Install Visual Studio 2022 with the **Desktop development with C++**
   workload (MSVC v143 or newer), plus CMake, Ninja and Git
   (`winget install Kitware.CMake Ninja-build.Ninja Git.Git` works).
2. Open an **x64 Native Tools Command Prompt** (so MSVC is on `PATH`), then
   the same configure + build commands as Linux. Alternatively use the
   Visual Studio generator instead of Ninja:
   ```bat
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ```
3. Run `build\ErsaHarness_artefacts\Release\ErsaHarness.exe` — expect `ALL OK`.
4. Install: copy `ERSA.vst3` to `C:\Program Files\Common Files\VST3\`, then
   rescan plugins in your DAW. No ASIO SDK or extra setup required.

## Repository layout

- `Source/` — plugin DSP, UI, preset backend
  (`DSPEngine.h` voices/filters/FX, `PluginProcessor.*`, `PluginEditor.*`,
  `PresetBrowser.*`, `Parameters.h`, `ErsaLookAndFeel.*`)
- `Source/Factory/*.xml` — the 256 factory presets (data, editable by hand)
- `Assets/` — embedded fonts + OFL notice
- `Test/Harness.cpp` — headless regression suite (see `CHECKS.md` for coverage)
- `CHECKS.md` — hard-won checklist: every bug class found during development
- `LESSONS.md` — the full issue→fix log behind that checklist
