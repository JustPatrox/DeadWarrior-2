"""Read pixels drawn by the native game to a private DC, including occluded windows."""
import ctypes as ct
from ctypes import wintypes as wt
from PIL import Image
from pathlib import Path

gdi=ct.windll.gdi32
user=ct.windll.user32
gdi.CreateCompatibleDC.argtypes=[wt.HDC]
gdi.CreateCompatibleDC.restype=wt.HDC
gdi.CreateDIBSection.argtypes=[wt.HDC,ct.c_void_p,wt.UINT,ct.POINTER(ct.c_void_p),wt.HANDLE,wt.DWORD]
gdi.CreateDIBSection.restype=wt.HBITMAP
gdi.SelectObject.argtypes=[wt.HDC,wt.HGDIOBJ]
gdi.SelectObject.restype=wt.HGDIOBJ
gdi.DeleteObject.argtypes=[wt.HGDIOBJ]
gdi.DeleteDC.argtypes=[wt.HDC]
user.SendMessageW.argtypes=[wt.HWND,wt.UINT,wt.WPARAM,wt.LPARAM]
user.SendMessageW.restype=wt.LPARAM
user.PrintWindow.argtypes=[wt.HWND,wt.HDC,wt.UINT]


class DibInfo(ct.Structure):
    _fields_=[("size",wt.DWORD),("width",wt.LONG),("height",wt.LONG),
              ("planes",wt.WORD),("bpp",wt.WORD),("compression",wt.DWORD),
              ("imageSize",wt.DWORD),("xRes",wt.LONG),("yRes",wt.LONG),
              ("colors",wt.DWORD),("importantColors",wt.DWORD)]

def capture_from_game(hwnd,path):
    """Request a fresh diagnostic render inside the native process."""
    path=Path(path)
    path.unlink(missing_ok=True)
    user.SendMessageW(hwnd,0x8000+15,0,0)
    with Image.open(path) as image:
        return image.convert("RGB")


def capture_client(hwnd):
    r=wt.RECT()
    user.GetClientRect(hwnd,ct.byref(r))
    w,h=r.right,r.bottom
    info=DibInfo(ct.sizeof(DibInfo),w,-h,1,32)
    bits=ct.c_void_p()
    dc=gdi.CreateCompatibleDC(None)
    bmp=gdi.CreateDIBSection(dc,ct.byref(info),0,ct.byref(bits),None,0)
    if not bmp:
        gdi.DeleteDC(dc)
        raise RuntimeError("Could not create client capture DIB")
    old=gdi.SelectObject(dc,bmp)
    try:
        if not user.PrintWindow(hwnd,dc,1):  # PW_CLIENTONLY, marshalled across processes
            raise RuntimeError("PrintWindow failed")
        gdi.GdiFlush()
        image=Image.frombytes("RGB",(w,h),ct.string_at(bits,w*h*4),"raw","BGRX")
        if not image.getbbox():
            raise RuntimeError("Blank client capture")
        return image
    finally:
        gdi.SelectObject(dc,old); gdi.DeleteObject(bmp); gdi.DeleteDC(dc)
