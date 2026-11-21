# Windows Crawlers checkpoint

The tenth mission, 4 legged Fury: Crawlers Attack (zero-based level 9), runs in the native campaign. The following Eclipsors mission remains unavailable until its translation and validation are complete.

## Original translation

The level uses Forest map 4 and the original empty CampaignController, with its XML starting army, recruitment composition, costs, research, and one-point reward. Cat actor 6123 uses the original stand/run/two attacks/death timelines and Default equipment. Adobe readback measured its collider at 93.45 by 150.

Cat.checkForHit uses damageToArmour against every target. The XML pack value 1.2 is stored in an AS3 int, so it contributes one damage per Cat, capped at five. Normal attacks deal 5. Insane campaign AI grants speed and pack tech: speed changes from 6 to 8, one Cat deals 6, and five or more deal 10. These rules were checked in the native probe and prepared original-player actor tests. No source cost or rule was changed to obtain victory.

The Forest sky continues its authored opaque RGB 0,35,75 behind its raster fringe. The exported 851-pixel sky has only 13/255 alpha in its last antialiased column; extending that column previously exposed tinted diagnostic clear in the wider viewport. Native checks now reject tinted clear as well as exact magenta. The original pre-fix functional run is preserved separately rather than treated as presentation evidence.

## Verification

- All eleven CTest checks passed after the sky correction. The updated Crawler probe also passes natural garrison defeat at tick 5091 and victory at tick 17962, with up to eight simultaneous Cats, two allied Giants, 139 enemy castle shots, save level 10, and map frame 554.
- The final real Windows 1280x720 run won at tick 13059, observing up to nine Cats and 117 Crawler hits. Its spell counts were 3/3/5.
- The final 1920x1080 run won at tick 12730, observing up to six Cats and 52 Crawler hits. That successful strategy used no spells. Its minimap actor capture was inspected visually.
- Both runs retain Normal maximum Crawler damage 5, exercise miners-only garrison/resume, save level 10 with four unspent points, reach the Eclipsors map (554), and pass the stronger diagnostic coverage check. Asynchronous click schedules and upgrade selections differ from the deterministic probe; these results do not prove identical original-game battle balance.
- The SDL frontend executed on Windows with ten mission entries and passed settings centering, toolbar states, aspect modes, pause, audio decoding/playback with dummy devices, saves, and the unfinished-content guard. This local result is not an executed macOS/Linux claim.
- Settings persistence, fullscreen restoration, restart choices, save deletion safety, and unsupported mission recovery passed with an isolated executable/log and evidence directory. Previously recorded captures remain preserved.

Logs: work/crawlers-sky-probes.log, work/crawlers-defeat-probe.log, work/crawlers-sky-{720,1080}.log, work/crawlers-sdl-smoke.log, work/crawlers-options.log, and work/crawlers-unfinished.log. Reports/captures: evidence/windows-crawlers/, evidence/adobe-crawlers/, work/crawlers-sdl-checks/, and work/crawlers-regressions/. The prepared original seven-Cat test is evidence/adobe-crawlers/pack/.

Original SoundLoader registers CrawlerDeath1..3. Cat, Knight, and Ninja call sword1, which is not registered anywhere in the recovered original scripts; SoundManager silently skips unknown names. The native log records this unresolved event without substituting another sound. The previously documented miner-control aliases are likewise preserved original silence.

Full collision/duel/combat parity, higher-difficulty walls, second castle research, and remaining campaign content stay open. The root SWF hash is unchanged. Next module: Eclipsors, Wingidon actor 2623, flying height 250, map 0. Its independent wing/leg/arm timelines and airborne projectile/target rules require original-player readback before implementation.
