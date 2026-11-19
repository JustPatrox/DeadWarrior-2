"""Drive the native Win32 window via its client mouse and keyboard messages."""
import ctypes as ct
from ctypes import wintypes as wt
import json
from pathlib import Path
import subprocess
import time

import numpy as np
from PIL import Image
from native_client import capture_client


user = ct.windll.user32
user.SetProcessDpiAwarenessContext(ct.c_void_p(-4))
user.FindWindowW.restype = wt.HWND
out = Path("evidence/windows-menu")


def check():
    process = subprocess.Popen(["out/windows-x64/DeadWarrior 2.exe"])
    try:
        deadline = time.monotonic()+15
        while time.monotonic()<deadline:
            hwnd = user.FindWindowW("DeadWarrior2Native","DeadWarrior 2")
            pid = wt.DWORD()
            user.GetWindowThreadProcessId(hwnd,ct.byref(pid))
            if hwnd and pid.value==process.pid:
                break
            time.sleep(0.1)
        else:
            raise RuntimeError("Game window did not open")

        def size():
            r=wt.RECT()
            user.GetClientRect(hwnd,ct.byref(r))
            return r.right,r.bottom

        def click(x,y):
            coord = (round(y)<<16) | round(x)
            for message,key in ((0x200,0),(0x201,1),(0x202,0)):
                user.SendMessageW(hwnd,message,key,coord)
            time.sleep(0.25)

        def stage(x,y):
            w,h=size()
            s=min(w/(700*16/9),h/700)
            return w/2+(x-425)*s,(h-700*s)/2+y*s

        def shot(name):
            image=capture_client(hwnd)
            image.save(out/f"window-{name}.png")
            return np.asarray(image.convert("RGB"))

        def delta(a,b):
            return int(np.any(a!=b,axis=2).sum())

        out.mkdir(parents=True, exist_ok=True)
        time.sleep(0.4)
        notice=shot("startup")
        assert notice[0,0].max()==0 and notice.std()>3, "Startup notice missing"
        get_class_icon=user.GetClassLongPtrW
        get_class_icon.restype=ct.c_size_t
        assert get_class_icon(hwnd,-14) and get_class_icon(hwnd,-34), "Window icons missing"
        time.sleep(11)
        w,h=size()
        main=shot("main")
        assert main.std()>18, "Blank window capture"
        left=(850-700*16/9)/2
        click(*stage(left+24,18))
        muted=shot("muted")
        # The sliding panels can still ease by a fraction of a pixel. Measure
        # the toggle inside its anchored control, independently of the panels.
        x,y=map(round,stage(left+55,40))
        difference=delta(main[:y,:x],muted[:y,:x])
        assert difference>10, "Music control did not change"
        click(*stage(850-left-25,18))
        credits=shot("credits")
        assert delta(muted,credits)>w*h/8, "Right-anchored credits control did not open"
        click(*stage(850-left-25,18))
        user.SendMessageW(hwnd,0x100,0x7A,0)  # F11
        full=size()
        assert full==(user.GetSystemMetrics(0),user.GetSystemMetrics(1))
        time.sleep(0.35)
        shot("fullscreen-main")
        click(*stage(161,664))
        time.sleep(1.5)
        submenu=shot("fullscreen-new")
        user.SendMessageW(hwnd,0x100,0x7A,0)
        time.sleep(0.3)
        # Force a tall client ratio. The upper letterbox must ignore a hit at
        # the same x as the back control that works inside the viewport.
        user.SetWindowPos(hwnd,0,0,0,990,900,0x16)
        time.sleep(0.5)
        tall=shot("tall-new")
        click(stage(850-left-25,18)[0],10)
        assert delta(tall,shot("tall-band-click"))<10, "Letterbox click changed the scene"
        click(*stage(850-left-25,18))
        time.sleep(1.5)
        assert delta(tall,shot("tall-back"))>1000, "Back control did not work at tall aspect"
        # Check that the visual actually moved to the new-game screen.
        assert delta(submenu,np.asarray(Image.open(out/"window-fullscreen-main.png").convert("RGB")))>1000
        report=dict(window=[w,h],fullscreen=full,tall=size(),dpi=user.GetDpiForWindow(hwnd),startupNotice=True,
                    windowName="DeadWarrior 2",windowIcons=True,
                    musicChangedPixels=difference,credits=True,fullscreenCampaign=True,letterboxIgnored=True)
        (out/"window-checks.json").write_text(json.dumps(report,indent=2))
        print(report)
    finally:
        process.terminate()
        process.wait(timeout=10)


if __name__ == "__main__":
    check()
