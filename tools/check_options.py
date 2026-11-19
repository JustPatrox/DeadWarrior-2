"""Verify persisted Insert settings, fullscreen restore, and save deletion safety."""
import configparser
import ctypes as ct
from ctypes import wintypes as wt
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
from reference_projector import user,window
from native_client import capture_from_game

ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get("DW2_TEST_EXE",ROOT/"out/windows-x64/DeadWarrior 2.exe"))
OUT=ROOT/"evidence/windows-options"


def run():
    OUT.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory() as temporary:
        folder=Path(temporary);options=folder/"options.ini";save=folder/"campaign.sav"
        save.write_text("DeadWarrior2-Campaign 1\n6 6 1 000000000000000000010\n"+"220 220 1\n"*6+"-1 0 0\n"*8)
        sentinel=folder/"unrelated.txt";sentinel.write_text("preserve")
        def settings():
            parser=configparser.ConfigParser();parser.read(options);return dict(parser['Display'])
        def launch(check):
            capture=OUT/"capture.png"
            with subprocess.Popen([EXE,"--save-dir",folder,"--capture-client",capture]) as process:
                try:
                    until=time.monotonic()+15
                    while time.monotonic()<until:
                        hwnd=next((h for h,c in window(process.pid) if c=="DeadWarrior2Native"),None)
                        if hwnd:break
                        time.sleep(.1)
                    else:raise AssertionError("Native window missing")
                    def key(code):user.SendMessageW(hwnd,0x100,code,0)
                    def size():
                        r=wt.RECT();user.GetClientRect(hwnd,ct.byref(r));return r.right,r.bottom
                    def click(x,y):
                        w,h=size();s=min(w/(700*16/9),h/700)
                        px=round(w/2+(x-425)*s);py=round((h-700*s)/2+y*s)
                        for msg,b in ((0x200,0),(0x201,1),(0x202,0)):user.SendMessageW(hwnd,msg,b,(py<<16)|(px&65535))
                    def query(n):return user.SendMessageW(hwnd,0x8000+n,0,0)
                    def wait(test,seconds=12):
                        deadline=time.monotonic()+seconds
                        while time.monotonic()<deadline:
                            if test():return
                            if process.poll() is not None:raise AssertionError("Native process exited")
                            time.sleep(.05)
                        raise AssertionError("Options test timed out")
                    check(hwnd,key,click,query,wait,size,capture)
                    user.PostMessageW(hwnd,0x10,0,0);assert process.wait(timeout=10)==0
                finally:
                    if process.poll() is None:process.terminate()
        def configure(hwnd,key,click,query,wait,size,capture):
            time.sleep(11)
            assert settings()==dict(mode='windowed',resolution='auto',fps_limit='60',vsync='0',show_fps='0')
            key(0x2d);click(615,200);click(615,200)
            click(257,448);click(588,507);click(588,559);click(425,300);key(0x1b)
            assert settings()==dict(mode='borderless',resolution='1600x900',fps_limit='30',vsync='1',show_fps='1')
            assert not (folder/'options.ini.tmp').exists()
        launch(configure)
        expected=options.read_bytes()
        def restore_and_delete(hwnd,key,click,query,wait,size,capture):
            time.sleep(11)
            assert options.read_bytes()==expected
            assert query(19)>0 and query(20)==(1600<<16)|900
            click(161,664);time.sleep(1.2);click(578,664);wait(lambda:query(18)&1)
            capture_from_game(hwnd,capture).save(OUT/'thanks-for-playing.png')
            click(175,465);wait(lambda:not query(18)&1)
            assert not save.exists() and options.read_bytes()==expected and sentinel.read_text()=='preserve'
        launch(restore_and_delete)
        options.write_text('[Display]\nmode=fullscreen\nresolution=auto\nfps_limit=120\nvsync=0\nshow_fps=1\n')
        def fullscreen(hwnd,key,click,query,wait,size,capture):
            time.sleep(.5)
            capture_from_game(hwnd,capture).save(OUT/'project-notice.png')
            assert size()==(user.GetSystemMetrics(0),user.GetSystemMetrics(1))
            key(0x7a);wait(lambda:size()!=(user.GetSystemMetrics(0),user.GetSystemMetrics(1)))
            assert user.IsWindowVisible(hwnd) and settings()['mode']=='windowed'
        launch(fullscreen)
        options.write_text('[Display]\nmode=invalid\nresolution=giant\nfps_limit=999999999999999999999\nvsync=wrong\nshow_fps=-5\n')
        def invalid(hwnd,key,click,query,wait,size,capture):
            wait(lambda:settings()==dict(mode='windowed',resolution='auto',fps_limit='60',vsync='0',show_fps='0'))
            assert user.IsWindowVisible(hwnd)
        launch(invalid)
        report=dict(allInsertOptionsPersisted=True,relaunch=True,optionsSurviveDeleteSave=True,
            fullscreenRestoresAndRemainsVisible=True,invalidValuesUseDefaults=True)
        (OUT/'checks.json').write_text(json.dumps(report,indent=2)+'\n');print(report,flush=True)


if __name__=='__main__':run()
