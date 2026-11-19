"""Exercise Windows x64 renderer and compare the settled menu with the running SWF."""
import json
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image


def check():
    exe = Path("out/windows-x64/DeadWarrior 2.exe").resolve()
    out = Path("evidence/windows-menu")
    out.mkdir(exist_ok=True, parents=True)
    checks = []
    for w,h in ((1280,720),(1920,1080)):
        for tick in (0,22,59,96,97,119,123,149,173,174,239):
            name = out/f"native-{w}x{h}-tick{tick}.png"
            subprocess.run([exe,"--capture",str(w),str(h),str(tick),name,"--magenta"],check=True)
            img = np.asarray(Image.open(name).convert("RGB"))
            holes = int(np.all(img == [255,0,255],axis=2).sum())
            checks.append(dict(size=[w,h],tick=tick,uncovered=holes))
            if holes:
                raise AssertionError(f"{name}: {holes} magenta pixels")
        normal = out/f"native-{w}x{h}-main.png"
        subprocess.run([exe,"--capture",str(w),str(h),"239",normal],check=True)
        native = np.asarray(Image.open(normal).convert("RGB"),dtype=np.int16)
        reference = np.asarray(Image.open(f"evidence/reference/swf-web-{w}x{h}-18s.png").convert("RGB"),dtype=np.int16)
        # Ruffle executes the SWF's TOP_LEFT stage alignment. Measure the actual
        # right border of its painting to avoid assuming the player scale.
        white_cols = np.all(reference == 255,axis=(0,2))
        painted = np.where(~white_cols)[0]
        scale = (painted[-1]+1)/850
        if abs(700*scale-h)>2:
            checks.append(dict(size=[w,h],comparison="different reference scale",measuredScale=scale))
            continue
        # Exclude reanchored music, version, credits and the animated bottom panels.
        s=h/700
        left=(w-850*s)/2
        x0,x1,y0,y1=map(round,(30*s,820*s,80*s,615*s))
        ref=reference[y0:y1,x0:x1]
        got=native[y0:y1,round(left)+x0:round(left)+x1]
        diff=np.abs(ref-got)
        checks.append(dict(size=[w,h],comparison="settled painting and logos",meanAbsRGB=round(float(diff.mean()),3),
                           p95AbsRGB=float(np.percentile(diff,95)),sourceLeft=0,nativeLeft=round(left)))
    (out/"checks.json").write_text(json.dumps(checks,indent=2),encoding="utf-8")
    print(json.dumps([c for c in checks if "comparison" in c],indent=2))


if __name__ == "__main__":
    check()
