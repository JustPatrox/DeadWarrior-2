"""Measure the game's actual Windows session while only its main-menu music plays."""
import json
from pathlib import Path
import subprocess
import tempfile
import time
from native_client import capture_from_game
from reference_projector import user,window

ROOT=Path(__file__).resolve().parents[1]
out=ROOT/"evidence/windows-interaction"
out.mkdir(parents=True,exist_ok=True)
capture=out/"menu-music.png"
with tempfile.TemporaryDirectory() as save:
    with subprocess.Popen([ROOT/"out/windows-x64/DeadWarrior 2.exe","--save-dir",save,"--capture-client",capture]) as game:
        try:
            time.sleep(11)
            hwnd=next(h for h,c in window(game.pid) if c=="DeadWarrior2Native")
            meter=subprocess.run([ROOT/"build/windows/Release/audio_probe.exe","--process-peak",str(game.pid)],capture_output=True,text=True)
            capture_from_game(hwnd,capture)
            log=(ROOT/"out/windows-x64/lastest.log").read_text()
            assert "AUDIO music=loginMusic loop=1 gain=0.672000" in log,log
            assert meter.returncode==0,meter.stdout+meter.stderr
            report=dict(nativeSession=True,menuOnly=True,peak=float(meter.stdout.split()[-1]),gain=.672)
            user.PostMessageW(hwnd,0x10,0,0); assert game.wait(timeout=8)==0
            (out/"menu-music-checks.json").write_text(json.dumps(report,indent=2)+"\n")
            (out/"menu-music-lastest.log").write_bytes((ROOT/"out/windows-x64/lastest.log").read_bytes())
            print(report)
        finally:
            if game.poll() is None: game.terminate()
