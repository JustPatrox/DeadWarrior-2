# Windows x64 development and validation

For a first build, follow [building.md](building.md). This guide records the current platform behavior, test commands, and remaining parity work. Files under `work/` and `evidence/` are generated locally and are not distributed in the source repository.

## Build and output

From PowerShell at the project root, with Visual Studio 2022 or 2026, the desktop C++ workload, Windows SDK, and CMake installed:

```powershell
.\tools\build_windows.ps1
& '.\out\windows-x64\DeadWarrior 2.exe'
```

The output directory is `out/windows-x64/`. The executable loads prepared textures and audio from its adjacent `assets/` directory and runs the controllers in C++. MSVC's runtime is linked statically.

The helmet icon comes from `icon.png`; `src/resources/deadwarrior2.ico` contains ten icon sizes. The three-second startup notice reads **This project is not affiliated with** and precedes the menu and its music. The supplied Max Games logo in `assets/branding/max-games.png` has a transparent background, white controller buttons, and a thin white outline.

## Display settings and timing

**Insert** opens PC settings. Choose **Windowed**, **Borderless Window**, or **Fullscreen** with the mouse or arrow keys and Enter. Borderless Window preserves the taskbar; Fullscreen covers the monitor. Insert or Esc closes the panel, and F11 toggles fullscreen. Presentation remains 16:9; letterbox areas do not accept game clicks.

**FPS limit** offers 30, 60, 120, 240, and Unlimited, initially 60. Navigate to the row, select a value with Left/Right, and press Enter. The simulation runs at 30 steps per second and catches up when rendering takes longer. Direct2D presents frames; GDI+ supports private captures and provides a fallback.

**V-Sync** starts Off. Clicking its row or pressing Enter toggles synchronization with the display refresh rate while retaining the selected frame limit. See [Direct2D presentation options](https://learn.microsoft.com/en-us/windows/win32/api/d2d1/ne-d2d1-d2d1_present_options).

**Show FPS** starts Off. When enabled, the counter stays in the top-left of the 16:9 viewport after closing settings. It measures completed presentations over the last second; private diagnostic captures are excluded. End selects Show FPS, and End followed by Up selects V-Sync. Enter or Space toggles the selected switch.

```powershell
work/venv/Scripts/python.exe tools/check_display.py --fps-limits --vsync
work/venv/Scripts/python.exe tools/check_display.py --compare-software
work/venv/Scripts/python.exe tools/check_fps_counter.py
```

Recorded results on an Intel i5-7400 and RTX 3050 at 1920 × 1080 were 60.07 FPS with Direct2D and 1.88 FPS with GDI+ during the tutorial. One of 853 accelerated frames exceeded 25 ms between presentations. These are measurements from that machine, not performance guarantees.

The frame-limit checks recorded simulation rates between 29.82 and 30.20 steps per second with Direct2D, and 30.01 with GDI+ while rendering fell to 1.84 FPS. The FPS-counter test measured 60.03 / 30.01 FPS at 720p and 60.00 / 29.98 at 1080p. Actual presentation rate also depends on the monitor, scene, and V-Sync. Reports live in `work/windows-display/` and `evidence/windows-fps-counter/`.

### Resolution and saved preferences

Resolution offers **Auto**, **1280 × 720**, **1600 × 900**, **1920 × 1080**, **2560 × 1440**, and **3840 × 2160**. Use the arrows beside the value, or focus the row with Home followed by Up, choose with Left/Right, and press Enter. Auto follows the client size.

The setting controls the game's render buffer in every display mode. Windowed mode also fits the window to the requested size within the monitor's work area. Fullscreen and borderless presentation retain the desktop mode and scale the chosen render resolution. Letterbox proportions and mouse coordinates remain based on the physical client area. Direct2D and the software fallback both support this behavior.

All Insert options are saved automatically in `%LOCALAPPDATA%\DeadWarrior 2\options.ini`: display mode, render resolution, frame limit, V-Sync, and Show FPS. Writes use a temporary file and atomic replacement. Invalid values fall back to supported defaults. `--save-dir` redirects both campaign saves and preferences for tests. Restarting with a deleted campaign save preserves the options file.

```ini
[Display]
mode=windowed
resolution=auto
fps_limit=60
vsync=0
show_fps=0
```

`fps_limit=0` means Unlimited. The options file and temporary files stay outside source control and release packages.

### Campaign pause menu

The HUD's **MENU** button, **Esc**, and **P** open the original campaign pause panel. It offers Back to Game, a local How to Play guide, Restart Level, Quit to Menu, and independent Sound/Music toggles. Restart and quit use a confirmation panel. The original shield belongs to this pause menu; PC settings use a separate dark panel with gold beveled borders.

Both pause and in-battle Insert settings stop simulation ticks. Narration and gameplay sound voices stop at their current sample positions and resume without skipping ahead; background music follows its own toggle. Closing Insert over an open pause menu returns to the pause menu. Input is blocked from reaching the battle while either panel is open. Resuming rebases the simulation deadline rather than replaying paused time.

```powershell
work/venv/Scripts/python.exe tools/check_resolution_pause.py
work/venv/Scripts/python.exe tools/check_options.py
```

These native-window tests cover the actual render-target dimensions, window fitting, fullscreen/borderless modes, letterboxing, mouse/keyboard selection, pause timing, restart/quit actions, independent audio controls, process relaunch, invalid options, and preservation of preferences after deleting `campaign.sav`. Local results are written to `evidence/windows-pause-settings/` and `evidence/windows-options/`.

## Resource preparation

The Windows release includes prepared native resources. Players do not need a SWF or extraction tools. Developers can copy the release's `assets` folder into a checkout for a normal native C++ build. Only optional resource regeneration uses the compatible 18,886,237-byte SWF documented in [building.md](building.md); it does not modify the original. The executable never loads the SWF at runtime.

The pipeline uses FFDec 26.5.0, Temurin Java 21 x64, and the Python packages pinned in `requirements-assets.txt`. `tools/prepare_assets.py` accepts explicit SWF, Java, and FFDec paths. Individual generators use `DW2_SWF`, `DW2_JAVA_HOME`, and `DW2_FFDEC_HOME` when set; otherwise they use the original project-local paths.

The landscape extensions in `art/menu/` belong to the flattened menu illustration, symbol 976. The compiler matches their seam colors while preserving the original decoded 850 × 700 center. Titles, buttons, and animated opening fighters come from the supplied SWF.

## Menu and window checks

```powershell
work/venv/Scripts/python.exe tools/check_menu.py
work/venv/Scripts/python.exe tools/check_window.py
```

`check_menu.py` captures deterministic menu ticks against a magenta clear color, checks coverage, and compares the original central framing with locally captured SWF references. Its comparison stage requires those reference images. `check_window.py` operates a real window, clicks controls, and checks the physical client area. The recorded reference machine used 96 DPI. Results are written to `evidence/windows-menu/`.

## Tutorial, combat, and campaign progress

A new Normal campaign enters the tutorial after **Skip Intro**. The window test covers selection, camera movement, first combat, mining, training, and castle defense. At 720p and 1080p, all four allied units followed the garrison order; research produced a castle archer and six arrows hit the Spearton.

```powershell
.\tools\build_windows.ps1 -CheckTutorial
work/venv/Scripts/python.exe tools/check_tutorial.py 1280 720 --defence
work/venv/Scripts/python.exe tools/check_tutorial.py 1920 1080 --defence
work/venv/Scripts/python.exe tools/check_tutorial.py 1280 720 --battle
work/venv/Scripts/python.exe tools/check_tutorial.py 1920 1080 --hard-loss
```

The simulation probe checks the 300-gold research cost and duration, duplicate purchases, building hit areas, simultaneous production, and cancellation refunds. Enemy reinforcements trigger below 75% statue health. Statue destruction plays 89 fall frames before the result screen. Victory unlocks Spearton; defeat presents a tip and Retry.

After victory, **Continue** opens upgrades. Buy **Rage** or keep the point using **Next Battle**. The map moves to Archidon territory. Toggling **Autosave Disabled** writes the current campaign immediately. Saves use `%LOCALAPPDATA%\DeadWarrior 2\campaign.sav`; tests isolate them with `--save-dir`.

```powershell
work/venv/Scripts/python.exe tools/check_tutorial.py 1280 720 --intermission
work/venv/Scripts/python.exe tools/check_tutorial.py 1920 1080 --intermission --keep-point
work/venv/Scripts/python.exe tools/reference_intermission.py 1280 720
work/venv/Scripts/python.exe tools/reference_intermission.py 1920 1080
work/venv/Scripts/python.exe tools/compare_intermission.py
```

The native test reaches intermission by playing from the menu. The Adobe intermission reference prepares a post-victory state to isolate the original upgrade and map controllers; it does not establish parity for the preceding battle. Local evidence lives in `evidence/windows-tutorial/` and `evidence/adobe-intermission/`.

## Missions two through five

| Mission | Implemented behavior | Verification and remaining work |
| --- | --- | --- |
| Archidons | Forest; three miners and two Swordwraths; 1000/800 statue health; 700 gold and 200 mana on Normal. Speartons share the barracks queue. Independent archer trajectories, tower control and income, and purchased Rage are available. | `archidon_probe`; window tests at 720p/1080p; map advances to frame 92. Full combat and technology parity remain under review. |
| Silent Assassins | Mountains; three miners, one Archidon, and one Swordwrath; 1200/1200 statue health. Ninjas cloak, poison when leaving cloak, and accumulate damage against the same target. Garrisoning cures poison. | The probe checks poisoning, recruitment, results, and travel to frame 151. Entry landscape comparison measured mean RGB error 0.71/0.80 out of 255 at 720p/1080p; full combat comparison is pending. |
| Magic in the Air | SwampLands; 700/1200 statue health and an enemy castle archer. Meric heals; Magikill casts explosion, electric wall, and poison at pose frames 36, 47, and 44. | `magic_probe` checks spells, healing, defense, victory, and defeat. Victory shows both unit-unlock cards and travels to frame 238. |
| Rebels United | GrassHills; four miners and one each of Swordwrath, Spearton, Shadowrath, Magikill, and Meric; population 24; 1500/1500 statue health. Temple and magic-guild queues are independent. | `rebels_probe` and window tests check victory, saves, spell targeting, and travel to frame 298. Automatic movement into spell range is still pending. |

Select Magikill and use Q for explosion, W for Poison Spray after buying its upgrade, and E for Electric Wall after buying its upgrade. Click an in-range target position; right-click cancels targeting. Meric toggles healing with Q and uses W for Cure when purchased.

```powershell
.\tools\build_windows.ps1 -CheckArchidon -CheckMagic -CheckRebels
work/venv/Scripts/python.exe tools/check_archidon.py 1280 720
work/venv/Scripts/python.exe tools/check_archidon.py 1920 1080 --shadow
work/venv/Scripts/python.exe tools/check_archidon.py 1280 720 --magic
work/venv/Scripts/python.exe tools/check_archidon.py 1920 1080 --rebels
```

Use either resolution with each mode. These tests prepare temporary saves at the relevant territory. Comparisons and captures are local under `evidence/adobe-*` and `evidence/windows-*`.

## Massive Battle and camera bounds

The sixth mission implements Desert, Giant, Chaos workers, CastleDeads, and CampaignCutScene1. The controller holds the enemy statue at 180 health and enemy gold at zero. Killing the first Giant starts the fades, three Medusa voices, and four rebel spokesperson voices. The last voice triggers victory and unlocks Enslaved Giant. The next map reaches frame 341; the seventh battle displays the work-in-progress screen.

```powershell
.\tools\build_windows.ps1 -CheckGiants
work/venv/Scripts/python.exe tools/check_giants.py 1280 720
work/venv/Scripts/python.exe tools/check_giants.py 1920 1080
work/venv/Scripts/python.exe tools/reference_battle.py 1280 720 --giants
```

The Adobe `--giants` reference prepares the Giant's death to isolate the cutscene. It does not validate a natural victory in the original battle. Its captures live in `evidence/adobe-giants/prepared-death/`.

The requested 16:9 camera limits are `-Viewport::left` through `map.length - Viewport::right`, approximately 197.222 through `map.length - 1047.222`. This hides the area behind the castles without changing simulated map dimensions. The speed button uses its circular hit area so its transparent corner does not intercept minimap clicks.

## Audio, cursors, and appearance timelines

Original contextual pointers come from `assets/interaction/`: attack over enemies, a pickaxe over gold for selected miners, and a mana cursor over statues. Orders use the original 12-frame Cursor animation. Attack-move accepts map and minimap destinations; right-click cancels it. Hold-position, stop, and garrison controls act on the selection.

The system cursor is hidden while drawing a contextual game cursor. Letterbox areas, HUD, PC settings, and other windows retain the normal pointer. Losing focus or mouse capture cancels dragging. Right-clicking the minimap orders the selection to that position.

`windows_audio.cpp` mixes music, narration, and effects through separate XAudio2 voices. `SoundLoader`'s 187 entries are decoded to PCM, retaining names and gain multipliers. Victory and defeat music loop. Background music uses `0.2 * volumeMap[name]`; `loginMusic` has gain 0.672. An isolated native menu session measured a peak of 0.222104. `tools/check_menu_audio.py` tests menu music without attributing narration or effects to it.

The compiler stops equipment timelines selected by `Unit.setItem`, while retaining animated bodies. This fixes the cycling Giant bag and equipment changes on Shadowrath, Magikill, Meric, and other units. CastleDead uses Wrapped Helmet; castle archers use Basic Helmet. `loadout_assets.py` retains authored alternatives so a fresh tutorial build can select the castle-archer helmet after the default equipment timelines have been stopped.

```powershell
work/venv/Scripts/python.exe tools/build_interaction_assets.py --audio-only
work/venv/Scripts/python.exe tools/build_giants_assets.py --enslaved-only
work/venv/Scripts/python.exe tools/refresh_actor_loadouts.py
.\tools\build_windows.ps1 -CheckAudio
work/venv/Scripts/python.exe tools/check_menu_audio.py
```

The appearance refresh updates 4,726 frames across 61 poses without rebuilding terrain, UI, or audio. The Giant bag's 319 idle, walk, and death frames retain Default rather than cycling through Bones/Stone variants.

## Logs and unfinished content

Each launch overwrites a single **`lastest.log`** next to the executable, with `%LOCALAPPDATA%\DeadWarrior 2` as a fallback. It records platform initialization, audio errors, missing resources, game states, and unsupported controls. There is no rotating log archive.

Unsupported controls display **This button does not work yet, please report it with the log**. Entering an unfinished mission displays:

> Thanks for playing! But this part is still a work in progress. Stay tuned for an update!

The screen reuses menu background 976 and button states from 1002. **Restart keeping saves** preserves campaign progress; **Restart and delete save** deletes only `campaign.sav`; **Quit** closes the game. Saved progress pointing to an unfinished territory opens this screen rather than attempting to load missing resources. Insert settings block clicks on controls behind them. Tab/arrow navigation and Enter/Space are supported.

```powershell
work/venv/Scripts/python.exe tools/check_unfinished.py
```

Set `DW2_TEST_EXE` to an absolute executable path to check an alternative installation. Local reports live in `evidence/windows-unfinished/`.

## Original-player references and limitations

```powershell
work/venv/Scripts/python.exe tools/reference_battle.py 1280 720 --battle
work/venv/Scripts/python.exe tools/reference_battle.py 1920 1080 --battle
```

Adobe projector references operate the original controllers through a diagnostic copy and receive pixels and state over an already-connected local socket. The original stage drops to 0.01 FPS during battle, preventing the earlier HTTP capture approach from responding. The pointer-exit filter applies only inside the diagnostic stage. The root SWF remains unchanged.

Menu references also use local Ruffle web 0.7.1 with Canvas rendering and the game's external network disabled. Optional setup uses Playwright 1.65.2 and local browser files under `work/tools/`. Timing-based SWF animation samples may differ across runs; C++ capture ticks are deterministic.

Full parity of collisions, duels, AI, sieges, the tutorial's natural battle timing, remaining cinematics, higher-DPI behavior, and all interface states is not yet established. Prepared reference scenes are distinguished from complete original-game playthroughs. Windows validation and the full campaign remain open; macOS and Linux have not started.


## Seventh mission: Explosive War

Build and run its deterministic campaign checks with:

```powershell
.\tools\build_windows.ps1 -CheckBomber
```

The probe recruits miners, a healer, archers, and allied Giants using six earned upgrade points, exercises the original timed waves and armor-sensitive Boulder impacts, reaches both victory and defeat, restores the campaign save, and advances to Juggerknights (map frame 401). It does not bypass victory conditions.

Run the actual executable checks at both target resolutions:

```powershell
work/venv/Scripts/python.exe tools/check_bomber.py 1280 720
work/venv/Scripts/python.exe tools/check_bomber.py 1920 1080
```

The script uses temporary campaign saves and private client captures, validates full waves, Giant production, victory, upgrades, map travel and saving, and checks captured scenes for uncovered magenta pixels. Evidence: `evidence/windows-bomber/`. The Adobe prepared actor reference is `tools/reference_battle.py WIDTH HEIGHT --bomber`; it uses a diagnostic copy and leaves the original SWF unchanged.
