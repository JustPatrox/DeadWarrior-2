"""Compare the original Adobe castle fixture with castle_probe's native layers."""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image


def compare(reference, native):
    cases = json.loads((reference / 'bounds.json').read_text(encoding='utf-8'))
    results = []
    source = {}
    for case in cases:
        name, tick = case['name'], case['tick']
        stem = f'{name}-{tick}'
        original = np.asarray(Image.open(reference / (stem + '.png')).convert('RGBA')).astype(np.float32)
        original = original[:, :, :3] * original[:, :, 3:4] / 255
        source[name, tick] = original
        x, y, width, height = map(int, (native / (stem + '.txt')).read_text().split())
        pixels = np.fromfile(native / (stem + '.rgba'), dtype=np.uint8).reshape(height, width, 4)
        canvas = np.zeros((case['height'], case['width'], 4), dtype=np.uint8)
        left, top = x - case['x'], y - case['y']
        assert left >= 0 and top >= 0 and left + width <= canvas.shape[1] and top + height <= canvas.shape[0], stem
        canvas[top:top + height, left:left + width] = pixels
        Image.fromarray(canvas).save(native / (stem + '.png'))
        actual = canvas[:, :, :3].astype(np.float32) * canvas[:, :, 3:4] / 255
        error = np.abs(actual - original)
        animation = np.max(np.abs(original - source['neutral', tick]), axis=2) > 3 if name != 'neutral' else np.zeros(original.shape[:2], dtype=bool)
        result = dict(name=name, tick=tick, meanError=round(float(error.mean()), 4),
                      animationError=round(float(error[animation].mean()), 4) if animation.any() else 0)
        results.append(result)
    (native / 'comparison.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8', newline='\n')
    assert len(results) == 49, len(results)
    assert max(r['meanError'] for r in results) < 1.5, results
    assert max(r['animationError'] for r in results) < 8, results
    print(json.dumps(dict(cases=len(results), maxMeanError=max(r['meanError'] for r in results),
                          maxAnimationError=max(r['animationError'] for r in results))))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, default=Path('evidence/adobe-castle'))
    parser.add_argument('--native', type=Path, default=Path('evidence/native-castle'))
    args = parser.parse_args()
    compare(args.reference, args.native)
