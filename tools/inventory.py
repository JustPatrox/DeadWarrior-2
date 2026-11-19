"""Index original definitions, linkages and display list deltas. Coordinates are in stage units."""
import argparse
import hashlib
import json
import struct
import zlib
from collections import Counter
from pathlib import Path


class Reader:
    def __init__(self, data):
        self.data = data
        self.bit = 0

    def bits(self, count, signed=False):
        value = 0
        for _ in range(count):
            value = value * 2 + ((self.data[self.bit // 8] >> (7 - self.bit % 8)) & 1)
            self.bit += 1
        if signed and count and value >= 1 << (count - 1):
            value -= 1 << count
        return value

    def take(self, count):
        start = (self.bit + 7) // 8
        end = start + count
        if end > len(self.data):
            raise ValueError("Field extends past end of SWF tag")
        self.bit = end * 8
        return self.data[start:end]

    def uint(self, count):
        return int.from_bytes(self.take(count), "little")

    def string(self):
        start = (self.bit + 7) // 8
        end = self.data.index(b"\0", start)
        return self.take(end - start + 1)[:-1].decode("utf-8", "replace")

    def rect(self):
        size = self.bits(5)
        result = [self.bits(size, True) / 20 for _ in range(4)]
        self.take(0)
        return result

    def matrix(self):
        def pair(scale, default):
            if not self.bits(1):
                return default, default
            n = self.bits(5)
            return self.bits(n, True) / scale, self.bits(n, True) / scale

        a, d = pair(65536, 1)
        b, c = pair(65536, 0)
        size = self.bits(5)
        x, y = self.bits(size, True) / 20, self.bits(size, True) / 20
        self.take(0)
        return [a, b, c, d, x, y]

    def color_transform(self, channels):
        add, mul, size = self.bits(1), self.bits(1), self.bits(4)
        factors = [self.bits(size, True) for _ in range(channels)] if mul else [256] * channels
        offsets = [self.bits(size, True) for _ in range(channels)] if add else [0] * channels
        self.take(0)
        return factors + offsets


KINDS = {2:"shape", 6:"bitmap", 7:"button", 10:"font", 11:"text", 14:"sound",
         20:"bitmap", 21:"bitmap", 22:"shape", 32:"shape", 33:"text", 34:"button",
         35:"bitmap", 36:"bitmap", 37:"editText", 39:"sprite", 46:"morph",
         48:"font", 60:"video", 75:"font", 83:"shape", 84:"morph", 87:"binary",
         90:"bitmap", 91:"font"}


def placement(reader, code):
    if code == 4:
        symbol, depth = reader.uint(2), reader.uint(2)
        return {"op":"place", "depth":depth, "id":symbol, "matrix":reader.matrix()}
    flags = reader.uint(1)
    extra = reader.uint(1) if code in (70, 94) else 0
    row = {"op":"move" if flags & 1 else "place", "depth":reader.uint(2)}
    if extra & 8:
        row["class"] = reader.string()
    elif flags & 2:
        row["id"] = reader.uint(2)
    if flags & 4:
        row["matrix"] = reader.matrix()
    if flags & 8:
        row["color"] = reader.color_transform(4)
    if flags & 16:
        row["ratio"] = reader.uint(2)
    if flags & 32:
        row["name"] = reader.string()
    if flags & 64:
        row["clipDepth"] = reader.uint(2)
    if extra or flags & 128:
        row["extendedFlags"] = extra
        row["remainingHex"] = reader.data[reader.bit // 8:].hex()
    return row


def index(source):
    raw = source.read_bytes()
    if raw[:3] != b"CWS":
        raise ValueError("Expected the compressed Stick War 2 source")
    body = zlib.decompress(raw[8:])
    if len(body) + 8 != struct.unpack_from("<I", raw, 4)[0]:
        raise ValueError("SWF length mismatch")
    header = Reader(body)
    stage, rate, frames = header.rect(), header.uint(2) / 256, header.uint(2)
    symbols, linkage, timelines, abc, counts = {}, {}, {}, [], Counter()
    meta = dict(sha256=hashlib.sha256(raw).hexdigest(), stage=stage, fps=rate,
                frames=frames, version=raw[3], bytes=len(raw))

    def scan(data, owner, total):
        cursor = 0
        frame = 1
        events = []
        while cursor < len(data):
            word = struct.unpack_from("<H", data, cursor)[0]
            cursor += 2
            code, size = word >> 6, word & 63
            if size == 63:
                size = struct.unpack_from("<I", data, cursor)[0]
                cursor += 4
            payload = data[cursor:cursor+size]
            cursor += size
            counts[code] += 1
            r = Reader(payload)
            event = None
            if code in KINDS:
                symbol = r.uint(2)
                entry = {"kind":KINDS[code], "tag":code}
                symbols[symbol] = entry
                if entry["kind"] in ("shape", "morph", "text", "editText"):
                    entry["bounds"] = r.rect()
                if code == 39:
                    entry["frames"] = r.uint(2)
                    scan(payload[4:], symbol, entry["frames"])
            elif code in (56, 76):
                for _ in range(r.uint(2)):
                    symbol, name = r.uint(2), r.string()
                    linkage.setdefault(symbol, []).append(name)
            elif code in (4, 26, 70, 94):
                event = placement(r, code)
            elif code in (5, 28):
                if code == 5:
                    r.uint(2)
                event = {"op":"remove", "depth":r.uint(2)}
            elif code == 43:
                event = {"op":"label", "name":r.string()}
            elif code in (15, 89):
                event = {"op":"sound", "id":r.uint(2)} if code == 15 else {"op":"soundClass", "name":r.string()}
            elif code == 1:
                frame += 1
            elif code == 9 and owner == 0:
                meta["background"] = "#" + payload.hex()
            elif code == 69:
                meta["actionScript3"] = bool(r.uint(4) & 8)
            elif code == 82:
                r.uint(4)
                abc.append(r.string())
            if event is not None:
                events.append(dict(frame=frame, **event))
        if frame - 1 != total:
            raise ValueError(f"Frame count mismatch on timeline {owner}")
        timelines[owner] = events

    scan(body[header.bit // 8:], 0, frames)
    meta.update(symbols=len(symbols), timelines=len(timelines), abcBlocks=len(abc))
    return dict(metadata=meta, tagCounts=counts, symbols=symbols, linkage=linkage, timelines=timelines, abc=abc)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = index(args.source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, separators=(",", ":")), encoding="utf-8")
    print(json.dumps(result["metadata"], indent=2))
