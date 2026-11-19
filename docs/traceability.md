# Translation traceability

This table maps the original AS3 classes and symbol IDs to the native implementation. Generated resource paths refer to files created locally from the user's own SWF; they are not committed to the public repository. The inventory is generated as `evidence/inventory.json`, retaining timeline owners, frames, depths, registrations, masks, and placement changes.

## Menus and tutorial

| Original AS3 / symbol | Native implementation and generated data | Verified behavior / limits |
| --- | --- | --- |
| `CampaignMenuScreen.as` | `src/campaign_menu.cpp` | Intro, main panel, campaign, mute, and credits; authored panel/button placements and timing. Full interface coverage remains open. |
| `campaignMenuMc.as`, 1045 | `assets/menu/placements.tsv`, `CampaignMenu::compose` | Scene, logo, buttons, and overlays are composed separately. |
| `menuAssets_fla/Background_157.as`, 979 | `assets/menu/979-*.png`, `alpha.tsv` | Original placements through frames 1–97; fighter removal at 98, illustration appearance at 124, stop at 174. Six equipment timelines remain stopped. |
| 720 / 976 / 977 / 978 | `art/menu/`, `assets/menu/976-wide.png` | Full-width opening background; preserved original central illustration with separately composed landscape extensions. |
| `Title_184`, `Title_185`, 982 / 983 | 1044 / 699, `alpha.tsv` | Opacity curves recovered from the original timelines. |
| Panels 1006 / 1011 / 1024 / 1028 | `placements.tsv`, inverse viewport input mapping | SimpleButton hit states and campaign MovieClip bounds preserve their intended hit behavior. |
| Binary data 29 / 10208 | Local `work/extract/binaryData/` | Original campaign/game XML: campaign 1.24, Tutorial in Swamp Lands. |
| `CampaignTutorial`, `InGameMessage`, 10139 / 10138 / 1924 | `src/campaign_tutorial.cpp`, `src/tutorial_defence.cpp`, `messages.tsv` | Skip Intro entry and 22 messages; defense reaches the statue objective in simulation and real windows at 720p/1080p. |
| `Team.garrison`, `Team.defend`, `ArcheryBuilding`, `Tech.update` | `tutorial_defence.cpp`, `control-masks.tsv`, `rules.tsv` | Original order hit areas 9984/9977 and building area 1645; 300-gold upgrade and original research countdown. |
| `CastleArchers.addUnit`, `Archer.aim`, `ProjectileManager`, `Projectile` | `tutorial_defence.cpp`, 4176 / 4168 / 3988 | Independent body/bow registration and animation; projectile height, depth, gravity, and contacts. Complete frame-by-frame Adobe comparison remains pending. |
| `Team.updateUnitProduction`, `EnemyTeamAi`, `EnemyGoodTeamAi`, `Statue` | `src/tutorial_battle.cpp`, `rules.tsv` | Global/selection orders, production and refunds by building, both economies, statue attacks, and reinforcements. Deterministic win/loss routes pass; equivalent natural battle timing remains under review. |
| `UserInterface`, statue 1922, `PostGameScreen`, 286 / 312 / 335 / 338 / 343 | `src/campaign_result.cpp`, `src/windows.cpp` | Original 89-frame statue fall with team appearance; simulated population/worker graphs, unlock card, defeat tip, and result audio. |
| `SwampLands`, `Map.createMiningBlock`, `Background`, 4098 / 4102 / 1051 / 1078 | `tools/build_campaign_assets.py`, `CampaignTutorial::drawList` | 250-unit battlefield depth, 700-unit height, independent parallax, and ground extended beneath the HUD's flanks. |
| `UnitMove`, `UnitAi`, `Unit`, `Swordwrath`, 4922 | `campaign_tutorial.cpp`, `animations.tsv`, `markers.tsv` | Original six-row formation and 30 Hz movement/weapon contacts. Complete collision, synchronization, and random-sequence parity remain open. |
| `MinerAi`, `Miner`, `Statue`, 3104 / 1904 | `CampaignTutorial::workMiner`, `rules.tsv` | Reserved mining/worship positions and resources produced on authored animation events. |
| `TeamGood`, `GoodHud`, `SelectedUnits`, `ActionInterface` | 10118 / 9726 / 1244 / 1447 and command bitmaps | Castles, statue, resources, minimap, portraits, and orders. Remaining UI uses are still under review. |

## Upgrades, map, and saves

| Original | Native implementation | Verification |
| --- | --- | --- |
| `CampaignUpgradeScreen`, `Campaign.initUpgradeTree`, `GoodTech`, 694 | `src/campaign_intermission.cpp`, `upgrades.tsv` | Twenty technologies retain prerequisites, state, and tooltips. Rage purchase and keeping the point pass; tree and cards compared with Adobe. |
| `confirmTech` 693, field 684 | `tools/build_intermission_assets.py`, embedded font 672 metrics | Separate panel/dimming and readable original paragraphs; FFDec and projector field-capture limitations are documented locally. |
| `CampaignScreen`, 499 / 436 / 393 / 478 / 498 | `map-motion.tsv` and independent map/flag layers | Original transforms, route marks, destination animation, and extended ocean/rock coverage. Autosave and map return tested in native windows. |
| `CampaignGameScreen.endGame`, `Level.updateTime`, `Campaign.save/load` | `src/campaign_progress.cpp`, `src/windows_save.cpp` | Attempts, accumulated/best times, difficulty, level, upgrades, and points persist. Separate processes resume both the Rage and unspent-point branches; completed saves replace the previous file. |

## Missions two through six

| Original | Native implementation and symbols | Verification / remaining work |
| --- | --- | --- |
| `CampaignArcher`, `Forest`, `Hill` | `src/campaign_archidon.cpp`; 4119 / 4117 / 4114 / 4124, tower 1162 and flame 1158 | Tower control/income and original starting state; Adobe deployment and native window captures. |
| `Archer`, `ArcherAi`, `Bow` | Actor 4236, independent bow 4168, `bows.tsv` | Recruitment, mixed queues, refunds, trajectories, result, and travel to frame 92 in `archidon_probe`. |
| `CampaignShadow`, `Mountains`, `Ninja`, `NinjaAi`, `EnemyGoodTeamAi.updateNinjas` | Layers 4044 / 4048 / 4053; actor 7289 and poses 7060 / 7071 / 7134 / 7181 / 7278 / 7274 | Weapon-tip contacts, cloak, poison, and repeated-target damage; original entry, native poisoned ally, victory, and travel to frame 151. |
| `CampaignScreen`, 499 / 393 | `tools/build_archidon_map.py` | Original flag/map motion with per-frame 16:9 coverage. A three-pixel gap at frame 92 was corrected using the actual transform. |
| `Magikill`, `Nuke`, `ElectricWall`, `PoisonSpray` | `src/campaign_magic.cpp`, 3172 / 3120 / 3968 | Original costs, cooldowns, release frames, and bomber-based explosion radius. `magic_probe` checks spells, healing, defense, win/loss; full building collision parity remains open. |
| `Monk`, `MonkAi` | Poses 8067 / 8104 / 8183 / 8229, healing at frame 25 | Healing, independent temple production, and original-cost refunds. |
| `PostGameScreen.showNextUnitUnlocked` | `src/campaign_result.cpp` | Successive Magikill/Meric cards after mission four; Rebels United goes directly to statistics. |
| `GrassHills`, XML level 5 | Layers 4058 / 4056 / 4062 / 4067, `assets/rebels/` | Ground extended from the last opaque row; original route to frame 298. Entry landscapes, victory, and saves checked at 720p/1080p. |
| `CampaignCutScene1` | `src/campaign_giants.cpp` | Enemy statue 180, zero enemy gold, first Giant death, 61-frame fades, Medusa, five arriving rebels, seven voices, and final-voice victory. `giants_probe` and an Adobe prepared-death reference pass; natural original battle parity remains pending. |
| `Giant`, `MinerChaos`, `CastleDeads` / `Dead` | `campaign_giants.cpp`, Desert and Chaos castle resources | Up to three frontal targets, stun, Chaos mining, elevated poisoned projectiles; hit/poison/deployment checks and native captures. |
| `EnslavedGiant.setItem`, giantbag 1950 | Stopped Default appearance with original body transforms | All 319 idle/walk/death frames retain the bag instead of cycling Bones/Stone. The allied ranged attack is still pending. |

## Interaction, audio, and requested PC behavior

| Original / requested behavior | Implementation | Verification |
| --- | --- | --- |
| `MoveCommand.drawCursorPreClick`, `AttackMoveCommand`, `Cursor` | `src/campaign_interaction.cpp`; 2310 / 2313 / 8821 / 8824 | Shared target queries, red attack tint, prayer/mining contexts, attack-move cancellation, selection garrison, and minimap order markers. |
| `NukeCommand`, `PoisonDartCommand`, `StunCommand` | Cursors 8785 / 8800 and original range dimensions | Targeted explosion/wall in `rebels_probe`. |
| `ActionInterface` and Win32 pointer handling | `src/windows.cpp` | System cursor restored outside game context and on focus loss; attack, worship, mining, and movement captures at both resolutions. |
| `SoundLoader`, `SoundManager`, Unit/Team events | `tools/build_interaction_assets.py`, `src/windows_audio.cpp` | All 187 original entries converted to PCM; separate XAudio2 music/narration/effects voices; measured output/mute and victory music beyond 23 seconds. |
| `FogOfWar.setTint`, 1595 | RGB multiplier 0.1, preserved alpha, 400 additional logical units of opaque coverage | Tutorial captures and no-magenta coverage checks. |
| Requested castle camera bounds | `-Viewport::left` through `map.length - Viewport::right` | Simulated world size preserved; castle-edge captures and speed-button/minimap hit-area checks. |
| Requested unfinished-content screen and logging | `src/work_in_progress.cpp`, `src/windows_log.cpp`; menu 976 / button 1002 | Single `lastest.log`, restart with/without `campaign.sav`, Quit, saved unfinished territory, canceled click, and blocking behind Insert. |
| Requested FPS switch | `src/windows_display.cpp`, `src/windows.cpp` | Default Off; completed-presentation counter in the viewport's top-left, mouse/keyboard toggling, 30/60 FPS, V-Sync, and letterboxing at 720p/1080p. |

`ArtCompiler` applies `loadout_assets.py` only to the receivers of `gotoAndStop(weapon/armor/misc)` in the unit classes. Default is retained except for explicit appearances such as CastleDead's Wrapped Helmet and castle archers' Basic Helmet. Body/action animations remain active. `refresh_actor_loadouts.py` updates 4,726 frames across 61 poses while preserving registration and separate bow composition.

Music gain follows `0.2 * volumeMap(name)`. An isolated native menu session measured peak 0.222104 with `loginMusic` gain 0.672; the earlier combined music/voice/effect test did not isolate the music contribution.

The complete campaign, natural combat parity, remaining cinematics, broader DPI coverage, and all interface states remain unfinished. The seventh territory currently opens the work-in-progress screen. No macOS or Linux implementation is claimed.

## Native pause and PC settings

`CampaignPauseMenu`, `PauseMenu`, and `UserInterface.pauseButton` map to `src/pause_menu.cpp` and the Windows input/clock integration. The original panel (9964), shield (9951), static labels, text-button hover states (9915/9919/9957/9958), confirmation (9963/9939/9946), and sound/music toggle (9928) are compiled by `tools/build_pause_assets.py`. The normal text-button up states are transparent; hit testing uses their visible hover-state bounds. Native restart/quit confirmations and an offline control guide complete the PC interaction.

Requested PC resolution and persistence are implemented in `windows_display.cpp`: fixed 16:9 render sizes or Auto, uniform presentation, work-area window fitting, and atomic `options.ini` writes. The renderer reports actual Direct2D buffer dimensions for native tests. Pause tests verify frozen simulation ticks and 15 resumed ticks over half a second; audio tests compare narration sample positions across pause/resume and measure independent channel output. Both revised notice strings are rendered by the native executable.
