<p align="center">
  <img src="logo.png" alt="DeadWarrior 2" width="820">
</p>

<p align="center">
  <strong>A native port of Stick War 2. Work in progress.</strong><br>
  C++20 · Windows x64 · 16:9 presentation
</p>

<p align="center">
  <a href="https://github.com/JustPatrox/DeadWarrior-2/actions/workflows/windows.yml"><img src="https://github.com/JustPatrox/DeadWarrior-2/actions/workflows/windows.yml/badge.svg" alt="Windows build"></a>
  <img src="https://img.shields.io/badge/status-work_in_progress-e5b344" alt="Work in progress">
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white" alt="C++20">
  <img src="https://img.shields.io/badge/platform-Windows_x64-0078D4" alt="Windows x64">
</p>

<p align="center">
  <a href="#current-status">Status</a> ·
  <a href="#build-and-play">Build</a> ·
  <a href="#pc-controls">Controls</a> ·
  <a href="https://github.com/JustPatrox/DeadWarrior-2/issues/new/choose">Report a bug</a> ·
  <a href="CONTRIBUTING.md">Contribute</a>
</p>

## About

DeadWarrior 2 translates the game's logic into C++ and adapts its presentation to 16:9 displays. The executable uses Direct2D and XAudio2, with a software rendering fallback. The simulation runs at **30 Hz**, independently of the rendering frame rate.

Each user must supply their **own copy of `stickwar2.swf`** to generate the resources for their installation. This repository contains the code and preparation tools. The SWF, decompiled game scripts, sprites, music, and sound effects stay outside Git.

This project is not affiliated with Max Games or the original game's creators. See [credits and third-party content](NOTICE.md).

## Current status

**Development build: the full campaign has not been ported yet.** Six missions are accessible, through Massive Battle. Unimplemented sections display a work-in-progress screen with restart and quit options.

| Feature | Status |
| --- | --- |
| Menus, tutorial, and first six missions | Implemented; full parity is still under review |
| Campaign progress, upgrades, and saves | Implemented for the current content |
| 16:9 rendering, audio, and contextual cursors | Implemented |
| Windowed, borderless window, and fullscreen modes | Available in the Insert menu |
| 30 / 60 / 120 / 240 FPS or unlimited | Available |
| V-Sync and FPS counter | Available |
| Remaining campaign and cutscenes | Pending |
| macOS and Linux | Pending; after Windows validation |

CI checks the Windows source build and Python tools. Gameplay tests require locally generated assets; a green CI result does not certify the full campaign.

## Build and play

You need Windows x64, Git, **Visual Studio 2022 or 2026** with *Desktop development with C++*, CMake 3.24 or later, **Python 3.11**, **Java 21 x64**, and **JPEXS FFDec 26.5.0**. Links and instructions are in the [build guide](docs/building.md).

```powershell
git clone https://github.com/JustPatrox/DeadWarrior-2.git
cd DeadWarrior-2
python -m venv work/venv
work/venv/Scripts/python.exe -m pip install -r requirements-assets.txt
```

Prepare your SWF and tools as described in [the guide](docs/building.md), then run:

```powershell
work/venv/Scripts/python.exe tools/prepare_assets.py --swf "C:\path\to\stickwar2.swf" --java-home "C:\path\to\java-21" --ffdec-home "C:\path\to\ffdec"
.\tools\build_windows.ps1
& '.\out\windows-x64\DeadWarrior 2.exe'
```

The first preparation run extracts and generates thousands of resources and can take a while. Keep the executable alongside its `assets` folder.

## PC controls

| Key | Action |
| --- | --- |
| **Insert** | Open or close PC settings |
| **F11** | Toggle fullscreen |
| **Arrow keys / Tab** | Navigate PC settings |
| **Enter / Space** | Change the selected option |
| **Esc** | Close PC settings |

Use **Insert → Show FPS → On** to display the counter in the top-left corner. Frame-rate settings affect presentation while preserving the simulation rate.

## Reporting a problem

Open [a bug report](https://github.com/JustPatrox/DeadWarrior-2/issues/new?template=bug_report.yml) with reproduction steps, resolution, display mode, and version or commit. Attach **`lastest.log`** — that spelling is intentional. It is replaced on each launch and normally sits next to the executable. If that directory is not writable, it is stored in `%LOCALAPPDATA%\DeadWarrior 2`.

Review logs for personal paths before posting them. Do not attach the SWF, extracted resources, or saves containing information you do not want to share.

## Development

See [CONTRIBUTING.md](CONTRIBUTING.md), the [Windows guide](docs/windows.md), and the [translation traceability](docs/traceability.md). Simulation code lives in `src/campaign_*` and `src/tutorial_*`; `src/windows_*` handles the platform. Preparation and verification tools live in `tools/`.

<p align="center"><img src="icon.png" alt="DeadWarrior 2 helmet" width="72"><br><sub>DeadWarrior 2 · Work in progress</sub></p>
