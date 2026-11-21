# Giant rendering and Medusa staging

The large enemy and allied Giants previously enlarged textures baked at one pixel per original stage unit. The original vector artwork is now rasterized at triple density for all 950 stand, walk, attack, and death frames. Each existing logical rectangle, origin, animation length, collider, and timing remains unchanged. This restores edge detail instead of applying another blur filter to enlarged low-resolution pixels.

The Windows Direct2D renderer now decodes the next eight Giant frames on a worker, with a 32 MiB decoded-pixel budget. GPU resources stay on the window thread. The requested frame always renders: a prediction miss or unavailable worker falls back to synchronous decoding. No simulation rate or animation frame is skipped. Existing GPU caching remains bounded. Render hitches are throttled into `lastest.log`, including frame interval, upload count, prefetched count, blocking load time, and simulation tick.

In the prepared 1080p scene with eight visible enemy Giants, cold-frame p95 decreased from 15.8573 ms with synchronous loading to 6.3013 ms with prediction; blocking texture-load time decreased from 709.743 to 388.647 ms across the run. These timings describe a prepared rendering test on one machine, rather than a universal FPS guarantee or natural-battle balance evidence. All actors were asserted visible, textures stayed within the existing GPU budget, and no cache evictions occurred. Twelve resize/V-Sync render-target transitions also passed while prefetched frames were being consumed.

Medusa's cinematic height previously omitted her own 1.2 scale. Original `Unit.updateMotion` multiplies flying height by the full perspective scale: at `py=125`, `380 * (1.2 * .65) = 296.4`, rather than 247. The omitted 49.4 stage units placed her roughly 76 pixels too low at 1080p, behind the battlement. Her artwork and health bar now follow the original airborne position. Her authored art, world coordinates, camera, and depth order are preserved.

The twelve CTests passed, including new checks for Medusa's original draw coordinates and health-bar elevation during all three speeches. Actual Windows campaign runs at 1280x720 and 1920x1080 passed combat, seven voiced cinematic states, Giant unlock, victory, save, and map progression. Both Medusa views were inspected, and all six speech captures passed a cape-visibility check that rejects the previous hidden-body capture. The visible enemy Giant was also inspected at 1080p.

Developer tools:

- `tools/refresh_giant_resolution.py` regenerates the denser original artwork without changing manifest bounds. The regular Giant and Bomber builders use the same rasterization helper.
- `giant_render_probe` measures cold/warm rendering with prediction enabled or disabled, and exercises target recreation.
- `tools/check_giants.py` centers the real Giant on its first strike and checks Medusa's cape above the battlement.

Evidence is under `work/giant-medusa-native`, with `work/giant-rendering-checkpoint.json`, `work/giant-medusa-probes.log`, and prepared rendering reports. The campaign still contains ten verified missions; the remaining campaign and full original-game parity are in progress.
