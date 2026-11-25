"""Exercise lossless intro packing, key intervals and an incomplete live capture."""
import contextlib
import io
import json
import os
from pathlib import Path
import tempfile
import threading
import time
import unittest

import numpy as np
from PIL import Image

from pack_intro_frames import HEIGHT, WIDTH, pack


class IntroPackerChecks(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="DeadWarrior2-intro-packer-")
        self.root = Path(self.temporary.name)
        self.source = self.root / "capture"
        self.source.mkdir()
        self.output = self.root / "packed"
        self.rgb = np.random.default_rng(31).integers(0, 256, size=(HEIGHT, WIDTH, 3), dtype=np.uint8)

    def tearDown(self):
        self.temporary.cleanup()

    def manifest(self, frames):
        (self.source / "frames.json").write_text(json.dumps([dict(frame=n, width=WIDTH, height=HEIGHT) for n in frames]))

    def image(self, frame, pixels=None):
        Image.fromarray(self.rgb if pixels is None else pixels).save(self.source / f"{frame:04}.png")

    def run_pack(self, **options):
        with contextlib.redirect_stdout(io.StringIO()):
            return pack(self.source, self.output, **options)

    def test_three_frames_are_reconstructed(self):
        self.image(3)
        changed = self.rgb.copy()
        changed[300:303, 400:403] ^= 127
        self.image(4, changed)
        self.image(5)
        self.manifest([3, 4, 5])
        report = self.run_pack(first=3, last=5)
        lines = (self.output / "frames.tsv").read_text().splitlines()
        self.assertEqual(lines[:3], ["DeadWarrior2-Intro 1", "1280 720 30 3 3 5", "music.wav"])
        self.assertEqual([line.split()[1] for line in lines[3:]], ["K", "D", "D"])
        self.assertTrue(report["losslessRgbaVerified"])
        self.assertEqual(report["maximumWorkingRasterBuffers"], 2)
        self.assertEqual(len(json.loads((self.output / "frame-hashes.json").read_text())), 3)

    def test_no_more_than_29_deltas(self):
        self.image(3)
        for frame in range(4, 36):
            os.link(self.source / "0003.png", self.source / f"{frame:04}.png")
        self.manifest(list(range(3, 36)))
        report = self.run_pack(first=3, last=35)
        rows = [line.split() for line in (self.output / "frames.tsv").read_text().splitlines()[3:]]
        self.assertEqual([int(row[0]) for row in rows if row[1] == "K"], [3, 33])
        self.assertEqual(report["maximumConsecutiveDeltas"], 29)

    def test_follow_does_not_publish_an_incomplete_capture(self):
        self.image(4)
        self.image(5)
        encoded = (self.source / "0004.png").read_bytes()
        (self.source / "0003.png").write_bytes(encoded[:100])
        errors = []

        def complete_capture():
            try:
                time.sleep(.25)
                (self.source / "0003.png").write_bytes(encoded)
                time.sleep(.25)
                self.assertFalse((self.output / "frames.tsv").exists())
                self.manifest([3, 4, 5])
            except Exception as error:
                errors.append(error)

        writer = threading.Thread(target=complete_capture)
        writer.start()
        report = self.run_pack(first=3, last=5, follow=True, timeout=5)
        writer.join()
        self.assertEqual(errors, [])
        self.assertEqual(report["frameCount"], 3)

    def test_wrong_capture_manifest_never_publishes(self):
        for frame in (3, 4, 5):
            self.image(frame)
        self.manifest([3, 5, 4])
        with self.assertRaisesRegex(ValueError, "exact ordered"):
            self.run_pack(first=3, last=5)
        self.assertFalse((self.output / "frames.tsv").exists())

    def test_transparency_is_rejected(self):
        rgba = np.empty((HEIGHT, WIDTH, 4), dtype=np.uint8)
        rgba[:, :, :3] = self.rgb
        rgba[:, :, 3] = 255
        rgba[10, 10, 3] = 0
        self.image(3, rgba)
        self.manifest([3])
        with self.assertRaisesRegex(ValueError, "Non-opaque"):
            self.run_pack(first=3, last=3)
        self.assertFalse((self.output / "frames.tsv").exists())


if __name__ == "__main__":
    unittest.main()
