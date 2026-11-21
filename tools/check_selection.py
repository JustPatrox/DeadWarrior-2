"""Verify single-click overlap picking and held box selection in the native game."""
import argparse
import csv
import ctypes as ct
from ctypes import wintypes as wt
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
from native_client import capture_from_game
from reference_projector import user,window

ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get('DW2_TEST_EXE',ROOT/'out/windows-x64/DeadWarrior 2.exe'))
OUT=Path(os.environ.get('DW2_TEST_EVIDENCE',ROOT/'work/selection-window-evidence'))

def run(width,height):
    OUT.mkdir(parents=True,exist_ok=True);prefix=f'{width}x{height}'
    capture=OUT/f'{prefix}-capture.png'
    with tempfile.TemporaryDirectory() as temporary:
        with subprocess.Popen([EXE,'--save-dir',temporary,'--capture-client',capture]) as game:
            try:
                hwnd=None
                def wait(test,limit=20):
                    until=time.monotonic()+limit
                    while time.monotonic()<until:
                        if test():return
                        if game.poll() is not None:raise AssertionError('Owned native game exited')
                        time.sleep(.05)
                    raise AssertionError('Selection test condition timed out')
                def find():
                    nonlocal hwnd
                    hwnd=next((h for h,c in window(game.pid) if c=='DeadWarrior2Native'),None)
                    return bool(hwnd)
                wait(find)
                user.SetWindowLongW(hwnd,-16,(user.GetWindowLongW(hwnd,-16)&~0xcf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                def query(n):return user.SendMessageW(hwnd,0x8000+n,0,0)
                def send(msg,p,button=0):
                    vw=query(23)/100;vh=query(24)/100;s=min(width/vw,height/vh)
                    px=round(width/2+(p[0]-425)*s);py=round((height-vh*s)/2+p[1]*s)
                    user.SendMessageW(hwnd,msg,button,(py<<16)|(px&65535))
                def click(p):
                    for msg,b in ((0x200,0),(0x201,1),(0x202,0)):send(msg,p,b)
                def units():
                    query(26)
                    return list(csv.DictReader(capture.with_suffix('.units.tsv').open(encoding='utf-8'),delimiter='\t'))
                def count():return sum(int(u['selected']) for u in units())
                wait(lambda:query(28)==1);time.sleep(1.6)
                for p,page,delay in (((161,664),2,1.3),((437,664),3,1.3),((154,668),4,3.2)):
                    click(p);wait(lambda:query(28)==page);time.sleep(delay)
                click((434,639));wait(lambda:query(22)>=1);time.sleep(.3)
                # Both original tutorial soldiers overlap at this point.
                at=(650,397)
                click(at);assert count()==1,units()
                selected=next(u for u in units() if u['selected']=='1')
                assert selected['id']=='1','Overlap pick did not choose the frontmost soldier'
                capture_from_game(hwnd,capture).save(OUT/f'{prefix}-single.png')
                # Holding without dragging and small motion must stay a single pick.
                send(0x201,at,1);time.sleep(.3);send(0x200,(652,399),1);send(0x202,(652,399))
                assert count()==1
                # A proper held drag selects both. The green marquee is visible only here.
                send(0x201,(580,260),1);send(0x200,(720,510),1)
                assert count()==2,units()
                capture_from_game(hwnd,capture).save(OUT/f'{prefix}-drag.png')
                send(0x202,(720,510));assert count()==2
                # Explicit single picks must replace the group even after tutorial ticks.
                click(at);assert count()==1;time.sleep(.3);assert count()==1
                capture_from_game(hwnd,capture).save(OUT/f'{prefix}-replace-group.png')
                report=dict(resolution=[width,height],overlappingClickOne=True,frontmost=True,heldWithoutDragOne=True,
                            pointerJitterOne=True,heldRectangleTwo=True,releaseKeepsGroup=True,clickReplacesGroup=True)
                (OUT/f'{prefix}-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
                user.PostMessageW(hwnd,0x10,0,0);assert game.wait(timeout=10)==0
                print(report,flush=True)
            finally:
                if game.poll() is None:game.terminate();game.wait(timeout=10)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('width',type=int);parser.add_argument('height',type=int)
    args=parser.parse_args();run(args.width,args.height)
