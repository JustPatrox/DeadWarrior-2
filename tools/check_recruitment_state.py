"""Compare native ready/unaffordable/refunded recruitment graphics and clicks."""
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
OUT=ROOT/'evidence/windows-recruitment-state'


def run():
    OUT.mkdir(parents=True,exist_ok=True)
    cost=float(dict(line.split() for line in (ROOT/'assets/tutorial/rules.tsv').read_text().splitlines())['archer.gold'])
    trace=OUT/'steps.tsv';trace.unlink(missing_ok=True);capture=OUT/'capture.png'
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
                width,height=1280,720
                def send(message,x,y,button=0):
                    px=round(width/2+(x-425)*height/700);py=round(y*height/700)
                    user.SendMessageW(hwnd,message,button,(py<<16)|(px&65535))
                def click(x,y):
                    for msg,b in ((0x200,0),(0x201,1),(0x202,0)):send(msg,x,y,b)
                def row():
                    rows=trace.read_text().splitlines() if trace.exists() else []
                    return list(map(float,rows[-1].split('\t'))) if rows else []
                def wait(check,limit=20):
                    deadline=time.monotonic()+limit
                    while time.monotonic()<deadline:
                        if game.poll() is not None:raise AssertionError('Native process exited')
                        r=row()
                        if r and check(r):return r
                        time.sleep(.05)
                    raise AssertionError(row())
                def shot(name):
                    send(0x200,425,550)
                    im=capture_from_game(hwnd,capture);im.save(OUT/f'{width}x{height}-{name}.png')
                    a=np.asarray(im)
                    x0=round(width/2+(127-425)*height/700);x1=round(width/2+(163-425)*height/700)
                    return a[round(600*height/700):round(629*height/700),x0:x1].astype(float)
                style=user.GetWindowLongW(hwnd,-16);user.SetWindowLongW(hwnd,-16,(style&~0xcf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                time.sleep(11);click(161,664);time.sleep(1.2);click(578,664)
                wait(lambda r:r[:2]==[101,401]);click(425,300);wait(lambda r:r[0]==27)
                checks=[]
                for width,height in ((1280,720),(1920,1080)):
                    user.SetWindowPos(hwnd,0,0,0,width,height,0x34);time.sleep(.2)
                    wait(lambda r:len(r)>62 and r[5]>=cost)
                    ready=shot('ready')
                    for _ in range(8):
                        r=row()
                        if r[5]<150:break
                        before=r[5];click(30,610);wait(lambda r:r[5]<before)
                    wait(lambda r:r[5]<cost)
                    unavailable=shot('insufficient-gold')
                    assert ready.mean()-unavailable.mean()>12,'Affordable icon remained shaded'
                    for _ in range(6):
                        if row()[5]>=cost:break
                        before=row()[5];click(51,595);wait(lambda r:r[5]>before)
                    wait(lambda r:r[5]>=cost)
                    refunded=shot('refunded-ready')
                    assert np.abs(ready-refunded).mean()<1,'Refund did not restore the available artwork'
                    before=row()[56];click(146,610);wait(lambda r:r[56]==before+1)
                    click(166,595);wait(lambda r:r[56]==before)
                    # Restore the remaining queued Miners for the next size.
                    for _ in range(8):
                        if row()[54]<=0:break
                        previous=row()[54];click(51,595);wait(lambda r:r[54]<previous)
                    checks.append(dict(resolution=[width,height],readyBrightness=float(ready.mean()),
                        unavailableBrightness=float(unavailable.mean()),refundRestored=True,recruitClickWorks=True))
                    print(checks[-1],flush=True)
                (OUT/'checks.json').write_text(json.dumps(checks,indent=2)+'\n')
                user.PostMessageW(hwnd,0x10,0,0);assert game.wait(timeout=8)==0
            finally:
                if game.poll() is None:game.terminate()


if __name__=='__main__':run()
