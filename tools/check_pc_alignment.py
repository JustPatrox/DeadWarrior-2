"""Check native PC panel centering, shifted click targets and toolbar label."""
import ctypes as ct
from ctypes import wintypes as wt
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
import numpy as np
from native_client import capture_from_game
from reference_projector import user, window

ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get('DW2_TEST_EXE',ROOT/'out/windows-x64/DeadWarrior 2.exe'))
OUT=ROOT/'evidence/windows-pc-alignment'


def run():
    OUT.mkdir(parents=True,exist_ok=True)
    capture=OUT/'capture.png'
    trace=OUT/'steps.tsv';trace.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory() as temporary:
        folder=Path(temporary)
        (folder/'campaign.sav').write_text('DeadWarrior2-Campaign 1\n7 7 1 000000000000000000010\n'+'220 220 1\n'*7+'-1 0 0\n'*7)
        with subprocess.Popen([EXE,'--save-dir',folder,'--capture-client',capture,'--trace-tutorial',trace]) as game:
            try:
                deadline=time.monotonic()+15
                while time.monotonic()<deadline:
                    hwnd=next((h for h,c in window(game.pid) if c=='DeadWarrior2Native'),None)
                    if hwnd:break
                    time.sleep(.1)
                else:raise AssertionError('Native window missing')
                def query(n):return user.SendMessageW(hwnd,0x8000+n,0,0)
                def key(code):user.SendMessageW(hwnd,0x100,code,0)
                def size():
                    r=wt.RECT();user.GetClientRect(hwnd,ct.byref(r));return r.right,r.bottom
                def options():return (folder/'options.ini').read_text()
                def click(x,y,settings=False):
                    w,h=size();lw,lh=query(23)/100,query(24)/100
                    if settings:y+=lh/2-409
                    if 'aspect_mode=stretched' in options():sx,sy=w/lw,h/lh
                    else:sx=sy=min(w/lw,h/lh)
                    px=round(w/2+(x-425)*sx);py=round((h-lh*sy)/2+y*sy)
                    for message,b in ((0x200,0),(0x201,1),(0x202,0)):
                        user.SendMessageW(hwnd,message,b,(py<<16)|(px&65535))
                def resize(w,h):
                    style=user.GetWindowLongW(hwnd,-16)
                    user.SetWindowLongW(hwnd,-16,(style&~0x00cf0000)|0x80000000)
                    user.SetWindowPos(hwnd,0,0,0,w,h,0x34);time.sleep(.25)
                def shot(name):
                    im=capture_from_game(hwnd,capture);im.save(OUT/f'{name}.png');return im
                time.sleep(11)
                # Initialize options before coordinate conversion.
                key(0x2d);key(0x23);key(0x0d);key(0x1b)
                click(161,664);time.sleep(1.2);click(578,664)
                deadline=time.monotonic()+15
                while time.monotonic()<deadline:
                    rows=trace.read_text().splitlines() if trace.exists() else []
                    if rows and rows[-1].split('\t')[:2]==['101','401']:break
                    time.sleep(.1)
                else:raise AssertionError('Campaign map did not reach its flag')
                click(425,300)
                deadline=time.monotonic()+15
                while time.monotonic()<deadline and query(22)<0:time.sleep(.1)
                assert query(22)>=0,'Battle did not open'
                resize(1280,720);shot('pc-label-720');click(344,683)
                assert query(21)&2,'PC toolbar button did not open settings'
                checks=[]
                for aspect,dimensions in [('16:9',[(1280,720),(1920,1080),(1280,1000)]),
                                          ('stretched',[(1280,900)]),
                                          ('dynamic',[(1920,1009),(2560,1080),(1024,768),(720,1280)])]:
                    while 'aspect_mode='+aspect not in options():click(500,643,True);time.sleep(.1)
                    for w,h in dimensions:
                        resize(w,h)
                        im=shot(f'{aspect.replace(":","-")}-{w}x{h}');rgb=np.asarray(im).astype(int)
                        gold=np.max(np.abs(rgb-np.array([177,138,50])),axis=2)<=2
                        lw,lh=query(23)/100,query(24)/100
                        sx,sy=(w/lw,h/lh) if aspect=='stretched' else (min(w/lw,h/lh),)*2
                        ys=np.flatnonzero(gold.sum(axis=1)>464*sx*.7)
                        xs=np.flatnonzero(gold.sum(axis=0)>578*sy*.7)
                        assert len(xs)>1 and len(ys)>1,(aspect,w,h)
                        box=[int(xs[0]),int(ys[0]),int(xs[-1])+1,int(ys[-1])+1]
                        error=[abs((box[0]+box[2])/2-w/2),abs((box[1]+box[3])/2-h/2)]
                        assert max(error)<=1.5,(aspect,w,h,box,error)
                        before='mouse_lock=1' in options();click(588,603,True)
                        assert ('mouse_lock=1' in options())!=before,'Shifted mouse-lock hit area failed'
                        click(588,603,True)
                        checks.append(dict(aspect=aspect,resolution=[w,h],bounds=box,centerErrorPixels=error,clicksAligned=True))
                        print(checks[-1],flush=True)
                key(0x1b);resize(1920,1080);shot('pc-label-1080')
                (OUT/'checks.json').write_text(json.dumps(checks,indent=2)+'\n')
                user.PostMessageW(hwnd,0x10,0,0);assert game.wait(timeout=8)==0
            finally:
                if game.poll() is None:game.terminate()


if __name__=='__main__':run()
