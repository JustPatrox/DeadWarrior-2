"""Review authored uniform screen fills without modifying captured intro assets.

SVG image URIs are decoded only in memory for constant-colour classification;
neither image data nor path payloads are printed or included in the report.
"""
import argparse
import base64
from collections import Counter, defaultdict
import hashlib
import io
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SVG = "{http://www.w3.org/2000/svg}"
XLINK = "{http://www.w3.org/1999/xlink}"
IDENTITY = [1, 0, 0, 1, 0, 0]
COLOUR = [256, 256, 256, 256, 0, 0, 0, 0]


def rectangle_path(data):
    if set(re.findall("[A-Za-z]", data)) - set("MLZ"):
        return None
    values = list(map(float, re.findall(r"[-+]?(?:\d*\.)?\d+(?:[eE][-+]?\d+)?", data)))
    if len(values) % 2:
        return None
    points = list(zip(values[::2], values[1::2]))
    unique = set(points)
    xs, ys = {p[0] for p in unique}, {p[1] for p in unique}
    if len(xs) != 2 or len(ys) != 2 or len(unique) != 4:
        return None
    if any(a[0] != b[0] and a[1] != b[1] for a, b in zip(points, points[1:] + points[:1])):
        return None
    return [min(xs), max(xs), min(ys), max(ys)]


def inspect_shapes(base, inventory):
    rectangles, solids, bitmaps = {}, {}, []
    root_ids = {event["id"] for event in inventory["timelines"]["0"] if "id" in event}
    large_images = {}
    for number, definition in inventory["symbols"].items():
        if definition["kind"] != "shape":
            continue
        path = base / "extract/shapes" / f"{number}.svg"
        if not path.exists():
            continue
        tree = ET.parse(path).getroot()
        paths = list(tree.iter(SVG + "path"))
        images = list(tree.iter(SVG + "image"))
        gradients = list(tree.iter(SVG + "linearGradient")) + list(tree.iter(SVG + "radialGradient"))
        filled = [p for p in paths if p.get("fill", "none") != "none" or p.get("style", "").startswith("fill:url")]
        colours = {p.get("fill", "") for p in filled}
        if not images and not gradients and len(colours) == 1 and all(re.fullmatch(r"#[0-9A-Fa-f]{6}", c) for c in colours):
            solids[int(number)] = dict(fill=next(iter(colours)), bounds=definition.get("bounds"),
                                       commandSet=sorted({c for p in filled for c in re.findall("[A-Za-z]", p.get("d", ""))}))
            if len(filled) == 1:
                bounds = rectangle_path(filled[0].get("d", ""))
                if bounds:
                    rectangles[int(number)] = dict(fill=next(iter(colours)), fillBounds=bounds,
                                                   shapeBounds=definition["bounds"], pathCount=len(paths),
                                                   svgSHA256=hashlib.sha256(path.read_bytes()).hexdigest())
        if int(number) not in root_ids:
            continue
        for image in images:
            if float(image.get("width", "0")) < 600 or float(image.get("height", "0")) < 200:
                continue
            uri = image.get(XLINK + "href", "")
            if not uri.startswith("data:image"):
                continue
            digest = hashlib.sha256(uri.encode()).hexdigest()
            if digest not in large_images:
                with Image.open(io.BytesIO(base64.b64decode(uri.split(",", 1)[1]))) as decoded:
                    rgba = decoded.convert("RGBA")
                    large_images[digest] = (decoded.size, rgba.getextrema())
            size, extrema = large_images[digest]
            if all(lo == hi for lo, hi in extrema[:3]):
                bitmaps.append(dict(shape=int(number), dimensions=size, rgbaExtrema=extrema))
    return rectangles, solids, bitmaps, len(large_images)


def compose(parent, local):
    a, b, c, d, x, y = parent
    e, f, g, h, u, v = local
    return [a*e+c*f, b*e+d*f, a*g+c*h, b*g+d*h, a*u+c*v+x, b*u+d*v+y]


def bounds_at(bounds, matrix):
    lo, hi, top, bottom = bounds
    a, b, c, d, x, y = matrix
    points = [(a*u+c*v+x, b*u+d*v+y) for u, v in ((lo, top), (lo, bottom), (hi, top), (hi, bottom))]
    return [min(p[0] for p in points), max(p[0] for p in points), min(p[1] for p in points), max(p[1] for p in points)]


def contains(box, target):
    return box[0] <= target[0] and box[1] >= target[1] and box[2] <= target[2] and box[3] >= target[3]


def alpha_of(state):
    colour = state.get("color", COLOUR)
    return min(1, max(0, colour[3]/256 + colour[7]/255))


def update(states, event):
    if event["op"] not in ("place", "move", "remove"):
        return
    depth = event["depth"]
    if event["op"] == "remove":
        states.pop(depth, None)
    else:
        if event["op"] == "place" or depth not in states:
            states[depth] = dict(matrix=IDENTITY, color=COLOUR, birth=event["frame"])
        elif "id" in event and event["id"] != states[depth].get("id"):
            states[depth]["birth"] = event["frame"]
        states[depth].update(event)


def intervals(frames):
    result = []
    for frame in sorted(set(frames)):
        if result and result[-1][1] == frame - 1:
            result[-1][1] = frame
        else:
            result.append([frame, frame])
    return result


def analyze(base, destination):
    inventory = json.loads((base / "inventory.json").read_text())
    captures = {row["frame"]: row for row in json.loads((base / "frames-wide/frames.json").read_text())}
    rectangles, solids, bitmaps, image_count = inspect_shapes(base, inventory)
    timelines, symbols = inventory["timelines"], inventory["symbols"]
    reachable = set(rectangles)
    while True:
        owners = {int(owner) for owner, events in timelines.items() if any(e.get("id") in reachable for e in events)}
        if owners <= reachable:
            break
        reachable |= owners
    cache = {}

    def child_states(owner, frame):
        key = owner, frame
        if key not in cache:
            states = {}
            for event in timelines[str(owner)]:
                if event["frame"] > frame:
                    break
                update(states, event)
            cache[key] = states
        return cache[key]

    def flatten(states, parent, alpha, local_frame, path, stack=()):
        for depth, state in states.items():
            number = state.get("id")
            if number not in reachable:
                continue
            matrix = compose(parent, state.get("matrix", IDENTITY))
            opacity = alpha*alpha_of(state)
            current = path + f"/{depth}:{number}"
            if number in rectangles:
                yield number, current, matrix, opacity
            elif number not in stack and str(number) in timelines:
                length = symbols[str(number)]["frames"]
                frame = (local_frame-state["birth"]) % length+1
                yield from flatten(child_states(number, frame), matrix, opacity, frame, current, stack+(number,))

    by_frame = defaultdict(list)
    for event in timelines["0"]:
        by_frame[event["frame"]].append(event)
    states, observations, non_rectangular = {}, [], defaultdict(list)
    for frame in range(1, inventory["metadata"]["frames"]+1):
        for event in by_frame[frame]:
            update(states, event)
        if frame not in captures:
            continue
        capture = captures[frame]
        if capture["rotation"] != 0:
            raise ValueError("This review requires explicit handling of a rotated captured root")
        root_matrix = [capture["scaleX"], 0, 0, capture["scaleY"], capture["movieX"], capture["movieY"]]
        for number, path, local, opacity in flatten(states, IDENTITY, 1, frame, "root"):
            if opacity <= 0:
                continue
            matrix = compose(root_matrix, local)
            box = bounds_at(rectangles[number]["fillBounds"], matrix)
            content = contains(box, [100, 1180, 127.5, 592.5])
            full = contains(box, [100, 1180, 0, 720])
            left, right = max(0, box[0]), max(0, 1280-box[1])
            axis = abs(matrix[1]) < 1e-10 and abs(matrix[2]) < 1e-10
            observations.append(dict(frame=frame, shape=number, path=path, alpha=opacity,
                                     localMatrix=local, pixelBounds=box, coversOriginalContent=content,
                                     coversOriginalFullHeight=full, axisAligned=axis,
                                     missingLeftPixels=left, missingRightPixels=right,
                                     extendCandidate=axis and content and (left > 0 or right > 0), camera=capture["camera"]))
        for state in states.values():
            number = state.get("id")
            if number in solids and number not in rectangles and alpha_of(state) > 0:
                box = bounds_at(solids[number]["bounds"], compose(root_matrix, state.get("matrix", IDENTITY)))
                if contains(box, [100, 1180, 127.5, 592.5]):
                    non_rectangular[number].append(frame)
    groups = defaultdict(list)
    for row in observations:
        groups[row["path"]].append(row)
    summaries = []
    for path, rows in groups.items():
        affected = [row for row in rows if row["extendCandidate"]]
        summaries.append(dict(shape=rows[0]["shape"], path=path, visibleFrames=intervals(r["frame"] for r in rows),
                              fullContentFrames=intervals(r["frame"] for r in rows if r["coversOriginalContent"]),
                              affectedFrames=intervals(r["frame"] for r in affected), affectedFrameCount=len(affected),
                              maximumLeftGapPixels=max(r["missingLeftPixels"] for r in affected) if affected else 0,
                              maximumRightGapPixels=max(r["missingRightPixels"] for r in affected) if affected else 0,
                              examples=affected[:1]))
    uses = {}
    for number in rectangles:
        uses[number] = [dict(owner=int(owner), frames=[e["frame"] for e in events if e.get("id") == number])
                        for owner, events in timelines.items() if any(e.get("id") == number for e in events)]
    report = dict(sourceSWFSHA256=inventory["metadata"]["sha256"], rootFrameCount=3235,
                  rootReferencedKinds=dict(Counter(symbols[str(e)]["kind"] for e in {x["id"] for x in timelines["0"] if "id" in x})),
                  uniformRectangles=rectangles, uniformRectangleUses=uses,
                  largeEmbeddedImagesExamined=image_count, constantRgbBitmapCandidates=bitmaps,
                  originalFieldPixels=[100, 1180, 0, 720], originalContentBandPixels=[100, 1180, 127.5, 592.5],
                  screenFillInstances=summaries, allUniformObservations=observations,
                  otherSolidNonRectangleScreenSizeBounds={str(k):dict(definition=solids[k], frames=intervals(v)) for k, v in non_rectangular.items()},
                  modelScope="SWF display-list matrices, including measured root/VCam transforms from the completed Adobe capture. Child frame scripts are not emulated; nested relevant screen-fill 2028 has one frame.",
                  assetsModified=False, correctedAdobeCapturePerformed=False)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(dict(report=str(destination), uniformRectangleDefinitions=len(rectangles),
                          affectedInstances=[{k:row[k] for k in ("shape", "path", "affectedFrames", "affectedFrameCount", "maximumLeftGapPixels", "maximumRightGapPixels")} for row in summaries if row["affectedFrameCount"]],
                          otherSolidNonRectangles=list(non_rectangular)), indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, default=ROOT / "work/trailer-source")
    parser.add_argument("--report", type=Path, default=ROOT / "work/trailer-source/screen-fill-review.json")
    args = parser.parse_args()
    analyze(args.base, args.report)
