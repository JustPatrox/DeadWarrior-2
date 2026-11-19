"""Capture the supplied SWF running in Adobe's Windows standalone player."""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
from pathlib import Path
import shutil
import subprocess
import time
import numpy as np
from PIL import Image, ImageGrab

user = ct.windll.user32
user.SetProcessDpiAwarenessContext(ct.c_void_p(-4))
root = Path(__file__).resolve().parents[1]


def window(pid):
    handles = []
    @ct.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def visit(hwnd, unused):
        proc = wt.DWORD()
        user.GetWindowThreadProcessId(hwnd,ct.byref(proc))
        if proc.value == pid and user.IsWindowVisible(hwnd):
            cls = ct.create_unicode_buffer(256)
            user.GetClassNameW(hwnd,cls,len(cls))
            handles.append((hwnd,cls.value))
        return True
    user.EnumWindows(visit,0)
    return handles


def unobscured_frame(hwnd):
    # This projector draws directly to the display. PrintWindow and Windows
    # Graphics Capture can return its white GDI surface and omit the SWF.
    # Read its displayed rectangle only while every pixel belongs to this HWND.
    user.GetTopWindow.restype=wt.HWND
    user.GetWindow.argtypes=[wt.HWND,wt.UINT]
    user.GetWindow.restype=wt.HWND
    def visible():
        bounds=wt.RECT()
        if not user.GetWindowRect(hwnd,ct.byref(bounds)) or user.IsIconic(hwnd): return False
        above=user.GetTopWindow(None)
        while above and above!=hwnd:
            rect=wt.RECT()
            if user.IsWindowVisible(above) and not user.IsIconic(above) and user.GetWindowRect(above,ct.byref(rect)):
                if max(bounds.left,rect.left)<min(bounds.right,rect.right) and max(bounds.top,rect.top)<min(bounds.bottom,rect.bottom):
                    cloaked=wt.DWORD()
                    ct.windll.dwmapi.DwmGetWindowAttribute(wt.HWND(above),14,ct.byref(cloaked),ct.sizeof(cloaked))
                    if not cloaked.value: return False
            above=user.GetWindow(above,2)
        return above==hwnd
    raised=not visible()
    if raised:
        user.SetWindowPos(hwnd,-1,0,0,0,0,0x13)  # No movement, resizing or activation.
        ct.windll.dwmapi.DwmFlush()
    try:
        if not visible(): raise RuntimeError("Adobe player window is occluded")
        image=ImageGrab.grab(window=hwnd).convert("RGB")
        if not visible(): raise RuntimeError("Adobe player was obscured during capture")
        return image
    finally:
        if raised: user.SetWindowPos(hwnd,-2,0,0,0,0,0x13)


def run(width,height,tutorial=False,visible_reference=False):
    from windows_capture import WindowsCapture
    out = root/("work/adobe-tutorial-check" if tutorial else "work/adobe-entry-check")
    out.mkdir(parents=True,exist_ok=True)
    captured=[]
    with subprocess.Popen([root/"work/tools/flashplayer-sa.exe",root/"stickwar2.swf"]) as player:
        capture_control=None
        try:
            until=time.monotonic()+15
            while time.monotonic()<until:
                candidates=window(player.pid)
                main=next((h for h,c in candidates if "Flash" in c),None)
                if main: break
                time.sleep(.1)
            else:
                raise RuntimeError(f"No Flash window: {candidates}")
            user.SetMenu(main,None)
            style=user.GetWindowLongW(main,-16)
            user.SetWindowLongW(main,-16,(style & ~0x00CF0000)|0x80000000)
            user.SetWindowPos(main,0,0,0,width,height,0x34)
            latest=[]
            if not visible_reference:
                session=WindowsCapture(cursor_capture=False,draw_border=False,window_hwnd=main)
                @session.event
                def on_frame_arrived(frame,control):
                    latest[:]=[frame.frame_buffer.copy()]
                @session.event
                def on_closed():
                    pass
                capture_control=session.start_free_threaded()

            def capture(label):
                r=wt.RECT()
                user.GetClientRect(main,ct.byref(r))
                if visible_reference:
                    im=unobscured_frame(main)
                else:
                    if not latest: raise RuntimeError("No frames from the Adobe player window")
                    im=Image.fromarray(latest[0][:,:,2::-1])
                if (r.right,r.bottom)!=(width,height) or im.size!=(width,height) or not im.getbbox():
                    raise RuntimeError(f"Invalid player capture: client {r.right}x{r.bottom}, image {im.size}")
                if label in {"after-intro","move","scroll","attack","return","miner"}:
                    s=height/700
                    area=np.asarray(im)[round(15*s):round(192*s),round(135*s):round(740*s)]
                    gold=(area[:,:,0]>220)&(area[:,:,1]>105)&(area[:,:,1]<215)&(area[:,:,2]<40)
                    if gold.sum()<1000*s*s:
                        raise RuntimeError(f"Tutorial message frame missing at {label}")
                path=out/f"original-{width}x{height}-{label}.png"
                im.save(path)
                captured.append(path)
                print(label,window(player.pid),flush=True)

            def send(code,x,y,buttons=0):
                p=wt.POINT(round(x*height/700),round(y*height/700))
                # Route input to the child rendering surface, if present.
                target=user.ChildWindowFromPointEx(main,p,7) or main
                user.MapWindowPoints(main,target,ct.byref(p),1)
                pos=(p.y<<16)|(p.x&0xffff)
                user.PostMessageW(target,code,buttons,pos)
            def click(x,y,right=False):
                send(0x200,x,y)
                time.sleep(.12)
                send(0x204 if right else 0x201,x,y,2 if right else 1)
                time.sleep(.18)
                send(0x205 if right else 0x202,x,y)

            time.sleep(3)
            capture("opening")
            time.sleep(9)
            capture("main")
            for name,x,y,wait in (("new",161,664,2),("difficulty",437,664,2),
                                  ("intro",154,668,5),("after-intro",434,639,8)):
                click(x,y)
                time.sleep(wait)
                capture(name)
            if tutorial:
                send(0x200,580,260)
                send(0x201,580,260,1)
                time.sleep(.16)
                send(0x200,720,510,1)
                time.sleep(.25)
                send(0x202,720,510)
                time.sleep(3)
                capture("move")
                click(150,420,True)
                time.sleep(7)
                capture("scroll")
                send(0x200,845,165)
                time.sleep(7)
                send(0x200,425,225)
                capture("attack")
                click(550,430,True)
                time.sleep(18)
                capture("return")
                click(350,600)
                time.sleep(6)
                capture("miner")
            evidence=root/"evidence/adobe"
            evidence.mkdir(parents=True,exist_ok=True)
            for path in captured:
                shutil.copyfile(path,evidence/path.name)
        finally:
            if capture_control is not None: capture_control.stop()
            player.terminate()


if __name__ == "__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("width",type=int)
    parser.add_argument("height",type=int)
    parser.add_argument("--tutorial",action="store_true")
    parser.add_argument("--visible-reference",action="store_true")
    args=parser.parse_args()
    run(args.width,args.height,args.tutorial,args.visible_reference)
