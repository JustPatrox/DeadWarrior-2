"""Check native campaign-map coverage and the original source crop at each stop."""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
import csv
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
import numpy as np
from PIL import Image
from native_client import capture_from_game
from reference_projector import user,window

ROOT=Path(__file__).resolve().parents[1]
STOPS=(1,50,92,151,238,298,341,401,441)


def run(width,height,levels):
    out=ROOT/'evidence/windows-map-repair';out.mkdir(exist_ok=True,parents=True);reports=[]
    exe=Path(os.environ.get('DW2_TEST_EXE',ROOT/'out/windows-x64/DeadWarrior 2.exe'))
    for level in levels:
        with tempfile.TemporaryDirectory() as temporary:
            folder=Path(temporary);trace=out/f'{width}x{height}-level{level}-trace.tsv';capture=out/f'{width}x{height}-capture.png'
            trace.unlink(missing_ok=True)
            (folder/'campaign.sav').write_text(f'DeadWarrior2-Campaign 1\n{level} {level} 1 000000000000000000010\n'+'220 220 1\n'*level+'-1 0 0\n'*(14-level),encoding='utf-8')
            with subprocess.Popen([exe,'--save-dir',folder,'--trace-tutorial',trace,'--capture-client',capture,'--magenta']) as game:
                try:
                    deadline=time.monotonic()+15
                    while time.monotonic()<deadline:
                        hwnd=next((h for h,c in window(game.pid) if c=='DeadWarrior2Native'),None)
                        if hwnd:break
                        time.sleep(.1)
                    else:raise AssertionError('Native map window missing')
                    style=user.GetWindowLongW(hwnd,-16);user.SetWindowLongW(hwnd,-16,(style&~0x00cf0000)|0x80000000)
                    user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                    def click(x,y):
                        px=round(width/2+(x-425)*height/700);py=round(y*height/700)
                        for msg,b in ((0x200,0),(0x201,1),(0x202,0)):user.SendMessageW(hwnd,msg,b,(py<<16)|(px&65535))
                    time.sleep(11);click(161,664);time.sleep(1.2);click(578,664)
                    deadline=time.monotonic()+15
                    while time.monotonic()<deadline:
                        rows=trace.read_text(encoding='utf-8').splitlines() if trace.exists() else []
                        if rows and rows[-1].split('\t')[:2]==['101',str(STOPS[level])]:break
                        if game.poll() is not None:raise AssertionError(f'Game exited {game.returncode}')
                        time.sleep(.1)
                    else:raise AssertionError(('Map did not arrive',level,rows[-1:] ))
                    picture=capture_from_game(hwnd,capture);picture.save(out/f'{width}x{height}-level{level}.png')
                    pixels=np.asarray(picture);assert not np.any(np.all(pixels==[255,0,255],axis=2))
                    name={1:'intermission',2:'archidon-map',3:'shadow-map',4:'magic-map'}.get(level,f'campaign-map-{level+1}')
                    assets=exe.parent/'assets'/name
                    manifest=list(csv.reader((assets/'textures.tsv').open(encoding='utf-8'),delimiter='\t'))
                    record=next(r for r in manifest if r[:2]==['436',str(STOPS[level])])
                    motion=(assets/'map-motion.tsv').read_text(encoding='utf-8').splitlines()[-1].split()
                    _,zoom,x,y,*_=map(float,motion);_,_,file,bx,by,bw,bh=record
                    bx,by,bw,bh=map(float,(bx,by,bw,bh));scale=height/700
                    image=Image.open(assets/file).convert('RGB')
                    expected=Image.new('RGB',(width,height))
                    size=(round(bw*zoom*scale),round(bh*zoom*scale))
                    expected.paste(image.resize(size,Image.Resampling.BILINEAR),
                        (round(width/2+(x+bx*zoom-425)*scale),round((y+by*zoom)*scale)))
                    actual=np.asarray(picture).astype(float);reference=np.asarray(expected).astype(float)
                    yy,xx=np.indices(actual.shape[:2]);sx=(xx-width/2)/scale+425;sy=yy/scale
                    mask=(sy>140)&(sy<550)&((sx<300)|(sx>555))
                    error=float(np.abs(actual-reference)[mask].mean())
                    assert error<8,(level,error)
                    report=dict(level=level,mapFrame=STOPS[level],resolution=[width,height],sourceCropMeanError=round(error,3),magentaPixels=0)
                    reports.append(report);print(report,flush=True)
                    user.PostMessageW(hwnd,0x10,0,0);assert game.wait(timeout=8)==0
                finally:
                    if game.poll() is None:game.terminate()
    (out/f'{width}x{height}-checks.json').write_text(json.dumps(reports,indent=2)+'\n',encoding='utf-8',newline='\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('width',type=int);parser.add_argument('height',type=int)
    parser.add_argument('--levels',type=int,nargs='+',default=list(range(1,8)))
    args=parser.parse_args();run(args.width,args.height,args.levels)
