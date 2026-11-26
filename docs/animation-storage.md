# Lossless animation storage

The game keeps the exported resolution, registration points, RGBA colors, animation timelines and sound-event clock. Optimization changes resource storage and texture reuse; the C++ simulation still runs at 30 Hz.

## Sprite sheets

`tools/build_sprite_sheets.py` converts symbols with multiple timeline frames to PNG sheets. Static images and map/background variants remain separate. The manifest retains each symbol/frame key and its four logical bounds, followed by a normalized rectangle within the sheet. Rendering combines that rectangle with any authored crop before sampling.

Frames are neither resized nor quantized. Exact duplicate RGBA cells share a rectangle. Two extruded edge pixels surround every cell to prevent adjacent artwork bleeding during linear filtering. Most pages fit within 2048 by 2048; large campaign-map frames use pages up to 4096 by 4096 so that several frames actually share each page.

The complete conversion covers 43,964 animated logical rows in 194 folder/symbol groups. It preserves 208 static rows, including 54 background variants. The affected folders decrease from 14,586 files and 553,440,430 bytes to 704 files and 450,328,813 bytes: 95.17% fewer files and 18.63% fewer bytes. These are resource-folder measurements, not whole-download savings.

Every unique source RGBA cell and its effective sheet crop was compared exactly. Every logical key, bound, alias and serialized UV reference was checked. A separate Direct2D comparison exercised twelve Giant poses, mirror/rotation, red tint, opacity and authored cropping. GPU interpolation differed by at most one channel unit; stored pixels were exact. The CPU compositor additionally checks shared-sheet frame changes, cropped frames and fractional scaling without stale cache reuse.

Direct2D and SDL cache textures by sheet path and tint, allowing frames to reuse an upload. The Windows ready-page CPU cache has a 64 MiB budget, with eight-frame prediction; a worker can hold an additional page while decoding. The Direct2D GPU cache targets 192 MiB and retains any pages used by the current frame. SDL separately caches menu-mask alpha channels so pointer motion does not repeatedly decode a complete sheet. Sheets reduce file/open/upload churn, but loading a page can cost more than loading one small frame. No universal FPS or total-memory improvement is claimed.

To regenerate from a prepared resource set using Python with Pillow and NumPy:

```sh
python tools/build_sprite_sheets.py --help
```

Keep output separate from the source. The tool emits its integrity report outside the runtime assets. Its input is the prepared game resource set, not a new SWF extraction.

## Intro movie

`tools/pack_intro_movie.py` converts the previously verified version-1 PNG key/XOR intro into `movie.dw2v`. Version 2 of `frames.tsv` points to this single file, with no individual PNG-frame directory. Music and the nineteen sound events remain separate and byte-identical to the verified intro resources.

DW2MOV1 stores independent lossless WebP chunks of at most thirty logical source slots, plus an index. The native decoder loads only the active compressed chunk and copies the requested RGBA canvas into the existing bounded prefetch cache. It does not load the whole movie into memory. It retains the 1280 by 720 export and the authored thirty-frame source clock.

Identical pictures can coalesce into one encoded visual frame. The decoder maps each logical source slot to its visual span using rounded millisecond boundaries, including an entirely static thirty-slot chunk. Random/backward seeks decode at most thirty encoded pictures. The decoder validates canvas size, chunk extents, lossless payloads and time-span coverage. Source-frame numbering, pause, restart, Skip/Back and sound-event dispatch remain in `IntroMovie`.

Runtime decoding uses the pinned libwebp 1.6.0 dependency from `cmake/Webp.cmake`; an encoder executable is not needed to play. Offline encoding uses Pillow/NumPy and requires more preparation time. Neither process-memory savings nor faster playback follow automatically from the smaller file. The original version-1 key/XOR reader remains available for older prepared assets.

The common `intro_probe` exercises both formats, coalesced spans, translucent RGBA, seeks, immutable published pictures and event order, with and without prefetch. The optional Windows `intro_assets_probe` verifies the actual complete movie against all 3,233 exported RGBA hashes and all nineteen events. The portable frontend smoke report separately records `spriteSheets` and `losslessIntro`, and the release workflow requires both.

For input provenance, original framing, screen-fill correction and audio limitations, see [the intro reference](intro-reference.md).
