"""Exercise fresh Dynamic defaults, portrait menu pages and legacy saved choice."""
import configparser
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
OUT=Path(os.environ.get('DW2_TEST_EVIDENCE',ROOT/'work/desktop-ai-menu'))

def run():
    OUT.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory() as temporary:
        folder=Path(temporary)
        def launch(legacy=False):
            capture=OUT/'capture.png'
            with subprocess.Popen([EXE,'--save-dir',folder,'--capture-client',capture]) as process:
                try:
                    def wait(test,seconds=18):
                        end=time.monotonic()+seconds
                        while time.monotonic()<end:
                            if test():return
                            if process.poll() is not None:raise AssertionError('Native game exited')
                            time.sleep(.05)
                        raise AssertionError('Native menu condition timed out')
                    hwnd=None
                    def find():
                        nonlocal hwnd
                        hwnd=next((h for h,c in window(process.pid) if c=='DeadWarrior2Native'),None)
                        return hwnd is not None
                    wait(find)
                    user.SetWindowLongW(hwnd,-16,(user.GetWindowLongW(hwnd,-16)&~0xcf0000)|0x80000000)
                    user.SetWindowPos(hwnd,0,0,0,0,0,0x37)
                    def query(n):return user.SendMessageW(hwnd,0x8000+n,0,0)
                    def size():
                        r=wt.RECT();user.GetClientRect(hwnd,ct.byref(r));return r.right,r.bottom
                    def resize(w,h):
                        oldw,oldh=size();r=wt.RECT();user.GetWindowRect(hwnd,ct.byref(r))
                        user.SetWindowPos(hwnd,0,20,20,r.right-r.left+w-oldw,r.bottom-r.top+h-oldh,0x14)
                        wait(lambda:size()==(w,h));time.sleep(.2)
                    def click(x,y):
                        w,h=size();vw=query(23)/100;vh=query(24)/100;s=min(w/vw,h/vh)
                        px=round(w/2+(x-425)*s);py=round((h-vh*s)/2+y*s)
                        for msg,b in ((0x200,0),(0x201,1),(0x202,0)):user.SendMessageW(hwnd,msg,b,(py<<16)|(px&65535))
                    wait(lambda:query(28)==1)
                    if legacy:
                        resize(686,720)
                        assert query(24)==70000 and query(23)==124444
                        time.sleep(1.5) # Main has appeared; allow its original slide-in to settle.
                        click(161,664);wait(lambda:query(28)==2)
                        capture_from_game(hwnd,capture).save(OUT/'legacy-fixed-new.png')
                    else:
                        ini=configparser.ConfigParser();ini.read(folder/'options.ini')
                        assert ini['Display']['aspect_mode']=='dynamic'
                        for w,h in ((686,720),(720,960),(1280,720),(1920,1080)):
                            resize(w,h);vh=query(24)/100
                            assert abs(query(23)/query(24)-w/h)<.0001
                            for name,button,page in (('main',161,2),('new',437,3),('difficulty',154,4)):
                                time.sleep(1.5)
                                image=capture_from_game(hwnd,capture);image.save(OUT/f'{w}x{h}-{name}.png')
                                if h>w and name=='main':
                                    pixels=np.asarray(image);band=pixels[int(h*.72):h-85,60:w-60]
                                    gold=(band[:,:,0]>120)&(band[:,:,1]>90)&(band[:,:,2]<65)
                                    assert int(gold.sum())<20,'Inactive gold menu buttons leaked into portrait window'
                                click(button,vh-(32 if name=='difficulty' else 36));wait(lambda:query(28)==page)
                            time.sleep(3.2)
                            capture_from_game(hwnd,capture).save(OUT/f'{w}x{h}-intro.png')
                            # Return through the original top-right Back button.
                            click(425+(query(23)/100)/2-25,25);wait(lambda:query(28)==1)
                        # A tall resize between UI ticks must retain the current page.
                        resize(686,720);assert query(28)==1
                    user.PostMessageW(hwnd,0x10,0,0);assert process.wait(timeout=10)==0
                finally:
                    if process.poll() is None:process.terminate();process.wait(timeout=10)
        launch()
        (folder/'options.ini').write_text('[Display]\naspect_mode=16:9\n',encoding='utf-8')
        launch(True)
        ini=configparser.ConfigParser();ini.read(folder/'options.ini');assert ini['Display']['aspect_mode']=='16:9'
    report=dict(dynamicDefault=True,portraitPages=True,resizeInput=True,legacyFixedChoicePreserved=True)
    (OUT/'checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(report,flush=True)

if __name__=='__main__':run()
