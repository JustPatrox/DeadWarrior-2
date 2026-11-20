# Windows ninth-mission checkpoint

CampaignDeads (Undead War, zero-based level 8) runs in the native campaign and is included in Preview 7. Preview 6 remains the historical eight-mission TODO release.

The mobile Dead uses actor 8529 with its Default head. Its frames use a separate 10000+ bank from the castle defender's Wrapped Helmet. Original Adobe readback measured the mobile collider at 104.95 by 116.15. The C++ translation retains the original aiming, attack event, Guts projectile, poison payment, and one-time 152-update poison instruction. Miners can garrison or resume without interrupting military orders. Original miner-speed, integer mana payments, passive income, and garrison healing are retained.

## Verification

- `tools/check_deads_sim.cpp` reaches victory at tick 11869 using six legitimately earned upgrades, two unspent points, original costs, two Giants, healers, and wizards. This is a deterministic native strategy, not an original-game balance comparison.
- `tools/check_deads.py 1280 720` reaches victory at tick 13937, with 464 Guts shots, 67 poison hits, 33 cures, and 2/2/7 spell counts.
- `tools/check_deads.py 1920 1080` reaches victory at tick 10179, with 114 Guts shots, 39 poison hits, 26 cures, and 6/3/8 spell counts.
- Both real-window runs exercise miners-only controls, save level 9 with three unspent points, reach map frame 510, and capture no uncovered magenta. Their asynchronous click schedules differ, so their tick totals do not demonstrate identical battle balance.
- All ten campaign/toolbar/blend CTest checks pass. Logs: `work/deads-all-probes.log`, `work/deads-resume-build.log`, and `work/deads-resume-{720,1080}.log`. Captures, traces, and reports: `evidence/windows-deads/`.
- Prepared original-player deployment and funded-poison references at both sizes are in `evidence/adobe-deads/`. These exercise original actors and controller behavior, rather than proving a natural Adobe battle victory.

Original `UserInterface.as` calls `manthefortSoundOrder` and `defendSoundOrder` for miner controls. Neither is registered by the original `SoundLoader.as`, and no corresponding sound was recovered. Original `SoundManager.playSoundFullVolume` returns zero for those calls. The port keeps that original silence and logs the unresolved event; it does not substitute a different voice.

The source SWF remains unchanged. Full combat/collision/duel parity, higher-difficulty walls and other previously recorded paths remain open. The next module is Crawlers (zero-based level 9), actor 6123 / `Cat.as`, GrassHills map 4. Its speed and pack tech are granted by campaign AI only on Insane; preserve that reachable behavior and its original damage-to-armour quirk. Analysis is in `work/crawler-next-module.json`.
