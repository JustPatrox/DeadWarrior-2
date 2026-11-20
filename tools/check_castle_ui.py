"""Capture original castle hover states, recruitment tips and restored troop selection."""
import csv
import json
from pathlib import Path
import subprocess
import tempfile
import time
import numpy as np
from native_client import capture_from_game
from reference_projector import user, window
SELECTED_MILITARY_COLUMN=45

ROOT=Path(__file__).resolve().parents[1]


def run(width,height,tooltips_only=False):
    out=ROOT/'evidence/windows-castle-todo';out.mkdir(parents=True,exist_ok=True)
    prefix=f'{width}x{height}';capture=out/f'{prefix}-capture.png'
    trace=out/f'{prefix}-trace.tsv';timing=out/f'{prefix}-frames.tsv';trace.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory() as temporary:
        folder=Path(temporary)
        (folder/'campaign.sav').write_text('DeadWarrior2-Campaign 1\n7 7 1 000000000000000000010\n'+'220 220 1\n'*7+'-1 0 0\n'*7,encoding='utf-8')
        with subprocess.Popen([ROOT/'out/windows-x64/DeadWarrior 2.exe','--save-dir',folder,
             '--capture-client',capture,'--trace-tutorial',trace,'--frame-timing',timing,'--magenta']) as game:
            try:
                deadline=time.monotonic()+15
                while time.monotonic()<deadline:
                    hwnd=next((h for h,c in window(game.pid) if c=='DeadWarrior2Native'),None)
                    if hwnd:break
                    time.sleep(.1)
                else:raise AssertionError('Window missing')
                style=user.GetWindowLongW(hwnd,-16);user.SetWindowLongW(hwnd,-16,(style&~0x00cf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                def row():
                    lines=trace.read_text(encoding='utf-8').splitlines() if trace.exists() else []
                    return lines[-1].split('\t') if lines else []
                def wait(test):
                    until=time.monotonic()+15
                    while time.monotonic()<until:
                        if test():return
                        if game.poll() is not None:raise AssertionError(('Game exited',game.returncode))
                        time.sleep(.08)
                    raise AssertionError(row())
                def send(msg,x,y,button=0):
                    x=round(width/2+(x-425)*height/700);y=round(y*height/700)
                    user.SendMessageW(hwnd,msg,button,(y<<16)|(x&65535))
                def click(x,y):
                    for msg,b in ((0x200,0),(0x201,1),(0x202,0)):send(msg,x,y,b)
                def shot(name):
                    picture=capture_from_game(hwnd,capture);picture.save(out/f'{prefix}-{name}.png')
                    assert not np.any(np.all(np.asarray(picture)==[255,0,255],axis=2)),name
                time.sleep(11);click(161,664);time.sleep(1.2);click(578,664)
                wait(lambda:row()[:2]==['101','401']);click(425,300);wait(lambda:row() and row()[0]=='27')
                click(306,600);wait(lambda:row() and 197<=float(row()[2])<=201)
                camera=float(row()[2])
                castle=() if tooltips_only else (('bank',60,350),('temple',145,160),('siege',325,70),
                                 ('magic',432,130),('archery',550,100),('barracks',650,150))
                for name,x,y in castle:
                    send(0x200,x-camera,y)
                    for frame in range(3):
                        time.sleep(.2);shot(f'{name}-{frame}')
                send(0x200,30,610);time.sleep(.25);shot('miner-tooltip')
                if tooltips_only:
                    for name,x,y in (('sword',88,610),('spearton',62,662),('archer',145,610),('ninja',116,662),
                                     ('wizard',260,610),('healer',203,610),('giant',235,665)):
                        send(0x200,x,y);time.sleep(.25);shot(name+'-tooltip')
                click(550-camera,100);shot('castle-selected')
                user.SendMessageW(hwnd,0x100,32,0);user.SendMessageW(hwnd,0x101,32,0)
                wait(lambda:len(row())>SELECTED_MILITARY_COLUMN and int(row()[SELECTED_MILITARY_COLUMN])>0)
                selected=int(row()[SELECTED_MILITARY_COLUMN])
                shot('army-selected');send(0x200,550-camera,100);time.sleep(2)
                user.SendMessageW(hwnd,0x8000+19,0,0)
                sample=list(csv.DictReader(timing.open(),delimiter='\t'))[-40:]
                fps=1000*(len(sample)-1)/sum(float(r['interval_ms']) for r in sample[1:])
                assert fps>50,('Castle presentation rate',fps)
                report=dict(resolution=[width,height],castleStates=len(castle),hoverFramesPerState=3,
                            armySelectedAfterCastle=selected,tooltipCaptured=True,magentaPixels=0,presentationFps=round(fps,2))
                (out/f'{prefix}-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
                print(report,flush=True);user.PostMessageW(hwnd,0x10,0,0);assert game.wait(timeout=8)==0
            finally:
                if game.poll() is None:game.terminate();game.wait(timeout=5)


if __name__=='__main__':
    run(1280,720)
    run(1920,1080)
