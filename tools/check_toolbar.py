"""Capture original toolbar hover/press/drag states in the native Windows game."""
import argparse
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


def run(width,height):
    out=ROOT/'evidence/windows-toolbar';out.mkdir(exist_ok=True,parents=True)
    prefix=f'{width}x{height}';capture=out/f'{prefix}-capture.png';trace=out/f'{prefix}-steps.tsv'
    trace.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory() as temporary:
        folder=Path(temporary)
        (folder/'campaign.sav').write_text('DeadWarrior2-Campaign 1\n7 7 1 000000000000000000010\n'+'220 220 1\n'*7+'-1 0 0\n'*7)
        with subprocess.Popen([EXE,'--save-dir',folder,'--capture-client',capture,'--trace-tutorial',trace,'--magenta']) as game:
            try:
                until=time.monotonic()+15
                while time.monotonic()<until:
                    hwnd=next((h for h,c in window(game.pid) if c=='DeadWarrior2Native'),None)
                    if hwnd:break
                    time.sleep(.1)
                else:raise AssertionError('Toolbar test window missing')
                style=user.GetWindowLongW(hwnd,-16);user.SetWindowLongW(hwnd,-16,(style&~0xcf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                def send(msg,x,y,b=0):
                    px=round(width/2+(x-425)*height/700);py=round(y*height/700)
                    user.SendMessageW(hwnd,msg,b,(py<<16)|(px&65535))
                def click(x,y):
                    for msg,b in ((0x200,0),(0x201,1),(0x202,0)):send(msg,x,y,b)
                def row():
                    lines=trace.read_text().splitlines() if trace.exists() else []
                    return lines[-1].split('\t') if lines else []
                def wait(check):
                    until=time.monotonic()+20
                    while time.monotonic()<until:
                        if check():return
                        if game.poll() is not None:raise AssertionError(('Toolbar game exited',game.returncode))
                        time.sleep(.08)
                    raise AssertionError(row())
                def shot(name):
                    frame=capture_from_game(hwnd,capture);frame.save(out/f'{prefix}-{name}.png')
                    assert not np.any(np.all(np.asarray(frame)==[255,0,255],axis=2)),name
                    return np.asarray(frame)
                time.sleep(11);click(161,664);time.sleep(1.2);click(578,664)
                wait(lambda:row()[:2]==['101','401']);click(425,300);wait(lambda:row() and row()[0]=='27')
                captures=0
                for name,x,y in [('defend',425,637),('attack',499.5,634.5),('garrison',350.5,634.5),
                                 ('miners-left',368,660.95),('miners-right',481,660.95),('menu',506,683.5)]:
                    send(0x200,x,y);time.sleep(.16);shot(name+'-hover')
                    send(0x201,x,y,1);time.sleep(.16);shot(name+'-pressed')
                    send(0x200,425,550,1);time.sleep(.1);shot(name+'-drag-out')
                    send(0x200,x,y,1);time.sleep(.1);shot(name+'-drag-back')
                    send(0x202,425,550);send(0x200,425,550);captures+=4
                    assert not user.SendMessageW(hwnd,0x8000+21,0,0)&1,'Cancelled MENU press opened pause'
                click(425,637)
                user.SendMessageW(hwnd,0x100,32,0);user.SendMessageW(hwnd,0x101,32,0)
                wait(lambda:len(row())>45 and int(row()[45])>0)
                send(0x200,813,642);time.sleep(.1);first=shot('action-hover-start')
                time.sleep(.25);second=shot('action-hover-pulse')
                x0=round(width/2+(780-425)*height/700);x1=round(width/2+(846-425)*height/700)
                y0=round(626*height/700);y1=round(658*height/700)
                changed=int(np.any(first[y0:y1,x0:x1]!=second[y0:y1,x0:x1],axis=2).sum())
                assert changed>20,('Original action pulse did not animate',changed)
                send(0x201,813,642,1);time.sleep(.15);shot('action-pressed');send(0x202,425,550)
                send(0x200,30,610);time.sleep(.2);shot('recruit-highlight');captures+=4
                click(506,683.5);wait(lambda:user.SendMessageW(hwnd,0x8000+21,0,0)&1)
                user.SendMessageW(hwnd,0x100,27,0);wait(lambda:not user.SendMessageW(hwnd,0x8000+21,0,0)&1)
                report=dict(resolution=[width,height],simpleButtons=6,heldDragStates=True,actionPulseChangedPixels=changed,
                            cancelledMenuPress=True,menuStillOpensPause=True,recruitHighlight=True,captures=captures,magentaPixels=0)
                (out/f'{prefix}-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
                user.PostMessageW(hwnd,0x10,0,0);assert game.wait(timeout=8)==0;print(report,flush=True)
            finally:
                if game.poll() is None:game.terminate();game.wait(timeout=5)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--width',type=int);parser.add_argument('--height',type=int)
    args=parser.parse_args()
    if args.width and args.height:run(args.width,args.height)
    else:run(1280,720);run(1920,1080)
