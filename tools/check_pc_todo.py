"""Verify TODO display/input fixes against a running native Windows game."""
import configparser
import ctypes as ct
from ctypes import wintypes as wt
import json
import math
from pathlib import Path
import subprocess
import tempfile
import time
import numpy as np
from native_client import capture_from_game
from reference_projector import user,window

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'evidence/windows-pc-todo'


def run():
    OUT.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory() as temporary:
        folder=Path(temporary);options=folder/'options.ini'
        (folder/'campaign.sav').write_text('DeadWarrior2-Campaign 1\n7 7 1 000000000000000000010\n'+'220 220 1\n'*7+'-1 0 0\n'*7,encoding='utf-8')
        capture=OUT/'capture.png';trace=OUT/'steps.tsv';trace.unlink(missing_ok=True)
        with subprocess.Popen([ROOT/'out/windows-x64/DeadWarrior 2.exe','--save-dir',folder,
            '--capture-client',capture,'--trace-tutorial',trace,'--magenta']) as process:
            try:
                deadline=time.monotonic()+15
                while time.monotonic()<deadline:
                    hwnd=next((h for h,c in window(process.pid) if c=='DeadWarrior2Native'),None)
                    if hwnd:break
                    time.sleep(.1)
                else:raise AssertionError('Window missing')
                style=user.GetWindowLongW(hwnd,-16);user.SetWindowLongW(hwnd,-16,(style&~0x00cf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,1280,720,0x34)

                def query(n):return user.SendMessageW(hwnd,0x8000+n,0,0)
                def key(n):user.SendMessageW(hwnd,0x100,n,0);user.SendMessageW(hwnd,0x101,n,0)
                def size():
                    r=wt.RECT();user.GetClientRect(hwnd,ct.byref(r));return r.right,r.bottom
                def settings():
                    p=configparser.ConfigParser();p.read(options);return dict(p['Display'])
                def row():
                    lines=trace.read_text(encoding='utf-8').splitlines() if trace.exists() else []
                    return lines[-1].split('\t') if lines else []
                def wait(test,limit=15):
                    deadline=time.monotonic()+limit
                    while time.monotonic()<deadline:
                        if process.poll() is not None:raise AssertionError(f'Game exited {process.returncode}')
                        if test():return
                        time.sleep(.08)
                    raise AssertionError((row(),settings()))
                def point(x,y,battle=False):
                    w,h=size();lw,lh=query(23)/100,query(24)/100
                    if query(21)&2:y+=lh/2-409
                    if settings()['aspect_mode']=='16:9':
                        s=min(w/lw,h/lh);px=w/2+(x-425)*s;py=(h-lh*s)/2+y*s
                    else:
                        if battle and y>=575:y+=lh-700
                        px=(x-(850-lw)/2)*w/lw;py=y*h/lh
                    return round(px),round(py)
                def send(message,x,y,button=0,battle=False):
                    x,y=point(x,y,battle);user.SendMessageW(hwnd,message,button,(y<<16)|(x&65535))
                def click(x,y,battle=False):
                    send(0x200,x,y,battle=battle);send(0x201,x,y,1,battle);send(0x202,x,y,battle=battle)
                def shot(name):
                    pic=capture_from_game(hwnd,capture);pic.save(OUT/f'{name}.png')
                    pixels=np.asarray(pic);visible=pixels
                    if settings()['aspect_mode']=='16:9':
                        h,w=pixels.shape[:2];s=min(w/(700*16/9),h/700)
                        x=(w-700*16/9*s)/2;y=(h-700*s)/2
                        visible=pixels[math.ceil(y):math.floor(y+700*s),math.ceil(x):math.floor(x+700*16/9*s)]
                    assert not np.any(np.all(visible==[255,0,255],axis=2)),name
                    return pixels

                time.sleep(11);click(161,664);time.sleep(1.2);click(578,664)
                wait(lambda:row() and row()[0:2]==['101','401']);click(425,300)
                wait(lambda:row() and row()[0]=='27')
                click(344,683,True);wait(lambda:query(21)&2);shot('pc-from-toolbar')
                assert query(25)==0
                click(600,602);wait(lambda:settings()['mouse_lock']=='1');key(27)
                user.ShowWindow(hwnd,9);user.SetForegroundWindow(hwnd)
                wait(lambda:query(25)==1)
                user.ShowWindow(hwnd,6);wait(lambda:query(25)==0)
                user.ShowWindow(hwnd,9);user.SetForegroundWindow(hwnd);wait(lambda:query(25)==1)
                key(27);wait(lambda:query(21)&1);wait(lambda:query(25)==0);key(27)
                key(0x2d);wait(lambda:query(21)&2)
                click(600,602);wait(lambda:settings()['mouse_lock']=='0')
                # Borderless now occupies the monitor rather than its work area.
                click(425,300);time.sleep(.4)
                monitor=ct.windll.user32.MonitorFromWindow;monitor.argtypes=[wt.HWND,wt.DWORD];monitor.restype=wt.HANDLE
                class MonitorInfo(ct.Structure):_fields_=[('size',wt.DWORD),('monitor',wt.RECT),('work',wt.RECT),('flags',wt.DWORD)]
                info=MonitorInfo();info.size=ct.sizeof(info)
                get=ct.windll.user32.GetMonitorInfoW;get.argtypes=[wt.HANDLE,ct.POINTER(MonitorInfo)];assert get(monitor(hwnd,2),ct.byref(info))
                assert size()==(info.monitor.right-info.monitor.left,info.monitor.bottom-info.monitor.top)
                shot('borderless-monitor');click(425,243)
                reports=[]
                for aspect in ('16:9','stretched','dynamic'):
                    while settings()['aspect_mode']!=aspect:click(500,643)
                    key(27)
                    for w,h in ((1280,720),(1600,675),(2560,720),(1024,768),(720,1280)):
                        user.SetWindowPos(hwnd,0,0,0,w,h,0x34);time.sleep(.2)
                        image=shot(f'{aspect.replace(":","-")}-{w}x{h}')
                        lw,lh=query(23)/100,query(24)/100
                        if aspect=='dynamic':
                            assert abs(lw/lh-w/h)<.0001,(lw,lh,w,h)
                            assert np.any(image[3,3]!=0) and np.any(image[-4,-4]!=0),(aspect,w,h)
                        if aspect=='stretched':assert np.any(image[3,3]!=0),(aspect,w,h)
                        click(344,683,True);wait(lambda:query(21)&2);key(27)
                        reports.append(dict(mode=aspect,width=w,height=h,logicalWidth=lw,logicalHeight=lh))
                    key(0x2d);wait(lambda:query(21)&2)
                # Persist and return to the normal default before leaving.
                while settings()['aspect_mode']!='16:9':click(500,643)
                key(27)
                report=dict(pcToolbarOpens=True,mouseLock=True,releasesOnMinimize=True,releasesOnPause=True,
                    borderlessCoversMonitor=True,aspectChecks=reports,magentaPixels=0)
                (OUT/'checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
                user.PostMessageW(hwnd,0x10,0,0);assert process.wait(timeout=8)==0
                print(report,flush=True)
            finally:
                if process.poll() is None:process.terminate();process.wait(timeout=5)
                user.ClipCursor(None)


if __name__=='__main__':run()
