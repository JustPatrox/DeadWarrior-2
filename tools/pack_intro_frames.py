"""Pack captured opaque intro frames as lossless PNG keys and RGB XOR deltas.

The final manifest is written only after the complete capture manifest exists
and every packed frame has been reconstructed and compared with its source.

Run ``python tools/pack_intro_frames.py --follow`` while reference_trailer.py
captures the sequence, or omit --follow after capture has finished. K files
are byte-for-byte copies of their captured PNGs. D files contain the RGB XOR
against the preceding decoded frame, with alpha fixed at 255. A decoder XORs
only RGB, leaves alpha opaque and retains at most the previous/current frame.
The first frame is a key; a new key is forced after 29 consecutive deltas.

frames.tsv has three header lines (format, dimensions/rate/range, music path),
then source-frame/kind/relative-file rows. frame-hashes.json maps source frame
numbers to SHA-256 of tightly packed RGBA bytes for offline verification.
No audio files are read or written by this tool.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import time

import numpy as np
from PIL import Image, UnidentifiedImageError

ROOT = Path(__file__).resolve().parents[1]
WIDTH, HEIGHT, FPS = 1280, 720, 30
FIRST, LAST = 3, 3235
MAX_DELTAS = 29
PNG_END = b"\x00\x00\x00\x00IEND\xaeB\x60\x82"


def read_frame(path, encoded=None):
    """Decode one opaque source or delta; the returned array owns its bytes."""
    with Image.open(path if encoded is None else io.BytesIO(encoded)) as image:
        image.load()
        if image.format != "PNG" or image.size != (WIDTH, HEIGHT) or image.mode not in ("RGB", "RGBA"):
            raise ValueError(f"Expected opaque {WIDTH}x{HEIGHT} RGB/RGBA PNG: {path}")
        with image.convert("RGBA") as rgba:
            pixels = np.array(rgba, dtype=np.uint8, copy=True)
    if not np.all(pixels[:, :, 3] == 255):
        raise ValueError(f"Non-opaque intro frame: {path}")
    return pixels


def rgba_digest(pixels):
    return hashlib.sha256(memoryview(pixels)).hexdigest()


def atomic_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + f".tmp-{os.getpid()}")
    try:
        temporary.write_bytes(data)
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def encode_delta(current, previous):
    """Reuse current's buffer for RGB XOR, then restore its original bytes."""
    np.bitwise_xor(current[:, :, :3], previous[:, :, :3], out=current[:, :, :3])
    try:
        encoded = io.BytesIO()
        # frombuffer shares the current RGBA bytes; alpha stays opaque.
        with Image.frombuffer("RGBA", (WIDTH, HEIGHT), current, "raw", "RGBA", 0, 1) as image:
            image.save(encoded, format="PNG", compress_level=6)
        return encoded.getvalue()
    finally:
        np.bitwise_xor(current[:, :, :3], previous[:, :, :3], out=current[:, :, :3])


def wait_for_frame(path, follow, timeout):
    deadline = time.monotonic() + timeout
    while True:
        try:
            raw = path.read_bytes()
            if not raw.endswith(PNG_END):
                raise OSError("Capture PNG does not yet contain its complete IEND chunk")
            return read_frame(path, raw), raw
        except (FileNotFoundError, UnidentifiedImageError, OSError) as error:
            if not follow or time.monotonic() >= deadline:
                raise RuntimeError(f"Capture frame was not completed: {path}") from error
            # Adobe's capture process writes PNGs directly. A visible filename
            # can still be incomplete until that writer closes the file.
            time.sleep(.15)


def capture_manifest(source, expected, follow, timeout):
    path = source / "frames.json"
    deadline = time.monotonic() + timeout
    while True:
        try:
            raw = path.read_bytes()
            rows = json.loads(raw)
            break
        except (FileNotFoundError, json.JSONDecodeError) as error:
            if not follow or time.monotonic() >= deadline:
                raise RuntimeError(f"Final capture manifest missing or incomplete: {path}") from error
            time.sleep(.15)
    if not isinstance(rows, list) or [row.get("frame") for row in rows] != expected:
        raise ValueError("Capture manifest is not the exact ordered source frame range")
    if any(row.get("width") != WIDTH or row.get("height") != HEIGHT for row in rows):
        raise ValueError("Capture manifest dimensions differ from the native intro")
    actual = {p.name for p in source.glob("*.png")}
    wanted = {f"{frame:04}.png" for frame in expected}
    if actual != wanted:
        raise ValueError(f"Capture PNG list differs: missing={sorted(wanted-actual)[:8]}, extra={sorted(actual-wanted)[:8]}")
    return hashlib.sha256(raw).hexdigest()


def verify_frames(source, output, records, hashes):
    """Reconstruct the entire sequence with at most two working raster arrays."""
    previous = None
    streak = maximum = 0
    for index, record in enumerate(records):
        frame, kind, relative = record
        if kind == "K" and (output / relative).read_bytes() != (source / f"{frame:04}.png").read_bytes():
            raise ValueError(f"Key PNG bytes differ from completed source frame {frame}")
        decoded = read_frame(output / relative)
        if kind == "D":
            if previous is None:
                raise ValueError("First intro frame cannot be a delta")
            np.bitwise_xor(previous[:, :, :3], decoded[:, :, :3], out=previous[:, :, :3])
            del decoded
            streak += 1
            maximum = max(maximum, streak)
            if streak > MAX_DELTAS:
                raise ValueError("More than 29 consecutive intro deltas")
        else:
            previous = decoded
            del decoded
            streak = 0
        capture = read_frame(source / f"{frame:04}.png")
        if not np.array_equal(previous, capture):
            raise ValueError(f"Reconstructed RGBA bytes differ at source frame {frame}")
        if rgba_digest(previous) != hashes[str(frame)] or rgba_digest(capture) != hashes[str(frame)]:
            raise ValueError(f"Reconstructed/captured RGBA hash differs at source frame {frame}")
        del capture
        if index % 120 == 0 or index + 1 == len(records):
            print(f"Verified {index+1}/{len(records)} source frame {frame}", flush=True)
    return maximum


def pack(source, output, follow=False, timeout=900, first=FIRST, last=LAST):
    source, output = Path(source).resolve(), Path(output).resolve()
    expected = list(range(first, last + 1))
    if not expected or first < FIRST or last > LAST:
        raise ValueError("Invalid source frame range")
    if source == output or source == output / "frames":
        raise ValueError("Output cannot overwrite the captured source directory")
    (output / "frames").mkdir(parents=True, exist_ok=True)
    records, hashes = [], {}
    previous = None
    streak = keys = deltas = raw_bytes = packed_bytes = 0
    started = time.monotonic()
    for index, frame in enumerate(expected):
        current, raw = wait_for_frame(source / f"{frame:04}.png", follow, timeout)
        digest = rgba_digest(current)
        kind, payload = "K", raw
        if previous is not None and streak < MAX_DELTAS:
            delta = encode_delta(current, previous)
            if rgba_digest(current) != digest:
                raise ValueError(f"Delta encoder changed source frame {frame}")
            if len(delta) < len(raw):
                kind, payload = "D", delta
        relative = f"frames/f{frame:06}.png"
        atomic_write(output / relative, payload)
        records.append((frame, kind, relative))
        hashes[str(frame)] = digest
        raw_bytes += len(raw)
        packed_bytes += len(payload)
        if kind == "K":
            streak = 0
            keys += 1
        else:
            streak += 1
            deltas += 1
        previous = current
        del current
        if index % 30 == 0 or index + 1 == len(expected):
            print(f"Packed {index+1}/{len(expected)} source frame {frame}: K={keys} D={deltas}, bytes={packed_bytes}", flush=True)
    del previous
    capture_hash = capture_manifest(source, expected, follow, timeout)
    maximum = verify_frames(source, output, records, hashes)
    manifest = (f"DeadWarrior2-Intro 1\n{WIDTH} {HEIGHT} {FPS} {len(expected)} {first} {last}\nmusic.wav\n"
                + "".join(f"{frame} {kind} {relative}\n" for frame, kind, relative in records))
    report = dict(format="DeadWarrior2-Intro 1", width=WIDTH, height=HEIGHT, fps=FPS,
                  frameCount=len(expected), firstSourceFrame=first, lastSourceFrame=last,
                  keyFrames=keys, deltaFrames=deltas, maximumConsecutiveDeltas=maximum,
                  rawCapturePngBytes=raw_bytes, packedPngBytes=packed_bytes,
                  packedFraction=packed_bytes/raw_bytes, losslessRgbaVerified=True,
                  allReconstructedFramesCompared=True, captureManifestSHA256=capture_hash,
                  maximumWorkingRasterBuffers=2, maximumWorkingRasterBytes=2*WIDTH*HEIGHT*4,
                  memoryBoundScope="Persistent pixel arrays only; excludes PNG/Pillow codec scratch, compressed data, metadata and Python runtime",
                  elapsedSeconds=round(time.monotonic()-started, 3))
    atomic_write(output / "frame-hashes.json", (json.dumps(hashes, indent=2) + "\n").encode())
    atomic_write(output / "pack-report.json", (json.dumps(report, indent=2) + "\n").encode())
    # Publish the runtime manifest last: partial capture/packing is never ready.
    atomic_write(output / "frames.tsv", manifest.encode())
    print(json.dumps(report, indent=2), flush=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "work/trailer-source/frames-wide")
    parser.add_argument("--output", type=Path, default=ROOT / "assets/intro")
    parser.add_argument("--follow", action="store_true", help="Wait for growing capture PNGs and the final frames.json")
    parser.add_argument("--timeout", type=float, default=900, help="Maximum seconds to wait for each missing/incomplete capture file")
    args = parser.parse_args()
    pack(args.source, args.output, args.follow, args.timeout)


if __name__ == "__main__":
    main()
