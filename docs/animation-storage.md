# Lossless animation storage

The game keeps the exported resolution, registration points, RGBA colors, animation timelines and sound-event clock. Optimization changes resource storage and texture reuse; the C++ simulation still runs at 30 Hz.

## Sprite sheets

`tools/build_sprite_sheets.py` converts symbols with multiple timeline frames to PNG sheets. Static images and map/background variants remain separate. The manifest retains each symbol/frame key and its four logical bounds, followed by a normalized rectangle within the sheet. Rendering combines that rectangle with any authored crop before sampling.

Frames are neither resized nor quantized. Exact duplicate RGBA cells within a symbol share a rectangle. Two extruded edge pixels surround every cell to prevent adjacent artwork bleeding during linear filtering. Larger resource groups use separate pages per animated symbol, with cells in timeline order. This keeps a small hint or an idle pose from depending on pages filled with unrelated death animations and statue artwork. A complete group that fits in one page of at most 8 MiB decoded RGBA keeps compact packing, including the toolbar and cursors. Most pages fit within 2048 by 2048; large campaign-map frames use pages up to 4096 by 4096 so that several frames actually share each page.

The integrity report records original logical keys and bounds, source RGBA hashes, output rectangles, page ownership, unchanged static files, and compressed resource bytes. Cross-symbol duplicates can occupy separate cells to keep their active texture sets independent. This can increase page count and disk size compared with global packing; the sum of every page is not the amount needed by one visible action.

All 43,964 animated logical rows retain exact source RGBA crops, bounds, aliases and serialized UV references. The 208 static rows, including 54 background variants, remain unchanged. Converted folders contain 875 files and 462,331,768 bytes, compared with 14,586 files and 553,440,430 bytes in the individual-frame input. This is 13,259,162 bytes larger than preview14's globally packed layout; it trades some storage density for smaller action working sets.

An expanded actual Direct2D capture check exercises 24 poses with mirror/rotation, red tint, opacity and cropping. Both preview14 and the new sheets differ from individual-frame rendering by at most two channel units under hardware interpolation. An initial one-unit limit from the narrower twelve-Giant-pose fixture did not hold for this broader set; those results are retained rather than described as a pass. Stored pixels remain exact. Sequential captures compare the same pixel coordinates and require fresh storage-specific frame markers. The CPU compositor separately checks shared-sheet frame changes, cropped frames and fractional scaling without stale sample reuse.

Direct2D and SDL cache textures by sheet path and tint, allowing frames to reuse an upload. The Windows ready-page CPU cache has a 64 MiB budget, with eight-frame prediction; a worker can hold an additional page while decoding. The Direct2D GPU cache targets 192 MiB and retains any pages used by the current frame. Windows shares forecasts between visible instances of the same symbol, logical frame and tint, and across consecutive presentations of an unchanged pose. GPU eviction and target recreation invalidate this bookkeeping. It does not skip or substitute frames; a missed prediction still loads the exact requested picture. SDL separately caches menu-mask alpha channels so pointer motion does not repeatedly decode a complete sheet.

Loading a sheet can cost more than loading one small frame. Global size packing in preview14 spread the 32-frame tutorial hint across seven pages totaling 116,179,416 decoded bytes per tint. Symbol-local packing puts it in one 353,160-byte page. The Swordwrath stand/miner mine/hint page union falls from 212,075,728 to 30,237,696 bytes. These are complete action page sets derived from the actual generated images, not measurements of simultaneous residency, process memory, or game FPS. Frame-time and process-CPU checks must use the same live scene, frame cap, resolution and renderer without competing conversion or capture work.

To regenerate from a prepared resource set using Python with Pillow and NumPy:

```sh
python tools/build_sprite_sheets.py --help
```

Keep output separate from the source. The tool emits its integrity report outside the runtime assets. Its input is the prepared game resource set, not a new SWF extraction.

## Intro movie

`tools/pack_intro_movie.py` converts the previously verified version-1 PNG key/XOR intro into `movie.dw2v`. Version 2 of `frames.tsv` points to this single file, with no individual PNG-frame directory. Music and the nineteen sound events remain separate and byte-identical to the verified intro resources.

DW2MOV1 stores independent lossless WebP chunks of at most thirty logical source slots, plus an index. The native decoder loads only the active compressed chunk and copies the requested RGBA canvas into the existing bounded prefetch cache. It does not load the whole movie into memory. It retains the 1280 by 720 export and the authored thirty-frame source clock.

Identical pictures can coalesce into one encoded visual frame. The decoder maps each logical source slot to its visual span using rounded millisecond boundaries, including an entirely static thirty-slot chunk. Random/backward seeks decode at most thirty encoded pictures. The decoder validates canvas size, chunk extents, lossless payloads and time-span coverage. Source-frame numbering, pause, restart, Skip/Back and sound-event dispatch remain in `IntroMovie`.

The complete conversion changes 1,003,531,894 bytes of packed PNG pictures to a 753,417,600-byte movie, a 24.92% reduction in visual-resource bytes. There are 3,125 encoded visual spans covering all 3,233 logical slots; coalescence does not shorten the source timeline. The offline packer verifies every expanded RGBA hash and copies the audio/event resources byte-for-byte. These measurements do not include release-archive compression.

Runtime decoding uses the pinned libwebp 1.6.0 dependency from `cmake/Webp.cmake`; an encoder executable is not needed to play. Offline encoding uses Pillow/NumPy and requires more preparation time. Neither process-memory savings nor faster playback follow automatically from the smaller file. The original version-1 key/XOR reader remains available for older prepared assets.

The common `intro_probe` exercises both formats, coalesced spans, translucent RGBA, seeks, immutable published pictures and event order, with and without prefetch. The optional Windows `intro_assets_probe` verifies the actual complete movie against all 3,233 exported RGBA hashes and all nineteen events. The portable frontend smoke report separately records `spriteSheets` and `losslessIntro`, and the release workflow requires both.

For input provenance, original framing, screen-fill correction and audio limitations, see [the intro reference](intro-reference.md).
