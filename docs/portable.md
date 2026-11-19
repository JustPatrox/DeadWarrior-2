# macOS and Linux

The portable frontend uses SDL 3 for native windows, accelerated 2D presentation, mouse and keyboard input, and audio devices. It shares the existing C++20 campaign controllers, save format, rules, assets, and fixed 30 Hz clock. It does not load a SWF or launch an emulator. Windows continues to use Direct2D/XAudio2.

## Build

Install CMake 3.24+, a C/C++20 compiler, Git, and the desktop development libraries for your system. On macOS, install the Xcode command line tools. On Ubuntu, the release workflow lists the required graphics/audio development packages. SDL and stb are fetched at pinned revisions and verified with SHA-256. Noto Sans and its OFL license are included in the source tree.

Copy the prepared `assets` directory from a release into the checkout, then run:

```sh
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --parallel
cmake --install build/native --prefix out/package
```

Linux executable: `out/package/DeadWarrior 2`.

macOS app: `out/package/DeadWarrior 2.app`. Release packages target macOS 11 or newer, separately for x86_64 and arm64. The app contains its resources and font. Ad-hoc signing checks integrity; these preview builds do not have Apple Developer ID notarization.

## Settings, saves, and diagnostics

Insert or F10 opens PC settings. F11 changes fullscreen. MENU, Esc, and P open the campaign pause menu. Presentation supports a resolution selector, windowed/borderless/fullscreen, V-Sync, an FPS counter, and 30/60/120/240/unlimited presentation rates. Gameplay remains at 30 Hz. The pause menu freezes gameplay and narration; music has an independent toggle.

SDL chooses the per-user directory. Typical locations:

- macOS: `~/Library/Application Support/JustPatrox/DeadWarrior 2/`
- Linux: `${XDG_DATA_HOME:-~/.local/share}/JustPatrox/DeadWarrior 2/`

The directory contains `campaign.sav`, `options.ini`, and one overwritten `lastest.log`. Use `--save-dir /path/to/test-directory` to isolate a test session. Saves use the same campaign format as Windows. Deleting the campaign save preserves display preferences.

## Validation

The release workflow builds each architecture on its own GitHub-hosted runner, runs the tutorial/Archidon/magic/rebels/giants deterministic probes, installs the package, and runs the portable executable with `--smoke-test out/checks --save-dir out/test-save`. The smoke test uses the actual renderer and shared input handlers to check menu navigation, all six mission entries at 720p/1080p, pause/resume, persistent options, audio loading/playback, save reload, and the unfinished-content guard. Linux runs under Xvfb with Mesa; macOS uses its desktop renderer. The CI audio device is a dummy device; this does not verify physical speakers or every hardware/driver combination.

Screenshots, logs, and JSON reports are retained as workflow artifacts. Full gameplay parity and manual testing on additional systems remain ongoing.

## Release automation

Publishing a GitHub release triggers **Native release builds**. Prepare the release with a ready-to-play `DeadWarrior-2-Windows-x64.zip` asset before publishing, or use the manual workflow's `assets_tag` to select an existing compatible asset source. The workflow extracts only prepared assets, compiles all four architecture packages, and attaches them only after every build and required check passes. Existing archive assets are preserved; `SHA256SUMS.txt` is refreshed from GitHub's uploaded digests.

To add platform downloads to an existing release, run the workflow manually on the intended source revision with `release_tag`. Source and asset revision must match. The original SWF and extracted scripts remain outside the public Git history.
