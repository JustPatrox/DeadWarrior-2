"""Exercise Explosive War, allied Giant production, victory, map, and save in Win32."""
import argparse
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
from reference_projector import user,window

ROOT=Path(__file__).resolve().parents[1]


def run(width,height):
    out=ROOT/'evidence/windows-bomber';out.mkdir(exist_ok=True,parents=True)
    prefix=f'{width}x{height}';trace=out/f'{prefix}-steps.tsv';capture=out/f'{prefix}-capture.png'
    trace.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory() as temporary:
        folder=Path(temporary);save=folder/'campaign.sav'
        upgrades=sum(1<<n for n in (1,2,4,7,11,16,20))
        save.write_text(f'DeadWarrior2-Campaign 1\n6 0 1 {upgrades:021b}\n'+'220 220 1\n'*6+'-1 0 0\n'*8)
        exe=Path(os.environ.get('DW2_TEST_EXE',ROOT/'out/windows-x64/DeadWarrior 2.exe'))
        with subprocess.Popen([exe,'--save-dir',folder,'--capture-client',capture,'--trace-tutorial',trace,'--magenta']) as process:
            try:
                until=time.monotonic()+15
                while time.monotonic()<until:
                    hwnd=next((h for h,c in window(process.pid) if c=='DeadWarrior2Native'),None)
                    if hwnd:break
                    time.sleep(.1)
                else:raise AssertionError('Native window missing')
                style=user.GetWindowLongW(hwnd,-16)
                user.SetWindowLongW(hwnd,-16,(style&~0x00cf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                def click(x,y):
                    px=round(width/2+(x-425)*height/700);py=round(y*height/700)
                    for msg,button in ((0x200,0),(0x201,1),(0x202,0)):user.SendMessageW(hwnd,msg,button,(py<<16)|(px&65535))
                def row():
                    rows=trace.read_text().splitlines() if trace.exists() else []
                    return rows[-1].split('\t') if rows else []
                def wait(check,limit=20):
                    until=time.monotonic()+limit
                    while time.monotonic()<until:
                        if process.poll() is not None:raise AssertionError(f'Native process exited: {process.returncode}')
                        current=row()
                        if current and check(current):return current
                        time.sleep(.1)
                    raise AssertionError(f'Native state did not match: {row()}')
                def shot(name):
                    image=capture_from_game(hwnd,capture);image.save(out/f'{prefix}-{name}.png')
                    assert not np.any(np.all(np.asarray(image)==[255,0,255],axis=2)),name
                time.sleep(11);click(161,664);time.sleep(1.2);click(578,664)
                wait(lambda r:r[0]=='101' and r[1]=='341');shot('map-entry');click(207,299)
                initial=wait(lambda r:r[0]=='27')
                assert len(initial)==44 and initial[34]=='6' and initial[35]=='1' and float(initial[11])==1500,initial
                shot('entry');click(310,600);time.sleep(2.6)
                current=row();assert float(current[2])<=201,current
                shot('castle-left');click(550-float(current[2]),300);click(675,606)
                wait(lambda r:int(r[7])>=0)
                # The original campaign controller runs once per two updates in
                # fast-forward and tests frame modulo. Start on an even frame
                # so this battle checks actual waves instead of skipping them.
                until=time.monotonic()+2
                while time.monotonic()<until:
                    tick=user.SendMessageW(hwnd,0x8000+22,0,0)
                    if tick>0 and tick%2==0:click(554,582);break
                    time.sleep(.001)
                else:raise AssertionError('Even simulation frame not observed')
                wait(lambda r:r[16]=='1')
                next_order=0;seen=set();queued=-1;first=-1;previous=0;healer_queued=False
                until=time.monotonic()+400
                while time.monotonic()<until:
                    current=row();tick=int(current[1])
                    if current[0] in ('28','29'):break
                    assert current[16]=='1',current
                    gold,miners,archers,giants,population=(int(current[n]) for n in (5,41,42,40,43))
                    if tick>=next_order:
                        next_order=tick+120
                        if not healer_queued and gold>=300:
                            click(200,610);healer_queued=True
                        if miners+int(current[13])<16:click(30,610)
                        if archers<3 and gold>=200:click(146,610)
                        # Use current, rather than previously displayed gold after
                        # other orders, so the UI makes the affordability decision.
                        if miners>=14 and gold>=1500 and giants<4:
                            click(230,665)
                            if queued<0:queued=tick
                        click(500 if giants>=3 else 425,634)
                    if giants and first<0:
                        first=tick;shot('giant-ready');click(384,600)
                    if int(current[38])>0 and 'throw' not in seen:
                        seen.add('throw');shot('throw')
                    if int(current[39])>0 and 'impact' not in seen:
                        seen.add('impact');shot('impact')
                    if int(current[37])>0 and 'explosion' not in seen:
                        seen.add('explosion');shot('explosion')
                    if tick>=previous+1800:
                        previous=tick;print(f'{prefix}: tick={tick}, giants={giants}, statues={current[10:12]}',flush=True)
                    time.sleep(.12)
                assert current[0]=='28',current
                assert seen=={'throw','impact','explosion'} and first>queued>0,(seen,first,queued)
                assert int(current[35])>=3,current
                wait(lambda r:r[15]=='1');time.sleep(.5);shot('victory')
                click(580,653);wait(lambda r:r[0]=='100');shot('upgrades')
                click(750,664);wait(lambda r:r[0]=='100' and r[4]=='1');click(353,381)
                wait(lambda r:r[0]=='101' and r[1]=='401');shot('juggerknights-map')
                click(309,644);wait(lambda r:r[0]=='101' and r[5]=='1')
                record=save.read_text().splitlines();assert record[1].split()[:2]==['7','1'],record[:2]
                report=dict(resolution=[width,height],victory=True,victoryTick=int(current[1]),
                    waves=int(current[35]),explosions=int(current[37]),boulderShots=int(current[38]),boulderHits=int(current[39]),
                    firstGiant=first,firstQueued=queued,savedLevel=7,nextMapFrame=401,magentaPixels=0)
                (out/f'{prefix}-checks.json').write_text(json.dumps(report,indent=2)+'\n')
                user.PostMessageW(hwnd,0x10,0,0);assert process.wait(timeout=8)==0
                print(report,flush=True)
            finally:
                if process.poll() is None:process.terminate()
                log=exe.parent/'lastest.log'
                if log.exists():(out/f'{prefix}-lastest.log').write_bytes(log.read_bytes())


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('width',type=int);parser.add_argument('height',type=int)
    args=parser.parse_args();run(args.width,args.height)
