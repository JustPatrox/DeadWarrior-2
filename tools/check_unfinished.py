"""Exercise the native WIP screen, restart choices, save safety and pending-button banner."""
import ctypes as ct
from ctypes import wintypes as wt
import json
import re
import os
from pathlib import Path
import subprocess
import tempfile
import time
import numpy as np
from native_client import capture_from_game
from reference_projector import user,window

ROOT=Path(__file__).resolve().parents[1]
PLAYABLE=int(re.search(r"playableLevelCount\s*=\s*(\d+)",(ROOT/"src/campaign_progress.hpp").read_text(encoding="utf-8")).group(1))
OUT=ROOT/"evidence/windows-unfinished"
OUT.mkdir(parents=True,exist_ok=True)
EXE=Path(os.environ.get("DW2_TEST_EXE",ROOT/"out/windows-x64/DeadWarrior 2.exe"))
LOG=EXE.parent/"lastest.log"

def fixture(folder,level):
    save=folder/"campaign.sav"
    save.write_text(f"DeadWarrior2-Campaign 1\n{level} {level} 1 000000000000000000010\n"+
        "220 220 1\n"*level+"-1 0 0\n"*(14-level))
    return save

def run(width,height,checks):
    with tempfile.TemporaryDirectory() as temporary:
        folder=Path(temporary); save=fixture(folder,PLAYABLE)
        sentinel=folder/"unrelated.txt"; sentinel.write_text("preserve")
        capture=OUT/f"{width}x{height}-capture.png"; trace=OUT/f"{width}x{height}-trace.tsv"
        LOG.write_text("previous-session-marker\n")
        with subprocess.Popen([EXE,"--save-dir",folder,
            "--capture-client",capture,"--trace-tutorial",trace]) as process:
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
                def send(code,x,y,button=0):
                    px=round(width/2+(x-425)*height/700); py=round(y*height/700)
                    user.SendMessageW(hwnd,code,button,(py<<16)|(px&65535))
                def click(x,y):
                    send(0x200,x,y); send(0x201,x,y,1); send(0x202,x,y)
                def state(): return user.SendMessageW(hwnd,0x8000+18,0,0)
                def wait(test,limit=15):
                    until=time.monotonic()+limit
                    while time.monotonic()<until:
                        if test(): return
                        if process.poll() is not None: raise AssertionError("Native process exited")
                        time.sleep(.1)
                    raise AssertionError("Native condition did not match")
                def shot(name):
                    image=capture_from_game(hwnd,capture); image.save(OUT/f"{width}x{height}-{name}.png")
                    assert not np.any(np.all(np.asarray(image)==[255,0,255],axis=2)),name
                def enter():
                    time.sleep(11); click(161,664); time.sleep(1.2); click(578,664)
                    wait(lambda:state()&1)
                enter(); shot("unfinished")
                assert "previous-session-marker" not in LOG.read_text()
                assert f"work-in-progress unavailable_level={PLAYABLE+1}" in LOG.read_text()
                if not checks:
                    click(425,550); assert process.wait(timeout=8)==0
                    (OUT/f"{width}x{height}-lastest.log").write_bytes(LOG.read_bytes())
                    return
                # Releasing outside a button must not delete a save.
                send(0x201,175,465,1); send(0x200,425,390,1); send(0x202,425,390)
                assert save.exists() and state()&1
                user.SendMessageW(hwnd,0x100,0x2d,0)
                click(175,465); assert save.exists() and state()&1
                user.SendMessageW(hwnd,0x100,0x1b,0)
                click(675,465); wait(lambda:not state()&1)
                assert save.exists() and save.read_text().splitlines()[1].split()[:2]==[str(PLAYABLE)]*2
                assert "RESTART completed saves=kept" in LOG.read_text()
                enter(); click(175,465); wait(lambda:not state()&1)
                assert not save.exists() and sentinel.read_text()=="preserve"
                assert "RESTART completed saves=deleted" in LOG.read_text()
                user.PostMessageW(hwnd,0x10,0,0); assert process.wait(timeout=8)==0
                (OUT/f"{width}x{height}-lastest.log").write_bytes(LOG.read_bytes())
            finally:
                if process.poll() is None: process.terminate()

def banner_check(width=1280,height=720):
    with tempfile.TemporaryDirectory() as temporary:
        folder=Path(temporary); fixture(folder,1)
        trace=OUT/f"banner-{width}-steps.tsv"; trace.unlink(missing_ok=True); capture=OUT/f"banner-{width}-capture.png"
        with subprocess.Popen([EXE,"--save-dir",folder,
            "--capture-client",capture,"--trace-tutorial",trace]) as process:
            try:
                time.sleep(11)
                hwnd=next(h for h,c in window(process.pid) if c=="DeadWarrior2Native")
                style=user.GetWindowLongW(hwnd,-16)
                user.SetWindowLongW(hwnd,-16,(style&~0x00cf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                r=wt.RECT(); user.GetClientRect(hwnd,ct.byref(r))
                def click(x,y):
                    px=round(r.right/2+(x-425)*r.bottom/700); py=round(y*r.bottom/700)
                    user.SendMessageW(hwnd,0x201,1,(py<<16)|(px&65535)); user.SendMessageW(hwnd,0x202,0,(py<<16)|(px&65535))
                click(161,664); time.sleep(1.2); click(578,664)
                until=time.monotonic()+12
                while time.monotonic()<until:
                    rows=trace.read_text().splitlines() if trace.exists() else []
                    if rows and rows[-1].startswith("101\t50\t"): break
                    time.sleep(.1)
                else: raise AssertionError("Map did not arrive")
                click(475,270)
                until=time.monotonic()+12
                while time.monotonic()<until:
                    if trace.exists() and trace.read_text().splitlines()[-1].startswith("27\t"): break
                    time.sleep(.1)
                else: raise AssertionError("Battle did not open")
                click(310,600);time.sleep(2.6)
                camera=float(trace.read_text().splitlines()[-1].split('\t')[2])
                click(550-camera,100);click(675,650)
                assert user.SendMessageW(hwnd,0x8000+18,0,0)&2,"Pending button did not show banner"
                assert "UNAVAILABLE button=HUD.CastleArcher additional research" in LOG.read_text()
                capture_from_game(hwnd,capture).save(OUT/"pending-button.png")
                time.sleep(6.2); assert not user.SendMessageW(hwnd,0x8000+18,0,0)&2
                limits=[]
                length=float(next(line.split()[1] for line in (ROOT/"assets/archidon/rules.tsv").read_text().splitlines()
                    if line.startswith("map.length")))
                for name,x,lower,upper in (("left",306,197.222,201),("right",535,length-1047.222-3,length-1047.222+.001)):
                    click(x,600); until=time.monotonic()+8
                    while time.monotonic()<until:
                        rows=trace.read_text().splitlines(); camera=float(rows[-1].split("\t")[2])
                        if lower<=camera<=upper: break
                        time.sleep(.1)
                    else: raise AssertionError(("Camera went beyond castle",name,camera,lower,upper))
                    limits.append(camera)
                    capture_from_game(hwnd,capture).save(OUT/f"{width}x{height}-castle-{name}.png")
                assert "AUDIO MISSING" not in LOG.read_text() and "AUDIO ERROR" not in LOG.read_text()
                (OUT/f"{width}x{height}-camera.json").write_text(json.dumps(dict(limits=limits,terrainLength=length,viewWidth=700*16/9),indent=2)+"\n")
                user.PostMessageW(hwnd,0x10,0,0); assert process.wait(timeout=8)==0
                (OUT/"banner-lastest.log").write_bytes(LOG.read_bytes())
            finally:
                if process.poll() is None: process.terminate()

if __name__=="__main__":
    run(1280,720,True); run(1920,1080,False); banner_check()
    report=dict(resolutions=[[1280,720],[1920,1080]],keepSave=True,deleteOnlyCampaign=True,
        overlayBlocksButtons=True,cancelledClick=True,quit=True,singleLogTruncated=True,pendingButtonBanner=True,castleCameraBounds=True)
    (OUT/"checks.json").write_text(json.dumps(report,indent=2)+"\n"); print(report)
