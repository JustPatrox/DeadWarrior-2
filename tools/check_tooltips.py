"""Verify the extracted original recruitment tooltips in native Windows scenes."""
import json
import numpy as np
from PIL import Image
from check_castle_ui import ROOT,run


def check(width,height):
    run(width,height,tooltips_only=True)
    out=ROOT/'evidence/windows-castle-todo';reports=[]
    for frame,name in enumerate(('miner','sword','spearton','archer','ninja','wizard','healer','giant'),20001):
        actual=np.asarray(Image.open(out/f'{width}x{height}-{name}-tooltip.png').convert('RGB')).astype(float)
        art=Image.open(ROOT/f'assets/tooltips/1119-{frame}.png').convert('RGBA');s=height/700
        size=(round(art.width*s/1.5),round(art.height*s/1.5));art=art.resize(size,Image.Resampling.BILINEAR)
        expected=Image.new('RGBA',(width,height));expected.paste(art,(round(width/2+(1.65-425)*s),round(370.6*s)))
        rgba=np.asarray(expected);yy,_=np.indices(rgba.shape[:2])
        # Original UserInterface sits above TipBox, so its lower portion is covered by the HUD.
        mask=(rgba[:,:,3]>=254)&(yy<565*s)
        error=float(np.abs(actual-rgba[:,:,:3].astype(float))[mask].mean())
        assert error<18,(name,error)
        reports.append(dict(name=name,sourceSymbol=1119,meanOpaquePixelError=round(error,3)))
    (out/f'{width}x{height}-original-tooltip-checks.json').write_text(json.dumps(reports,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(reports,flush=True)


if __name__=='__main__':
    check(1280,720)
    check(1920,1080)
