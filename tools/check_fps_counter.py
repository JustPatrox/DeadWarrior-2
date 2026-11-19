"""Verify the Insert FPS switch against native completed frames and rendered pixels."""
import ctypes as ct
import csv
from ctypes import wintypes as wt
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
import numpy as np
from native_client import capture_from_game
from reference_projector import user,window

ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get("DW2_TEST_EXE",ROOT/"out/windows-x64/DeadWarrior 2.exe"))
OUT=ROOT/"evidence/windows-fps-counter"

def run(width,height):
    prefix=f"{width}x{height}"
    capture=OUT/f"{prefix}-capture.png"
    timing=OUT/f"{prefix}-frames.tsv"
    with tempfile.TemporaryDirectory() as save_dir:
        with subprocess.Popen([EXE,"--save-dir",save_dir,"--capture-client",capture,"--frame-timing",timing]) as process:
            try:
                until=time.monotonic()+15
                while time.monotonic()<until:
                    hwnd=next((h for h,c in window(process.pid) if c=="DeadWarrior2Native"),None)
                    if hwnd: break
                    time.sleep(.1)
                else: raise AssertionError("Native window missing")
                style=user.GetWindowLongW(hwnd,-16)
                user.SetWindowLongW(hwnd,-16,(style&~0x00cf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                def click(x,y):
                    r=wt.RECT(); user.GetClientRect(hwnd,ct.byref(r))
                    scale=min(r.right/(700*16/9),r.bottom/700)
                    px=round(r.right/2+(x-425)*scale); py=round((r.bottom-700*scale)/2+y*scale)
                    for message,button in ((0x200,0),(0x201,1),(0x202,0)):
                        user.SendMessageW(hwnd,message,button,(py<<16)|(px&65535))
                def key(code): user.SendMessageW(hwnd,0x100,code,0)
                def fps(): return user.SendMessageW(hwnd,0x8000+19,0,0)/100
                def timing_fps(limit,sync):
                    # WM_APP+19 flushes timings at the counter observation.
                    # Read them before captures or later keyboard actions stall drawing.
                    rows=list(csv.DictReader(timing.open(),delimiter="\t"))
                    sample=[r for r in rows if int(r["fps_limit"])==limit and int(r["vsync"])==sync][-25:]
                    assert len(sample)>5,(limit,sync,len(sample))
                    return 1000*(len(sample)-1)/sum(float(r["interval_ms"]) for r in sample[1:])
                def shot(name):
                    im=capture_from_game(hwnd,capture); im.save(OUT/f"{prefix}-{name}.png"); return np.array(im)
                def counter_pixels(im):
                    scale=height/700
                    return im[round(8*scale):round(38*scale),round(8*scale):round(120*scale)]
                time.sleep(11)
                assert fps()<0,"FPS counter should initially be off"
                off=shot("off")
                key(0x2d); click(588,559)
                shot("switch-on")
                key(0x1b); time.sleep(1.4)
                sixty=fps(); assert 53<=sixty<=66,sixty
                on=shot("on-60")
                change=np.abs(counter_pixels(on).astype(float)-counter_pixels(off).astype(float)).mean()
                assert change>5,("FPS label was not drawn at the top left",change)
                key(0x2d); click(257,448); key(0x1b); time.sleep(1.4)
                thirty=fps(); assert 27<=thirty<=33,thirty
                shot("on-30")
                # Private software capture can stall presentation while drawing
                # the screenshot. Sample again once that interval has aged out.
                time.sleep(1.4)
                thirty=fps(); assert 27<=thirty<=33,thirty
                timed_thirty=timing_fps(30,0)
                key(0x2d); click(341,448); key(0x23); key(0x20); key(0x1b)
                assert fps()<0,"End/Space did not switch the counter off"
                hidden=shot("off-again")
                assert np.abs(counter_pixels(hidden).astype(float)-counter_pixels(off).astype(float)).mean()<2
                key(0x2d); key(0x23); key(0x0d); key(0x1b); time.sleep(1.4)
                assert fps()>0,"End/Enter did not switch the counter on"
                key(0x2d); key(0x23); key(0x26); key(0x20); key(0x1b); time.sleep(1.4)
                synced=fps(); assert 10<synced<70,synced
                timed_synced=timing_fps(60,1)
                key(0x2d); key(0x23); key(0x26); key(0x20); key(0x1b)
                if width==1280:
                    user.SetWindowPos(hwnd,0,0,0,1280,1000,0x34); time.sleep(.5)
                    tall=shot("letterbox")
                    assert np.max(tall[:130])==0,"Counter was drawn in the letterbox band"
                    assert np.any(tall[145:181,8:125,1]>180),"Counter missing from the game's top left"
                user.PostMessageW(hwnd,0x10,0,0); assert process.wait(timeout=8)==0
                samples={"30":timed_thirty,"vsync":timed_synced}
                assert abs(timed_thirty-thirty)<5,("30",timed_thirty,thirty)
                assert abs(timed_synced-synced)<5,("vsync",timed_synced,synced)
                report=dict(resolution=[width,height],defaultOff=True,mouseSwitch=True,keyboardSwitch=True,
                    fps60=sixty,fps30=thirty,fpsVsync=synced,timingFps=samples,topLeftPixelDifference=float(change))
                (OUT/f"{prefix}-checks.json").write_text(json.dumps(report,indent=2)+"\n")
                print(report,flush=True)
            finally:
                if process.poll() is None: process.terminate()

if __name__=="__main__":
    OUT.mkdir(exist_ok=True,parents=True)
    run(1280,720); run(1920,1080)
