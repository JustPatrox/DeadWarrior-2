"""Capture only the original SWF player's client area on Windows (physical pixels)."""
import argparse
import ctypes
from ctypes import wintypes
from pathlib import Path
import subprocess
import time
from PIL import Image
from windows_capture import WindowsCapture


user = ctypes.windll.user32
user.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))


def window_for(pid):
    handles = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def collect(handle, unused):
        owner = wintypes.DWORD()
        user.GetWindowThreadProcessId(handle, ctypes.byref(owner))
        if owner.value == pid and user.IsWindowVisible(handle):
            handles.append(handle)
        return True

    user.EnumWindows(collect, 0)
    return handles[0] if handles else None


def capture(width, height, samples):
    out = Path("evidence/reference")
    out.mkdir(parents=True, exist_ok=True)
    log = out / f"player-{width}.log"
    with log.open("w") as stream, subprocess.Popen([
        "work/tools/reference-player/ruffle.exe", "--graphics", "gl", "--no-gui", "--scale", "show-all",
        "--force-scale", "--align", "center", "--force-align", "--letterbox", "off",
        "--width", str(width), "--height", str(height), "--volume", "0", "--storage", "memory",
        "--open-url-mode", "deny", "--proxy", "http://127.0.0.1:1", "--tcp-connections", "deny",
        "--config", "work/reference-config", "stickwar2.swf",
    ], stdout=stream, stderr=stream) as player:
        try:
            limit = time.monotonic() + 20
            hwnd = None
            while not hwnd:
                if time.monotonic() > limit or player.poll() is not None:
                    raise RuntimeError(f"Reference window unavailable; see {log}")
                hwnd = window_for(player.pid)
                time.sleep(0.05)
            window, client = wintypes.RECT(), wintypes.RECT()
            # Use a borderless client. The compositor may maximize a decorated
            # window and place its resize borders outside the monitor.
            user.ShowWindow(hwnd, 9)
            style = user.GetWindowLongW(hwnd, -16)
            user.SetWindowLongW(hwnd, -16, (style & ~0x00CF0000) | 0x80000000)
            user.SetWindowPos(hwnd, 0, 0, 0, width, height, 0x34)
            latest = []
            session = WindowsCapture(cursor_capture=False, draw_border=False, window_hwnd=hwnd)

            @session.event
            def on_frame_arrived(frame, control):
                latest[:] = [frame.frame_buffer.copy()]

            @session.event
            def on_closed():
                pass

            capture_control = session.start_free_threaded()
            start = time.monotonic()
            for seconds in samples:
                time.sleep(max(0, start + seconds - time.monotonic()))
                if not latest:
                    raise RuntimeError("Windows Graphics Capture returned no frame")
                picture = Image.fromarray(latest[0][:,:,2::-1])
                user.GetClientRect(hwnd, ctypes.byref(client))
                if (client.right, client.bottom) != (width, height):
                    raise RuntimeError(f"Unexpected client area {client.right} x {client.bottom}")
                if picture.size != (width, height) or not picture.getbbox():
                    raise RuntimeError(f"Empty or incorrectly sized GPU capture {picture.size}")
                picture.save(out / f"swf-{width}x{height}-{seconds:g}s.png")
            capture_control.stop()
        finally:
            player.terminate()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("width", type=int)
    parser.add_argument("height", type=int)
    parser.add_argument("samples", nargs="+", type=float)
    options = parser.parse_args()
    capture(options.width, options.height, sorted(options.samples))
