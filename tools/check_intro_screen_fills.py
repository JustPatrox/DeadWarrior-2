"""Compare paired same-EXIT_FRAME screen-fill captures and original alpha phases."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def compare(capture, analysis, original, output):
    rows = json.loads((capture / "frames.json").read_text())
    before_rows = json.loads((capture / "before-frames.json").read_text())
    if [r["frame"] for r in rows] != [r["frame"] for r in before_rows]:
        raise ValueError("Paired source frame lists differ")
    observations = json.loads(analysis.read_text())["allUniformObservations"]
    expected = {}
    required = {}
    for row in observations:
        if row["coversOriginalContent"] and row["axisAligned"]:
            expected.setdefault(row["frame"], []).append((row["shape"], round(row["alpha"], 10)))
            if row["extendCandidate"]:
                required.setdefault(row["frame"], []).append((row["shape"], round(row["alpha"], 10)))
    result = []
    for before, after in zip(before_rows, rows):
        frame = after["frame"]
        if not after.get("screenEffectsPreserved"):
            raise ValueError(f"Original effect matrix or ColorTransform changed at {frame}")
        for key in ("movieX", "movieY", "scaleX", "scaleY", "rotation", "camera", "width", "height"):
            if before[key] != after[key]:
                raise ValueError(f"Original root/VCam transform changed at {frame}: {key}")
        actual_alpha = [(r["id"], round(r["effectiveAlpha"], 10))
                        for r in after["screenEffectAlphas"] if r["effectiveAlpha"] > 0]
        actual_counter = Counter(actual_alpha)
        if (actual_counter - Counter(expected.get(frame, [])) or
                Counter(required.get(frame, [])) - actual_counter):
            raise ValueError(f"Original alpha phase or field recognition differs at {frame}: actual={actual_alpha}, expected={expected.get(frame, [])}")
        path = f"{frame:04}.png"
        with Image.open(capture / "before" / path) as image:
            baseline = np.array(image.convert("RGB"))
        with Image.open(capture / path) as image:
            corrected = np.array(image.convert("RGB"))
        if baseline.shape != (720, 1280, 3) or corrected.shape != baseline.shape:
            raise ValueError(f"Unexpected capture dimensions at {frame}")
        changed = np.any(baseline != corrected, axis=2)
        centre = int(changed[:, 100:1180].sum())
        if centre:
            raise ValueError(f"Original central 1080x720 pixels changed at {frame}: {centre}")
        source_difference = None
        if original and (original / path).exists():
            with Image.open(original / path) as image:
                source = np.array(image.convert("RGB"))
            source_difference = int(np.any(source[:, 100:1180] != baseline[:, 100:1180], axis=2).sum())
        result.append(dict(sourceFrame=frame, originalCentreDifferentPixels=centre,
                           leftDifferentPixels=int(changed[:, :100].sum()), rightDifferentPixels=int(changed[:, 1180:].sum()),
                           entireImageDifferentPixels=int(changed.sum()), baselineVsPriorCaptureCentreDifference=source_difference,
                           originalEffectsPreserved=True, originalAlphaPhasesMatch=True,
                           originalEffectAlphas=after["screenEffectAlphas"]))
    by_frame = {r["sourceFrame"]: r for r in result}
    for frame in (25, 813, 2615, 2920, 3130, 3140, 3235):
        if frame in by_frame and by_frame[frame]["entireImageDifferentPixels"]:
            raise ValueError(f"No-extension/invisible/partial-field control changed at {frame}")
    # A nearly opaque black field over an already-black side can be extended
    # without changing quantized RGB. Its recognition/alpha guards above remain
    # mandatory (including frame 3129). The early black fade at 3053 does expose
    # visible side pixels in this source, in addition to all white-field cases.
    for frame in (5, 12, 24, 102, 136, 793, 812, 2199, 2201, 2921, 3053):
        if frame in by_frame and not by_frame[frame]["entireImageDifferentPixels"]:
            raise ValueError(f"Expected missing lateral field was not extended at {frame}")
    observer = capture / "observer.as"
    if not observer.exists():
        observer = ROOT / "tools/reference/TrailerObserver.as"
    report = dict(capture=str(capture), frameCount=len(result), originalCentrePixelsCompared=len(result)*1080*720,
                  pairedCentrePixelExact=True, originalAlphaPhasesMatch=True, originalEffectPropertiesPreserved=True,
                  cameraAndPartialControlsUnchanged=True, onlyLateralPixelsChanged=True, cases=result,
                  observerSHA256=hashlib.sha256(observer.read_bytes()).hexdigest(),
                  assetsRebuilt=False)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps({k: v for k, v in report.items() if k != "cases"}, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--analysis", type=Path, default=ROOT / "work/trailer-source/screen-fill-review.json")
    parser.add_argument("--original", type=Path, default=ROOT / "work/trailer-source/frames-wide")
    parser.add_argument("--output", type=Path, default=ROOT / "work/intro-screen-fill-checks.json")
    args = parser.parse_args()
    compare(args.capture.resolve(), args.analysis.resolve(), args.original.resolve(), args.output.resolve())
