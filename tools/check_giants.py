"""Win32 campaign-six battle, seven voiced states, unlock, map and save round trip."""
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
from reference_projector import user,window

ROOT=Path(__file__).resolve().parents[1]

def run(width,height):
    out=ROOT/"evidence/windows-giants"
    out.mkdir(exist_ok=True,parents=True)
    prefix=f"{width}x{height}"
    trace=out/f"{prefix}-steps.tsv"
    capture=out/f"{prefix}-capture.png"
    trace.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory() as save_dir:
        save=Path(save_dir)/"campaign.sav"
        save.write_text("DeadWarrior2-Campaign 1\n5 5 1 000000000000000000010\n"+"220 220 1\n"*5+"-1 0 0\n"*9)
        args=[Path(os.environ.get("DW2_TEST_EXE",ROOT/"out/windows-x64/DeadWarrior 2.exe")),"--save-dir",save_dir,"--trace-tutorial",trace,
              "--capture-client",capture,"--frame-timing",out/f"{prefix}-frames.tsv","--magenta"]
        with subprocess.Popen(args) as process:
            try:
                deadline=time.monotonic()+15
                while time.monotonic()<deadline:
                    hwnd=next((h for h,c in window(process.pid) if c=="DeadWarrior2Native"),None)
                    if hwnd: break
                    time.sleep(.1)
                else: raise RuntimeError("Native window missing")
                style=user.GetWindowLongW(hwnd,-16)
                user.SetWindowLongW(hwnd,-16,(style&~0x00cf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                def send(code,x,y,button=0):
                    px=round(width/2+(x-425)*height/700); py=round(y*height/700)
                    user.SendMessageW(hwnd,code,button,(py<<16)|(px&65535))
                def click(x,y):
                    send(0x200,x,y); send(0x201,x,y,1); send(0x202,x,y)
                def row():
                    rows=trace.read_text().splitlines() if trace.exists() else []
                    return rows[-1].split("\t") if rows else []
                def wait(condition,limit=15):
                    until=time.monotonic()+limit
                    while time.monotonic()<until:
                        if process.poll() is not None: raise AssertionError(f"Native process exited ({process.returncode})")
                        current=row()
                        if current and condition(current): return current
                        time.sleep(.1)
                    raise AssertionError(f"Native state did not match: {row()}")
                def shot(name):
                    image=capture_from_game(hwnd,capture)
                    image.save(out/f"{prefix}-{name}.png")
                    assert not np.any(np.all(np.asarray(image)==[255,0,255],axis=2)),name
                time.sleep(11)
                click(161,664); time.sleep(1.2); click(578,664)
                wait(lambda r:r[0]=="101" and r[1]=="298")
                shot("map-entry"); click(390,270)
                initial=wait(lambda r:r[0]=="27")
                assert initial[30]=="1" and float(initial[11])==180,initial
                shot("entry")
                click(310,600); time.sleep(3)
                current=row(); click(550-float(current[2]),300); click(675,606)
                wait(lambda r:int(r[7])>=0)
                shot("castle-left")
                click(554,582)
                seen=set(); attacked=False; next_order=0; start=-1
                until=time.monotonic()+160
                while time.monotonic()<until:
                    current=row(); tick=int(current[1]); step=int(current[0])
                    if step in (28,29): break
                    if step<200 and tick>=next_order:
                        click(146,610); click(88,610); next_order=tick+300
                        if tick>1650: click(500,634)
                    if step>=200:
                        if start<0: start=tick; shot("fade-out")
                        dialogue=int(current[33])
                        assert current[16]=="0",current
                        if dialogue>=0 and dialogue not in seen:
                            seen.add(dialogue); shot(f"dialogue-{dialogue+1}")
                            assert float(current[11])==180,current
                            before=int(current[5]); click(88,610); click(500,634)
                            assert int(row()[5])==before,"Cinematic accepted training input"
                    elif int(current[31])>0 and not attacked:
                        attacked=True; shot("giant-strike")
                    time.sleep(.12)
                assert current[0]=="28",current
                assert seen==set(range(7)) and start>0,seen
                wait(lambda r:r[15]=="1"); time.sleep(.7); shot("giant-unlocked")
                click(425,490); time.sleep(1.2); shot("victory")
                click(580,653); wait(lambda r:r[0]=="100")
                click(750,664); wait(lambda r:r[0]=="100" and r[4]=="1")
                click(353,381); wait(lambda r:r[0]=="101" and r[1]=="341")
                shot("explosive-war-map")
                click(309,644); wait(lambda r:r[0]=="101" and r[5]=="1")
                record=save.read_text().splitlines()
                assert record[1].split()[:2]==["6","6"] and int(record[7].split()[2])==1,record[:8]
                report=dict(resolution=[width,height],victory=True,cinematicStart=start,victoryTick=int(current[1]),
                    dialogueStates=len(seen),giantHits=int(current[31]),castleShots=int(current[26]),savedLevel=6,nextMapFrame=341)
                (out/f"{prefix}-checks.json").write_text(json.dumps(report,indent=2)+"\n")
                user.PostMessageW(hwnd,0x10,0,0); assert process.wait(timeout=8)==0
                print(report,flush=True)
            finally:
                if process.poll() is None: process.terminate()
                log=args[0].parent/"lastest.log"
                if log.exists(): (out/f"{prefix}-lastest.log").write_bytes(log.read_bytes())

if __name__=="__main__":
    parser=argparse.ArgumentParser(); parser.add_argument("width",type=int); parser.add_argument("height",type=int)
    args=parser.parse_args(); run(args.width,args.height)
