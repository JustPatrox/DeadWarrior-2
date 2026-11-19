"""Exercise resolution, themed PC settings, and native campaign pause controls."""
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
EXE=Path(os.environ.get("DW2_TEST_EXE",ROOT/"out/windows-x64/DeadWarrior 2.exe"))
OUT=ROOT/"evidence/windows-pause-settings"


def run():
    OUT.mkdir(parents=True,exist_ok=True)
    capture=OUT/"capture.png"; trace=OUT/"steps.tsv"
    with tempfile.TemporaryDirectory() as temporary:
        save=Path(temporary)/"campaign.sav"
        save.write_text("DeadWarrior2-Campaign 1\n1 1 1 000000000000000000010\n220 220 1\n"+"-1 0 0\n"*13)
        original_save=save.read_bytes()
        with subprocess.Popen([EXE,"--save-dir",temporary,"--capture-client",capture,"--trace-tutorial",trace]) as process:
            try:
                deadline=time.monotonic()+15
                while time.monotonic()<deadline:
                    hwnd=next((h for h,c in window(process.pid) if c=="DeadWarrior2Native"),None)
                    if hwnd:break
                    time.sleep(.1)
                else:raise AssertionError("Native window missing")
                def size():
                    r=wt.RECT();user.GetClientRect(hwnd,ct.byref(r));return r.right,r.bottom
                def key(k):user.SendMessageW(hwnd,0x100,k,0)
                def click(x,y):
                    w,h=size();s=min(w/(700*16/9),h/700)
                    px=round(w/2+(x-425)*s);py=round((h-700*s)/2+y*s)
                    for code,b in ((0x200,0),(0x201,1),(0x202,0)):user.SendMessageW(hwnd,code,b,(py<<16)|(px&65535))
                def query(n):return user.SendMessageW(hwnd,0x8000+n,0,0)
                def target():
                    value=query(20);return value>>16,value&65535
                def wait(test,limit=12):
                    until=time.monotonic()+limit
                    while time.monotonic()<until:
                        if process.poll() is not None:raise AssertionError("Native process exited")
                        if test():return
                        time.sleep(.05)
                    raise AssertionError(f"State timed out: client={size()}, buffer={target()}, pause={query(21)}, ticks={query(22)}")
                def shot(name):
                    image=capture_from_game(hwnd,capture);assert image.size==size()
                    image.save(OUT/(name+".png"));return np.array(image)
                time.sleep(11)
                desktop=(user.GetSystemMetrics(0),user.GetSystemMetrics(1))
                key(0x2d);click(615,200)
                wait(lambda:size()==(1280,720) and target()==(1280,720));shot("settings-720")
                click(615,200);wait(lambda:size()==(1600,900) and target()==(1600,900))
                click(615,200);wait(lambda:target()==(1920,1080))
                w,h=size();assert w*9==h*16 and w<=desktop[0] and h<desktop[1]
                key(0x7a);wait(lambda:size()==desktop)
                click(382,200);wait(lambda:target()==(1600,900))
                click(382,200);wait(lambda:target()==(1280,720));shot("fullscreen-720")
                click(425,300);time.sleep(.2)
                assert target()==(1280,720) and size()!=desktop
                key(0x7a);wait(lambda:size()==desktop)
                key(0x24);key(0x26)
                for _ in range(4):key(0x27)
                key(0x0d);wait(lambda:target()==(3840,2160));shot("fullscreen-2160")
                assert (user.GetSystemMetrics(0),user.GetSystemMetrics(1))==desktop
                key(0x27);key(0x0d);wait(lambda:target()==size())  # Auto.
                key(0x7a)
                key(0x24);key(0x26)
                key(0x27);key(0x0d);wait(lambda:target()==(1280,720))
                user.SetWindowPos(hwnd,0,0,0,1280,1000,0x16);time.sleep(.4)
                w,h=size();scale=720/min(h,w*9/16)
                expected=(round(w*scale),round(h*scale));wait(lambda:target()==expected)
                image=shot("letterbox-720");assert image[:30].max()==0
                # Return to Auto and a normal physical client for the battle.
                key(0x25);key(0x0d);key(0x1b)
                r=wt.RECT(0,0,1280,720)
                user.AdjustWindowRectExForDpi(ct.byref(r),user.GetWindowLongW(hwnd,-16),False,
                                             user.GetWindowLongW(hwnd,-20),user.GetDpiForWindow(hwnd))
                user.SetWindowPos(hwnd,0,0,0,r.right-r.left,r.bottom-r.top,0x16)
                click(161,664);time.sleep(1.2);click(578,664)
                def map_ready():
                    rows=trace.read_text().splitlines() if trace.exists() else []
                    return bool(rows) and rows[-1].startswith("101\t50\t")
                wait(map_ready)
                click(475,270);wait(lambda:query(22)>=0);time.sleep(.5)
                click(507,685);wait(lambda:query(21)&1)
                paused_tick=query(22);time.sleep(1.2);assert query(22)==paused_tick
                shot("pause-original");assert not query(18)&2
                click(120,320);time.sleep(.2);assert query(22)==paused_tick
                click(429,386);wait(lambda:not query(21)&1)
                start=query(22);time.sleep(.5);resumed=query(22)-start
                assert 8<=resumed<=25,("Pause time was replayed on resume",resumed)
                key(ord('P'));wait(lambda:query(21)&1)
                click(348,548);assert query(21)&12==8
                click(502,548);assert query(21)&12==0
                click(348,548);assert query(21)&12==4
                click(502,548);assert query(21)&12==12
                click(429,416);shot("pause-help");click(429,510);wait(lambda:not query(21)&1)
                key(0x1b);wait(lambda:query(21)&1)
                key(0x2d);assert query(21)&3==3
                frozen=query(22);time.sleep(.7);assert query(22)==frozen;shot("settings-over-pause")
                key(0x1b);assert query(21)&3==1
                key(0x1b);wait(lambda:not query(21)&1)
                key(0x7a);key(0x1b);assert size()==desktop and query(21)&1
                key(0x1b);assert size()==desktop and not query(21)&1
                time.sleep(.6);key(ord('P'));before=query(22)
                click(429,446);shot("restart-confirmation");click(467,492)
                assert query(21)&1 and query(22)==before
                click(429,446);click(383,492);wait(lambda:not query(21)&1 and 0<=query(22)<before)
                key(ord('P'));click(429,477);click(383,492)
                wait(lambda:query(22)==-1 and not query(21)&1)
                assert save.read_bytes()==original_save
                shot("returned-to-menu")
                log=(EXE.parent/"lastest.log").read_text()
                assert "FATAL" not in log and "AUDIO ERROR" not in log and "AUDIO MISSING" not in log
                (OUT/"lastest.log").write_text(log)
                result=dict(resolutions=[[1280,720],[1600,900],[1920,1080],[3840,2160]],
                    auto=True,windowed=True,borderless=True,fullscreen=True,desktopUnchanged=True,
                    letterbox=True,pauseHud=True,pauseKeyboard=True,pauseStopsTicks=True,
                    resumedTicksInHalfSecond=resumed,independentSoundMusic=True,localHelp=True,
                    insertNesting=True,restartConfirmation=True,quitToMenu=True,savesUnchanged=True)
                (OUT/"checks.json").write_text(json.dumps(result,indent=2)+"\n");print(result,flush=True)
            finally:
                if process.poll() is None:process.terminate()


if __name__=="__main__":run()
