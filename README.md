<p align="center">
  <img src="logo.png" alt="DeadWarrior 2" width="820">
</p>

<p align="center">
  <strong>A native port of Stick War 2. Work in progress.</strong><br>
  C++20 · Windows · macOS · Linux · 16:9 presentation
</p>

<p align="center">
  <a href="https://github.com/JustPatrox/DeadWarrior-2/actions/workflows/windows.yml"><img src="https://github.com/JustPatrox/DeadWarrior-2/actions/workflows/windows.yml/badge.svg" alt="Windows build"></a>
  <img src="https://img.shields.io/badge/status-work_in_progress-e5b344" alt="Work in progress">
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white" alt="C++20">
  <img src="https://img.shields.io/badge/platform-Windows_%7C_macOS_%7C_Linux-0078D4" alt="Windows, macOS and Linux">
</p>

<p align="center">
  <a href="#current-status">Status</a> ·
  <a href="#download-and-play">Download</a> ·
  <a href="#build-from-source">Build</a> ·
  <a href="#pc-controls">Controls</a> ·
  <a href="https://github.com/JustPatrox/DeadWarrior-2/issues/new/choose">Report a bug</a> ·
  <a href="CONTRIBUTING.md">Contribute</a>
</p>

## About

DeadWarrior 2 translates the game's logic into C++ and adapts its presentation to 16:9 displays. Windows uses Direct2D and XAudio2, with a software rendering fallback. macOS and Linux use an SDL 3 frontend with the same C++ campaign simulation. The simulation runs at **30 Hz**, independently of the rendering frame rate.

The downloads are **native, ready-to-play ports**, with prepared assets included. Windows uses `DeadWarrior 2.exe`, macOS uses an app bundle, and Linux uses a native executable. **No SWF, Flash Player, Java, Python, or emulator is needed to play.** The game runs its translated C++ controllers rather than loading the original SWF.

This project is not affiliated with Max Games or the original game's creators. See [credits and third-party content](NOTICE.md).

## Current status

**Development build: the full campaign has not been ported yet.** Ten missions are accessible, through Crawlers Attack. Unimplemented sections display a work-in-progress screen with restart and quit options.

| Feature | Status |
| --- | --- |
| Menus, tutorial, and first ten missions | Implemented; full parity is still under review |
| Campaign progress, upgrades, and saves | Implemented for the current content |
| 16:9 rendering, audio, and contextual cursors | Implemented |
| Windowed, borderless window, and fullscreen modes | Insert on Windows/Linux; Option on macOS |
| 30 / 60 / 120 / 240 FPS or unlimited | Available |
| V-Sync, FPS counter, render resolution, and cursor lock | Available |
| Aspect modes | 16:9 default, Stretched, Dynamic [Experimental] |
| Castle hover, tooltips, and PC toolbar button | Original artwork, font, hit areas, and animation states |
| Original toolbar hover and pressed states | Implemented, including action-slot glow |
| Original campaign pause menu | MENU, Esc, or P |
| Persistent PC settings | Saved in `options.ini` |
| Remaining campaign and cutscenes | Pending |
| macOS and Linux | Native preview builds; platform validation in progress |

Source CI checks Windows compilation and Python tools. The release workflow builds Windows x64, Linux x64, macOS Intel, and macOS Apple Silicon, and runs campaign probes using prepared release assets. Portable packages also run automated window, input, render, pause, audio, and save checks. These checks do not certify full game parity.

## Download and play

**[Download the development preview](https://github.com/JustPatrox/DeadWarrior-2/releases)**

| Platform | Release asset | Start |
| --- | --- | --- |
| Windows x64 | `DeadWarrior-2-Windows-x64.zip` | Extract the entire ZIP and run `DeadWarrior 2.exe` |
| macOS Apple Silicon | `DeadWarrior-2-macOS-arm64.zip` | Extract and open `DeadWarrior 2.app` |
| macOS Intel | `DeadWarrior-2-macOS-x64.zip` | Extract and open `DeadWarrior 2.app` |
| Linux x64 | `DeadWarrior-2-Linux-x64.tar.gz` | Extract and run `./"DeadWarrior 2"` inside its folder |

All packages include assets. Keep Linux/Windows assets beside the executable; macOS keeps them inside the app. macOS builds have an ad-hoc integrity signature, without Developer ID notarization. Linux builds target Ubuntu 22.04 or newer compatible distributions (glibc 2.35+), with desktop graphics and audio libraries.

This is an unfinished campaign preview with ten missions; it is not the complete game. All four native packages use the same campaign sources and prepared content. Automated checks are supplemented by ongoing gameplay validation.

## Build from source

For the native Windows build, you need Git and **Visual Studio 2022 or 2026** with *Desktop development with C++* and CMake 3.24 or later. Copy the prepared `assets` folder from the Windows download into your checkout, then build:

```powershell
git clone https://github.com/JustPatrox/DeadWarrior-2.git
cd DeadWarrior-2
# Copy the release's assets folder into this directory before building.
.\tools\build_windows.ps1
& '.\out\windows-x64\DeadWarrior 2.exe'
```

See the [build guide](docs/building.md) and [macOS/Linux guide](docs/portable.md) for details. Preparing resources from an original SWF is an **optional developer workflow**, using Python, Java, and FFDec. Those tools are not part of the game runtime. The source tree keeps generated game resources out of Git; releases include everything needed to play.

## PC controls

| Key | Action |
| --- | --- |
| **Insert** (Windows/Linux), **Option** (macOS) | Open or close PC settings |
| **F11** | Toggle fullscreen |
| **Arrow keys / Tab** | Navigate PC settings |
| **Enter / Space** | Change the selected option |
| **Esc** | Close PC settings or pause/resume the battle |
| **P** | Pause/resume the battle |

The larger **PC** label keeps the original font and button artwork. The original **PC** toolbar button also opens settings. Use **Insert â†’ Show FPS â†’ On** to display the counter in the top-left corner. Frame-rate settings affect presentation while preserving the simulation rate. The resolution selector offers Auto, 720p, 900p, 1080p, 1440p, and 2160p. Cursor lock confines the pointer during an active battle and releases it for pause, settings, focus loss, and minimization. **Stretched** fills the display; **Dynamic [Experimental]** expands the logical camera and adapts the HUD, terrain, fog, and minimap. The default remains **16:9**. Borderless covers the full monitor. PC settings are saved automatically in `%LOCALAPPDATA%\DeadWarrior 2\options.ini`; deleting a campaign save keeps these preferences.

See the [preview.6 fixes and validation](docs/todo-checkpoint.md) for the completed TODO items and test scope. The [economy and orders correction](docs/economy-orders-fix.md) documents finite gold, miner retargeting, original castle defenders, and defensive formation behavior.

## Reporting a problem

Open [a bug report](https://github.com/JustPatrox/DeadWarrior-2/issues/new?template=bug_report.yml) with reproduction steps, resolution, display mode, and version or commit. Attach **`lastest.log`** — that spelling is intentional. It is replaced on each launch and normally sits next to the executable. If that directory is not writable, it is stored in `%LOCALAPPDATA%\DeadWarrior 2`.

Review logs for personal paths before posting them. Do not attach the SWF, extracted resources, or saves containing information you do not want to share.

## Development

See [CONTRIBUTING.md](CONTRIBUTING.md), the [Windows guide](docs/windows.md), and the [translation traceability](docs/traceability.md). Simulation code lives in `src/campaign_*` and `src/tutorial_*`; `src/windows_*` handles the platform. Preparation and verification tools live in `tools/`.

<p align="center"><img src="icon.png" alt="DeadWarrior 2 helmet" width="72"><br><sub>DeadWarrior 2 · Work in progress</sub></p>
