"""Compare the centered 850-unit camera with the same Adobe post-victory screens."""
import json
from pathlib import Path

import numpy as np
from PIL import Image


def compare():
    base=Path(__file__).resolve().parents[1]/"evidence"
    reports={}
    for width,height in ((1280,720),(1920,1080)):
        stage_width=850*height/700
        offset=(width-stage_width)/2
        pages=["upgrades","confirm-upgrades","archidon-map"]
        if width==1280: pages.append("rage")
        for page in pages:
            name=f"{width}x{height}-{page}"
            with Image.open(base/"windows-tutorial"/(name+".png")) as im:
                native=im.convert("RGB")
            with Image.open(base/"adobe-intermission"/(name+".png")) as im:
                original=np.asarray(im.convert("RGB"),dtype=float)[:,:round(stage_width)]
            core=native.transform((round(stage_width),height),Image.Transform.AFFINE,(1,0,offset,0,1,0),
                                  Image.Resampling.BILINEAR)
            diff=np.abs(np.asarray(core,dtype=float)-original)
            full=np.asarray(native)
            clear=np.all(full==[255,0,255],axis=2)
            edges=np.concatenate((clear[:,:int(offset)],clear[:,width-int(offset):]),axis=1)
            report={"meanAbsoluteRGB":round(float(diff.mean()),3),
                    "percentPixelsWithMeanErrorAbove40":round(100*float((diff.mean(axis=2)>40).mean()),3),
                    "magentaPixels":int(clear.sum()),"magentaOutsideOriginalFrame":int(edges.sum())}
            reports[name]=report
            if report["magentaPixels"] or report["meanAbsoluteRGB"]>6 or report["percentPixelsWithMeanErrorAbove40"]>3:
                raise AssertionError((name,report))
    (base/"adobe-intermission/comparison.json").write_text(json.dumps(reports,indent=2)+"\n")
    for name,r in reports.items(): print(name,r["meanAbsoluteRGB"])


if __name__=="__main__": compare()
