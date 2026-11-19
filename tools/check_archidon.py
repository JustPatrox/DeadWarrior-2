"""Enter mission two through Continue and verify its native live battle."""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
import json
from pathlib import Path
import subprocess
import tempfile
import time

import numpy as np
from native_client import capture_from_game
from reference_projector import user,window

ROOT=Path(__file__).resolve().parents[1]


def run(width,height,shadow=False,magic=False,rebels=False):
    out=ROOT/"evidence"/("windows-rebels" if rebels else "windows-magic" if magic else "windows-shadow" if shadow else "windows-archidon")
    out.mkdir(exist_ok=True,parents=True)
    prefix=f"{width}x{height}"
    trace=out/f"{prefix}-steps.tsv"
    timing=out/f"{prefix}-frames.tsv"
    capture=out/f"{prefix}-capture.png"
    trace.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory() as save_dir:
        save=Path(save_dir)/"campaign.sav"
        level=4 if rebels else 3 if magic else 2 if shadow else 1
        save.write_text(f"DeadWarrior2-Campaign 1\n{level} {level} 1 "+format(2,"021b")+"\n"+
                        "220 220 1\n"*level+"-1 0 0\n"*(14-level))
        with subprocess.Popen([ROOT/"out/windows-x64/DeadWarrior 2.exe","--save-dir",save_dir,
                              "--trace-tutorial",trace,"--frame-timing",timing,"--capture-client",capture,"--magenta"]) as proc:
            try:
                until=time.monotonic()+12
                while time.monotonic()<until:
                    hwnd=next((h for h,c in window(proc.pid) if c=="DeadWarrior2Native"),None)
                    if hwnd: break
                    time.sleep(.1)
                else: raise RuntimeError("Native window missing")
                style=user.GetWindowLongW(hwnd,-16)
                user.SetWindowLongW(hwnd,-16,(style&~0x00cf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                def send(code,x,y,b=0):
                    px=round(width/2+(x-425)*height/700); py=round(y*height/700)
                    user.SendMessageW(hwnd,code,b,(py<<16)|(px&65535))
                def click(x,y,right=False):
                    send(0x200,x,y); send(0x204 if right else 0x201,x,y,2 if right else 1)
                    send(0x205 if right else 0x202,x,y)
                def wait(test,limit=12):
                    until=time.monotonic()+limit
                    while time.monotonic()<until:
                        if proc.poll() is not None: raise RuntimeError("Native process exited")
                        if trace.exists():
                            rows=[r.split("\t") for r in trace.read_text().splitlines()]
                            if rows and test(rows[-1]): return rows[-1]
                        time.sleep(.1)
                    raise AssertionError(f"Native state did not match: {rows[-1] if rows else 'none'}")
                def shot(name):
                    im=capture_from_game(hwnd,capture)
                    im.save(out/f"{prefix}-{name}.png")
                    assert not np.any(np.all(np.asarray(im)==[255,0,255],axis=2)),name
                time.sleep(11)
                click(161,664); time.sleep(1.2); click(578,664)
                if shadow or magic or rebels:
                    wait(lambda r:r[0]=="101" and int(r[1])==(238 if rebels else 151 if magic else 92))
                    click(365,320) if rebels else click(380,350) if magic else click(440,335)
                    entry=wait(lambda r:r[0]=="27")
                    assert float(entry[10])==(1500 if rebels else 700 if magic else 1200) and float(entry[11])==(1500 if rebels else 1200),entry
                    shot("entry")
                    if rebels:
                        im=np.asarray(capture_from_game(hwnd,capture))
                        soil=im[round(height*.9):,round(width*.85):].mean(axis=(0,1))
                        assert soil[2]<soil[1]*.8,("Sky visible below grass terrain",soil)
                    if not rebels: click(117,665) if magic else click(146,610)
                    click(554,582)
                    wait(lambda r:r[16]=="1")
                    if not rebels: wait(lambda r:int(r[1])>=2400,70)
                    shot("army")
                    for _ in range(50):
                        click(56,665); click(88,610); click(500,634)
                        if rebels: click(146,610)
                        if magic:
                            send(0x201,-190,240,1); send(0x200,1040,570,1); send(0x202,1040,570)
                            user.SendMessageW(hwnd,0x100,ord("Q"),0)
                        time.sleep(3)
                        row=trace.read_text().splitlines()[-1].split("\t")
                        if row[0] in ("28","29"): break
                    assert row[0]=="28",row
                    wait(lambda r:r[15]=="1")
                    if magic:
                        shot("magikill-unlocked")
                        click(425,490); time.sleep(1.2); shot("meric-unlocked")
                    click(425,490); time.sleep(1.2); shot("victory")
                    click(580,653); wait(lambda r:r[0]=="100")
                    click(750,664); wait(lambda r:r[0]=="100" and r[4]=="1")
                    destination=298 if rebels else 238 if magic else 151
                    click(353,381); wait(lambda r:r[0]=="101" and int(r[1])==destination)
                    shot("giants-map" if rebels else "rebels-map" if magic else "magikill-map")
                    click(309,644); wait(lambda r:r[0]=="101" and r[5]=="1")
                    record=save.read_text().splitlines()
                    assert record[1].split()[:2]==[str(level+1)]*2 and int(record[level+2].split()[2])==1
                    report=dict(resolution=[width,height],victory=True,savedLevel=level+1,nextMapFrame=destination,
                                victoryTick=int(row[1]),playerStatue=float(row[10]))
                    (out/f"{prefix}-checks.json").write_text(json.dumps(report,indent=2)+"\n")
                    user.PostMessageW(hwnd,0x10,0,0); assert proc.wait(timeout=8)==0
                    print(report)
                    return
                wait(lambda r:r[0]=="101" and r[1]=="50")
                shot("map")
                click(475,270)
                initial=wait(lambda r:r[0]=="27")
                assert (float(initial[10]),float(initial[11]),int(initial[5]))==(1000,800,700),initial
                shot("entry")
                click(88,610); click(56,665)
                wait(lambda r:int(r[14])==2)
                click(110,595)  # Cancel Swordwrath from a mixed barracks queue.
                wait(lambda r:int(r[14])==1 and int(r[5])==250)
                click(88,610)
                trained=wait(lambda r:int(r[17])==1 and int(r[14])==0,65)
                shot("trained")
                click(500,634); click(425,600)
                fire=wait(lambda r:int(r[19])>0 and int(r[9])>0,85)
                click(445,600); time.sleep(1)
                shot("archer-battle")
                report=dict(resolution=[width,height],entryHealth=[1000,800],mixedQueueCancelled=True,
                            speartons=int(trained[17]),enemyArchidons=int(fire[18]),arrows=int(fire[19]),holes=0)
                deadline=time.monotonic()+150
                while time.monotonic()<deadline:
                    row=trace.read_text().splitlines()[-1].split("\t")
                    if row[0] in ("28","29"): break
                    if int(row[5])>=150: click(88,610)
                    click(500,634)
                    time.sleep(8)
                won=wait(lambda r:r[0]=="28",8)
                wait(lambda r:r[15]=="1")
                click(425,490); time.sleep(1.2); shot("victory")
                assert float(won[10])>0 and float(won[11])==0,won
                click(580,653); wait(lambda r:r[0]=="100")
                click(750,664); wait(lambda r:r[0]=="100" and r[4]=="1")
                click(353,381)
                wait(lambda r:r[0]=="101" and r[1]=="92")
                shot("shadowrath-map")
                click(309,644)
                wait(lambda r:r[0]=="101" and r[5]=="1")
                record=save.read_text().splitlines()
                assert record[1].split()[:2]==["2","2"] and int(record[3].split()[2])==1,record[:4]
                report.update(victory=True,nextMapFrame=92,savedLevel=2)
                click(435,390)
                wait(lambda r:r[0]=="101" and r[5]=="0")
                click(440,335)
                third=wait(lambda r:r[0]=="27")
                assert float(third[10])==float(third[11])==1200,third
                shot("shadowrath-entry")
                report["thirdMission"]="Silent Assassins"
                user.PostMessageW(hwnd,0x10,0,0); assert proc.wait(timeout=8)==0
                (out/f"{prefix}-checks.json").write_text(json.dumps(report,indent=2)+"\n")
                print(report)
            finally:
                if proc.poll() is None: proc.terminate()


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("width",type=int); parser.add_argument("height",type=int)
    parser.add_argument("--shadow",action="store_true")
    parser.add_argument("--magic",action="store_true")
    parser.add_argument("--rebels",action="store_true")
    args=parser.parse_args()
    run(args.width,args.height,args.shadow,args.magic,args.rebels)
