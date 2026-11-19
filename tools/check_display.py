"""Check Windows display modes and measure completed frames in the live window."""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
from itertools import groupby
import json
from pathlib import Path
import subprocess
import tempfile
import time

import numpy as np
from PIL import Image
from windows_capture import WindowsCapture

from native_client import capture_client
from reference_projector import user, window


ROOT=Path(__file__).resolve().parents[1]
EXE=ROOT/"out/windows-x64/DeadWarrior 2.exe"
OUT=ROOT/"work/windows-display"
FPS_LIMITS=(30,60,120,240,0)


def measure(frames,warmup_ms):
    intervals=np.array([float(row[0]) for row in frames])
    start=int(np.searchsorted(intervals.cumsum(),warmup_ms))
    window_frames=frames[start:]
    assert len(window_frames)>2,"Too few frames after warmup"
    delta,render=np.array([[float(row[0]),float(row[1])] for row in window_frames]).T
    elapsed=delta[1:].sum()
    tick_delta=int(window_frames[-1][5])-int(window_frames[0][5])
    tick_rate=tick_delta*1000/(elapsed+render[0]-render[-1])
    return dict(frames=len(delta),fps=round(float((len(delta)-1)*1000/elapsed),2),
                simTicksPerSecond=round(float(tick_rate),2),renderMedianMs=round(float(np.median(render)),2),
                renderP95Ms=round(float(np.percentile(render,95)),2),
                intervalP95Ms=round(float(np.percentile(delta,95)),2),intervalOver25ms=int((delta>25).sum()))


def live_run(software,check_limits=False,check_vsync=False):
    name="software" if software else "direct2d"
    timing=OUT/f"{name}.tsv"
    trace=OUT/f"{name}-tutorial.tsv"
    args=[EXE,"--magenta","--frame-timing",timing,"--trace-tutorial",trace]
    if software:
        args.append("--software-renderer")
    with tempfile.TemporaryDirectory() as save_dir, subprocess.Popen([*args,"--save-dir",save_dir]) as proc:
        try:
            deadline=time.monotonic()+12
            while time.monotonic()<deadline:
                hwnd=next((h for h,c in window(proc.pid) if c=="DeadWarrior2Native"),None)
                if hwnd: break
                time.sleep(.1)
            else:
                raise RuntimeError("Window missing")

            def client():
                rect=wt.RECT()
                user.GetClientRect(hwnd,ct.byref(rect))
                return rect.right,rect.bottom

            def key(code):
                user.SendMessageW(hwnd,0x100,code,0)
                time.sleep(.2)

            def send(message,x,y,buttons=0):
                w,h=client()
                scale=min(w/(700*16/9),h/700)
                px,py=round(w/2+(x-425)*scale),round((h-700*scale)/2+y*scale)
                user.SendMessageW(hwnd,message,buttons,(py<<16)|px)

            def click(x,y):
                for message,buttons in ((0x200,0),(0x201,1),(0x202,0)):
                    send(message,x,y,buttons)

            def wait_trace(condition):
                deadline=time.monotonic()+(55 if software else 16)
                while time.monotonic()<deadline:
                    if trace.exists() and any(condition(line.split("\t")) for line in trace.read_text().splitlines()):
                        return
                    time.sleep(.25)
                raise AssertionError(f"Unexpected tutorial trace for {name}")

            time.sleep(26 if software else 11)
            if not software:
                original=client()
                key(0x2d)  # Insert
                click(425,299)
                assert user.GetWindowLongW(hwnd,-16)&0x80000000,"Borderless has chrome"
                w,h=client()
                assert w*9==h*16 and w<=user.GetSystemMetrics(0) and h<user.GetSystemMetrics(1),client()
                key(0x28); key(0x0d)  # Down, Enter
                assert client()==(user.GetSystemMetrics(0),user.GetSystemMetrics(1)),client()

                latest=[]
                session=WindowsCapture(cursor_capture=False,draw_border=False,window_hwnd=hwnd)

                @session.event
                def on_frame_arrived(frame,control):
                    latest[:]=[frame.frame_buffer.copy()]

                @session.event
                def on_closed():
                    pass

                capture=session.start_free_threaded()
                try:
                    # Move focus to exercise actual GPU drawing inside the overlay.
                    key(0x26)
                    time.sleep(.6)
                    w,h=client()
                    # On some sessions WGC only returns the earlier GDI+
                    # splash surface. The software comparison checks the same
                    # overlay independently with a private client capture.
                    if latest and tuple(latest[0][round(299*h/700),round(w/2+100*h/700),2::-1])==(57,67,80):
                        pixels=latest[0][:,:,2::-1]
                        assert not np.any(np.all(pixels==[255,0,255],axis=2)), "Unpainted pixels in the live view"
                        Image.fromarray(pixels).save(OUT/"overlay-direct2d.png")
                    else:
                        print("WGC did not return the live Direct2D overlay",flush=True)
                finally:
                    capture.stop()

                key(0x1b)  # Escape closes just the overlay
                assert client()==(user.GetSystemMetrics(0),user.GetSystemMetrics(1))
                key(0x1b)
                assert client()==original and user.GetWindowLongW(hwnd,-16)&0xc00000
                # Check native sizing, with Windows chrome excluded from the ratio.
                style=user.GetWindowLongW(hwnd,-16)
                margins=wt.RECT()
                user.AdjustWindowRectExForDpi(ct.byref(margins),style,False,0,user.GetDpiForWindow(hwnd))
                outer=wt.RECT(40,40,990,870)
                user.SendMessageW(hwnd,0x214,8,ct.addressof(outer))  # WMSZ_BOTTOMRIGHT
                w=outer.right-outer.left-(margins.right-margins.left)
                h=outer.bottom-outer.top-(margins.bottom-margins.top)
                assert w*9==h*16,(w,h)
                key(0x2d)
                click(161,664)  # Underlying campaign button must ignore clicks.
                key(0x1b)
            key(0x7a)  # F11
            assert client()==(1920,1080),client()
            if software:
                key(0x2d); key(0x26)
                image=capture_client(hwnd)
                assert image.getpixel((1114,461))==(57,67,80),"Display overlay missing"
                assert image.getpixel((787,690))==(74,59,41),"Default 60 FPS chip missing"
                image.save(OUT/"overlay-fullscreen.png")
                key(0x1b)
            for x,y,delay in ((161,664,1.35),(437,664,1.35),(154,668,2),(434,639,3)):
                click(x,y)
                time.sleep(delay*2.5 if software else delay)
            # Select the units, order movement and pan through the first battlefield.
            send(0x201,580,260,1); send(0x200,720,510,1); send(0x202,720,510)
            wait_trace(lambda row: int(row[0])==2)
            send(0x204,150,420,2); send(0x205,150,420)
            wait_trace(lambda row: int(row[0])==3)
            send(0x200,1041,165)
            wait_trace(lambda row: float(row[2])>=2800)
            time.sleep(5)
            if check_limits:
                for fps in (30,120,240,0,60):
                    key(0x2d)
                    click(257+84*FPS_LIMITS.index(fps),448)
                    key(0x1b)
                    time.sleep(3)
            if check_vsync:
                for x,y in ((593,448),(588,507),(257,448)):
                    key(0x2d)
                    click(x,y)
                    key(0x1b)
                    time.sleep(3)
                key(0x2d)
                key(0x23); key(0x26); key(0x0d)  # End, Up, Enter toggles V-Sync off.
                key(0x1b)
                time.sleep(3)
            user.PostMessageW(hwnd,0x10,0,0)
            assert proc.wait(timeout=6)==0
        finally:
            if proc.poll() is None:
                proc.terminate()

    frames=[line.split("\t") for line in timing.read_text().splitlines()[1:]]
    battle=[row for row in frames if row[3]=="battle"]
    minimum=15 if software else 100
    assert len(battle)>minimum,(name,"Too few frames in the battle",len(battle))
    assert all(int(row[2])==int(not software) for row in battle),(name,"Unexpected backend")
    assert any(float(row.split("\t")[2])>=2800 for row in trace.read_text().splitlines()),(name,"Camera did not pan")
    segments=[(mode,list(rows)) for mode,rows in groupby(battle,key=lambda row:(int(row[4]),int(row[6])))]
    assert segments[0][0]==(60,0),"Initial presentation mode changed"
    report=measure(segments[0][1],2000)
    assert abs(report["simTicksPerSecond"]-30)<=1, (name,report)
    if check_limits:
        limits=segments[1:1+len(FPS_LIMITS)]
        assert all(sync==0 for (_,sync),_ in limits)
        samples={limit:measure(rows,650) for (limit,_),rows in limits}
        assert set(samples)==set(FPS_LIMITS),samples
        for limit,sample in samples.items():
            assert abs(sample["simTicksPerSecond"]-30)<=1,(limit,sample)
            if limit:
                assert .9*limit<=sample["fps"]<=1.04*limit,(limit,sample)
        assert samples[0]["fps"]>samples[240]["fps"],samples
        report["limits"]=samples
    if check_vsync:
        start=1+len(FPS_LIMITS)*check_limits
        synchronized=segments[start:]
        expected=[(0,0),(0,1),(30,1),(30,0)]
        assert [mode for mode,_ in synchronized]==expected,[mode for mode,_ in synchronized]
        samples={}
        for (limit,sync),rows in synchronized:
            sample=measure(rows,650)
            assert abs(sample["simTicksPerSecond"]-30)<=1,sample
            if limit:
                assert sample["fps"]<=1.04*limit,sample
            samples[f"{limit or 'unlimited'}_{'on' if sync else 'off'}"]=sample
        report["vsync"]=samples
    return report


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--compare-software",action="store_true")
    parser.add_argument("--fps-limits",action="store_true")
    parser.add_argument("--vsync",action="store_true")
    args=parser.parse_args()
    OUT.mkdir(exist_ok=True,parents=True)
    report={"direct2d":live_run(False,args.fps_limits,args.vsync)}
    print("direct2d",report["direct2d"],flush=True)
    if args.compare_software:
        report["software"]=live_run(True)
    (OUT/"display-checks.json").write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report,indent=2))
