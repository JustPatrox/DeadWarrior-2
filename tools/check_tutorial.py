"""Exercise the campaign entry, box selection, move command and first enemy in the Win32 release."""
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

from reference_projector import window

user=ct.windll.user32
root=Path(__file__).resolve().parents[1]


def run(width,height,economy=False,defence=False,battle=False,hard_loss=False,intermission=False,keep_point=False,exe=None):
    battle=battle or intermission
    defence=defence or battle
    out=root/"evidence/windows-tutorial"
    out.mkdir(parents=True,exist_ok=True)
    prefix=f"{width}x{height}"+("-hard" if hard_loss else "")
    trace=out/f"{prefix}-steps.tsv"
    capture=out/f"{prefix}-current.png"
    scale=height/700
    def coord(x,y):
        return round(width/2+(x-425)*scale),round(y*scale)
    with tempfile.TemporaryDirectory() as save_dir, subprocess.Popen([
            exe or root/"out/windows-x64/DeadWarrior 2.exe","--magenta","--trace-tutorial",trace,
            "--capture-client",capture,"--save-dir",save_dir]) as proc:
        try:
            deadline=time.monotonic()+12
            while time.monotonic()<deadline:
                hwnd=next((h for h,c in window(proc.pid) if c=="DeadWarrior2Native"),None)
                if hwnd: break
                time.sleep(.1)
            else: raise RuntimeError("Native window missing")
            style=user.GetWindowLongW(hwnd,-16)
            user.SetWindowLongW(hwnd,-16,(style & ~0x00CF0000)|0x80000000)
            user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
            def event(message,x,y,buttons=0):
                px,py=coord(x,y)
                user.SendMessageW(hwnd,message,buttons,(py<<16)|(px&0xffff))
            def click(x,y,right=False):
                event(0x200,x,y)
                event(0x204 if right else 0x201,x,y,2 if right else 1)
                event(0x205 if right else 0x202,x,y)
            def wait_step(step,limit=10,hover=None):
                until=time.monotonic()+limit
                while time.monotonic()<until:
                    if hover:
                        event(0x200,*hover)
                        user.SendMessageW(hwnd,0x113,1,0)
                    if trace.exists() and any(l.split("\t")[0]==str(step) for l in trace.read_text().splitlines()):
                        return
                    time.sleep(.015 if hover else .045)
                raise AssertionError(f"Tutorial never reached state {step}. Trace: {trace.read_text()}")
            def shot(label):
                im=capture_from_game(hwnd,capture)
                assert im.size==(width,height),(im.size,width,height)
                im.save(out/f"{prefix}-{label}.png")
                a=np.asarray(im)
                holes=np.all(a==[255,0,255],axis=2).sum()
                assert not holes,(label,int(holes))
                print(label,flush=True)
                return a
            def bars(a,minimum_green=230):
                # In the playable plane count rows with bright green HealthBar pixels.
                crop=a[round(240*scale):round(500*scale)]
                green=(crop[:,:,1]>minimum_green)&(crop[:,:,0]<24)&(crop[:,:,2]<24)
                ys=np.where(green.sum(axis=1)>5)[0]
                return [[int(g.min()+round(240*scale)),int(g.max()+round(240*scale))] for g in
                        np.split(ys,np.where(np.diff(ys)>1)[0]+1) if g.size]
            time.sleep(12)
            for x,y,wait in [(161,664,1.25),(437,664,1.25),(425 if hard_loss else 154,668,2),(434,639,5.8)]:
                click(x,y); time.sleep(wait)
            def wait_value(index,test,limit):
                until=time.monotonic()+limit
                while time.monotonic()<until:
                    rows=trace.read_text().splitlines()
                    if rows:
                        r=rows[-1].split("\t")
                        if len(r)>index and test(float(r[index])): return r
                    time.sleep(.08)
                raise AssertionError(f"Trace column {index} did not reach the expected value. Last row {r}")
            if hard_loss:
                wait_step(1)
                shot("skip")
                click(442,137); wait_step(27)
                click(350,634)
                wait_value(6,lambda n:n==6,4)
                click(554,582)
                wait_value(16,lambda n:n==1,5)
                wait_step(29,290)
                shot("collapse")
                row=wait_value(15,lambda n:n==1,8); time.sleep(.7)
                shot("defeat-tip")
                click(425,417); time.sleep(1.2); shot("defeat")
                report=dict(resolution=[width,height],fastForward=True,playerStatue=float(row[10]),
                            enemyStatue=float(row[11]),defeat=True)
                assert report["playerStatue"]==0 and report["enemyStatue"]>0
                click(580,653)
                retry=wait_value(0,lambda n:n==1,8)
                assert float(retry[10])==1500 and float(retry[11])==400
                click(442,137)
                wait_value(0,lambda n:n==27,8)
                report["restartedAndSkipped"]=True
                (out/f"{prefix}-checks.json").write_text(json.dumps(report,indent=2))
                print(report)
                return
            initial=shot("select")
            wait_step(1)
            initial_bars=bars(initial)
            assert len(initial_bars)>=2,("Two initial soldiers missing",initial_bars)
            event(0x201,580,260,1); event(0x200,720,510,1); event(0x202,720,510)
            wait_step(2); time.sleep(2.6)
            shot("move")
            click(150,420,True)
            shot("movement-marker")
            wait_step(3); time.sleep(2.6)
            after=shot("scroll")
            sx,_=coord(480,0)
            assert not bars(after[:,sx:]),"Soldiers did not move left"
            wait_step(4,15,(425+700*16/9/2-5,165))
            event(0x200,425,225)
            time.sleep(2.6)
            attack=shot("attack")
            assert bars(attack,95),"No enemy health bar in the fog boundary after scrolling"
            event(0x200,550,430); time.sleep(.1)
            assert user.SendMessageW(hwnd,0x8010,0,0)==2313,"Original attack cursor missing"
            shot("attack-cursor")
            audio_before=user.SendMessageW(hwnd,0x8011,0,0)
            click(550,430,True)
            wait_step(5,30); time.sleep(2.6)
            end=shot("return")
            assert user.SendMessageW(hwnd,0x8011,0,0)>audio_before,"Movement or combat produced no audio"
            # After the kill the camera can navigate to the friendly castle via the minimap.
            click(350,600)
            wait_step(6,15); time.sleep(3)
            home=shot("miner")
            region=np.s_[round(190*scale):round(570*scale),:,:]
            change=float(np.abs(home[region].astype(float)-end[region].astype(float)).mean())
            assert change>7,("Minimap click did not move camera after the fight",change)
            report=dict(resolution=[width,height],initialHealthBars=initial_bars,holes=0,
                        movedLeft=True,enemyInCamera=True,baseCameraDifference=change)
            if economy or defence:
                def camera():
                    return float(trace.read_text().splitlines()[-1].split("\t")[2])
                click(1250-camera(),430)
                wait_step(7,7); time.sleep(2.2)
                event(0x200,1350-camera(),320); time.sleep(.1)
                assert user.SendMessageW(hwnd,0x8010,0,0)==8821,"Original prayer cursor missing"
                shot("prayer-cursor")
                user.SendMessageW(hwnd,0x8,0,0)
                assert user.SendMessageW(hwnd,0x8010,0,0)==0,"Focus loss left a game cursor active"
                event(0x200,1350-camera(),320)
                click(1350-camera(),320,True)
                wait_step(8,12)
                wait_step(9,21); time.sleep(2.2)
                shot("mine-gold")
                event(0x200,1590-camera(),490); time.sleep(.1)
                assert user.SendMessageW(hwnd,0x8010,0,0)==8824,"Original mining cursor missing"
                shot("mining-cursor")
                click(1590-camera(),490,True)
                wait_step(10,8); wait_step(11,12); time.sleep(2.6)
                shot("train")
                click(95,610)
                wait_step(12); wait_step(13,30); time.sleep(4)
                shot("spearton")
                report["prayedMinedTrained"]=True
            if defence:
                wait_step(14,20); time.sleep(3.2)
                shot("garrison")
                click(350,634)
                wait_step(15,3); wait_step(16,35); time.sleep(4.6)
                shot("range")
                click(532-camera(),170)
                wait_step(17,10); time.sleep(3)
                report["rangeCamera"]=float(wait_value(2,lambda x:197<=x<=201,4)[2])
                shot("castle-upgrade")
                click(675,606)
                wait_step(18,14); wait_step(19,20); time.sleep(3.5)
                shot("defend")
                click(425,637)
                wait_step(20,5); time.sleep(2.5)
                shot("defence-battle")
                wait_step(21,40); wait_step(22,18); time.sleep(3)
                shot("objective")
                rows=[line.split("\t") for line in trace.read_text().splitlines()]
                assert any(r[0]=="15" and int(r[6])==4 for r in rows),"Recall missed a unit"
                assert any(r[0]=="18" and 0<=int(r[7])<300 for r in rows),"Research did not run"
                shots,hits=map(int,rows[-1][8:10])
                assert hits>0 and shots>=hits,"No direct castle archer hit observed"
                statue_health=float(rows[-1][10])
                assert 0<statue_health<1500 and (1500-statue_health)%17==0,("First Spearton attack",statue_health)
                report.update(garrisoned=4,castleArcherShots=shots,castleArcherHits=hits,
                              invadingSpeartonDefeated=True,playerStatueAfterDefence=statue_health)
            if battle:
                click(26,610); click(88,610); click(88,610)
                wait_value(14,lambda n:n==2,4)
                shot("recruit")
                click(110,595); wait_value(14,lambda n:n==1,4)
                # Use the queue's training time to inspect both ends of the map.
                click(306,600); left=float(wait_value(2,lambda x:197<=x<=201,5)[2]); shot("camera-left")
                click(535,600); right=float(wait_value(2,lambda x:4449<=x<=4453,5)[2]); shot("camera-right")
                report["mapCameraLimits"]=[left,right]
                wait_value(14,lambda n:n==0,18)
                click(88,610); click(500,634)
                click(475,600)
                wait_value(12,lambda n:n==1,90)
                shot("reinforcement")
                wait_step(28,90); time.sleep(.7)
                shot("collapse")
                row=wait_value(15,lambda n:n==1,8); time.sleep(1)
                shot("spearton-unlocked")
                click(425,490); time.sleep(1.5)
                shot("victory")
                report.update(playerStatue=float(row[10]),enemyStatue=float(row[11]),victory=True,
                              speartonReinforcement=True,cancelledRecruit=True)
                assert report["playerStatue"]>0 and report["enemyStatue"]==0
            if intermission:
                click(580,653); wait_value(0,lambda n:n==100,8)
                event(0x200,425,493); time.sleep(.3); shot("upgrades")
                click(580,403)  # Block needs Rage.
                assert int(trace.read_text().splitlines()[-1].split("\t")[2])==1
                event(0x200,425,493); time.sleep(.12)
                click(750,664); wait_value(4,lambda n:n==1,3); shot("confirm-upgrades")
                click(510,381); wait_value(4,lambda n:n==0,3)
                if keep_point:
                    click(750,664); wait_value(4,lambda n:n==1,3); click(353,381)
                else:
                    click(425,493); wait_value(2,lambda n:n==0,3)
                    assert int(trace.read_text().splitlines()[-1].split("\t")[3])==6
                    click(425,493); shot("rage")
                    click(750,664)
                wait_value(0,lambda n:n==101,6)
                shot("map-travel")
                wait_value(1,lambda n:n==50,8); shot("archidon-map")
                save=Path(save_dir)/"campaign.sav"
                assert not save.exists(),"Fresh campaign unexpectedly enabled auto-save"
                click(309,644); wait_value(5,lambda n:n==1,6); shot("game-saved")
                lines=save.read_text().splitlines()
                assert lines[0]=="DeadWarrior2-Campaign 1"
                level,points,difficulty,tech=lines[1].split()
                expected_points=int(keep_point)
                expected_tech=2 if keep_point else 6
                assert (int(level),int(points),int(difficulty),int(tech,2))==(1,expected_points,1,expected_tech)
                best,total,attempts=map(int,lines[2].split())
                assert best==total and best>0 and attempts==1 and len(lines)==16
                click(435,390); wait_value(5,lambda n:n==0,3)
                user.PostMessageW(hwnd,0x10,0,0); proc.wait(timeout=8)
                resume_trace=out/f"{prefix}-resumed.tsv"
                with subprocess.Popen([exe or root/"out/windows-x64/DeadWarrior 2.exe","--save-dir",save_dir,
                                       "--magenta","--trace-tutorial",resume_trace,"--capture-client",capture]) as resumed:
                    try:
                        until=time.monotonic()+10
                        while time.monotonic()<until:
                            hwnd=next((h for h,c in window(resumed.pid) if c=="DeadWarrior2Native"),None)
                            if hwnd: break
                            time.sleep(.15)
                        assert hwnd
                        style=user.GetWindowLongW(hwnd,-16)
                        user.SetWindowLongW(hwnd,-16,(style & ~0x00CF0000)|0x80000000)
                        user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                        time.sleep(11)
                        click(161,664); time.sleep(1.3); shot("resume-menu")
                        trace=resume_trace; click(578,664)
                        wait_value(0,lambda n:n==101,7)
                        row=wait_value(1,lambda n:n==50,6); shot("resumed-map")
                        assert list(map(int,row[2:7]))==[expected_points,expected_tech,0,0,1]
                    finally:
                        resumed.terminate()
                report.update(mapFrame=50,savedAndResumed=True,upgradePoints=expected_points,
                              rage=not keep_point,bestTime=best)
            (out/f"{width}x{height}-checks.json").write_text(json.dumps(report,indent=2))
            print(report)
        finally:
            proc.terminate()


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("width",type=int,default=1280,nargs="?")
    parser.add_argument("height",type=int,default=720,nargs="?")
    parser.add_argument("--economy",action="store_true")
    parser.add_argument("--defence",action="store_true")
    parser.add_argument("--battle",action="store_true")
    parser.add_argument("--hard-loss",action="store_true")
    parser.add_argument("--intermission",action="store_true")
    parser.add_argument("--keep-point",action="store_true")
    parser.add_argument("--exe",type=Path)
    args=parser.parse_args()
    run(args.width,args.height,args.economy,args.defence,args.battle,args.hard_loss,args.intermission,args.keep_point,args.exe)
