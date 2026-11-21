"""Exercise miner retargeting and defensive facing through a real native window."""
import csv
import ctypes as ct
from ctypes import wintypes as wt
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
from native_client import capture_from_game
from reference_projector import user, window

ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get('DW2_TEST_EXE',ROOT/'out/windows-x64/DeadWarrior 2.exe'))
OUT=ROOT/'evidence/windows-economy-orders'
RULES=dict(line.split() for line in (ROOT/'assets/tutorial/rules.tsv').read_text(encoding='utf-8').splitlines())
BATTLEFIELD_Y=float(RULES['battlefieldY'])


def run(width,height):
    OUT.mkdir(parents=True,exist_ok=True)
    prefix=f'{width}x{height}'
    capture=OUT/f'{prefix}-capture.png';trace=OUT/f'{prefix}-steps.tsv'
    trace.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory() as temporary:
        save=Path(temporary)
        (save/'campaign.sav').write_text('DeadWarrior2-Campaign 1\n1 1 1 000000000000000000010\n'+'220 220 1\n'+'-1 0 0\n'*13,encoding='utf-8')
        with subprocess.Popen([EXE,'--save-dir',save,'--capture-client',capture,'--trace-tutorial',trace,'--magenta']) as game:
            try:
                until=time.monotonic()+20
                while time.monotonic()<until:
                    hwnd=next((h for h,c in window(game.pid) if c=='DeadWarrior2Native'),None)
                    if hwnd:break
                    time.sleep(.1)
                else:raise AssertionError('Native window did not appear')
                style=user.GetWindowLongW(hwnd,-16)
                user.SetWindowLongW(hwnd,-16,(style&~0xcf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                def send(message,x,y,button=0):
                    px=round(width/2+(x-425)*height/700);py=round(y*height/700)
                    user.SendMessageW(hwnd,message,button,(py<<16)|(px&65535))
                def click(x,y,right=False):
                    for message,button in ((0x200,0),(0x204 if right else 0x201,2 if right else 1),(0x205 if right else 0x202,0)):
                        send(message,x,y,button)
                def state():
                    rows=trace.read_text(encoding='utf-8').splitlines() if trace.exists() else []
                    return list(map(float,rows[-1].split('\t'))) if rows else []
                def units():
                    assert user.SendMessageW(hwnd,0x8000+26,0,0)>0,'Diagnostic unit/ore snapshot missing'
                    def read(path):
                        with path.open(encoding='utf-8') as f:
                            return [{k:(v if k=='pose' else float(v)) for k,v in row.items()} for row in csv.DictReader(f,delimiter='\t')]
                    return read(capture.with_suffix('.units.tsv')),read(capture.with_suffix('.ore.tsv'))
                def wait(check,limit=30):
                    end=time.monotonic()+limit
                    while time.monotonic()<end:
                        if game.poll() is not None:raise AssertionError('Native game exited')
                        value=check()
                        if value:return value
                        time.sleep(.06)
                    raise AssertionError(f'Timed out; trace={state()}')
                def shot(name):
                    capture_from_game(hwnd,capture).save(OUT/f'{prefix}-{name}.png')
                def pick(actor):
                    click(actor['x']-state()[2],BATTLEFIELD_Y+actor['y']-30)
                time.sleep(11);click(161,664);time.sleep(1.2);click(578,664)
                motion=list(map(float,(ROOT/'assets/intermission/map-motion.tsv').read_text(encoding='utf-8').splitlines()[-1].split()))
                stop,scale,mx,my,flag_x,flag_y=motion
                flag=next(line.split() for line in (ROOT/'assets/intermission/textures.tsv').read_text(encoding='utf-8').splitlines() if line.startswith('393\t16\t'))
                bx,by,bw,bh=map(float,flag[3:])
                wait(lambda:state() and state()[:2]==[101,stop])
                click(mx+scale*(flag_x+bx+bw/2),my+scale*(flag_y+by+bh/2))
                wait(lambda:state() and state()[0]==27)
                click(554,582) # Original fast-forward control, independent of rendering rate.
                def ready():
                    actors,ores=units();workers=[u for u in actors if u['symbol']==3104 and not u['enemy'] and u['health']>0]
                    full=next((int(u['resource']) for u in workers if u['resource']>=0 and sum(v['resource']==u['resource'] and v['slot']>=0 for v in workers)==2),None)
                    guest=next((u for u in workers if u['resource']>=0 and u['resource']!=full),None)
                    return (guest,full,ores) if full is not None and guest else None
                guest,resource,ores=wait(ready)
                miner_id=guest['id'];pick(guest);click(1350-state()[2],BATTLEFIELD_Y+115,right=True)
                def prayer():
                    actor=next(u for u in units()[0] if u['id']==miner_id)
                    return actor if actor['selected'] and actor['resource']==-1 and actor['pose']=='pray' else None
                wait(prayer);shot('praying')
                def full_again():
                    actors,ores=units()
                    return ores[int(resource)] if sum(not u['enemy'] and u['health']>0 and u['resource']==resource and u['slot']>=0 for u in actors)==2 else None
                ore=wait(full_again)
                click(ore['x']-state()[2],BATTLEFIELD_Y+ore['y']-5,right=True)
                def assigned():
                    actors,ores=units();actor=next(u for u in actors if u['id']==miner_id)
                    return (actor,ores) if actor['resource']>=0 and actor['pose']!='pray' else None
                actor,ores=wait(assigned)
                new_resource=int(actor['resource']);before=ores[new_resource]['remaining']
                def mining():
                    actors,ores=units();actor=next(u for u in actors if u['id']==miner_id)
                    return actor if actor['pose']=='mine' and ores[new_resource]['remaining']<before else None
                wait(mining);shot('prayer-returned-to-gold')
                user.SendMessageW(hwnd,0x100,32,0)
                click(305.2+2300/7000*231,600,right=True)
                wait(lambda:any(not u['enemy'] and u['symbol']!=3104 and u['x']>2100 for u in units()[0]))
                click(425,637)
                def defending():
                    army=[u for u in units()[0] if not u['enemy'] and u['health']>0 and u['symbol']!=3104]
                    return army if army and all(u['order']==0 and u['facing']==1 and 1600<u['x']<1760 for u in army) else None
                army=wait(defending);shot('defenders-facing-right')
                report=dict(resolution=[width,height],fullMineRetarget=True,resumedMining=True,
                            defenceFacesRight=True,defenders=army)
                (OUT/f'{prefix}-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
                print(json.dumps(report),flush=True)
                user.PostMessageW(hwnd,0x10,0,0);assert game.wait(timeout=8)==0
            except BaseException:
                if game.poll() is None:shot('failure')
                raise
            finally:
                log=EXE.parent/'lastest.log'
                if log.exists():
                    (OUT/f'{prefix}-lastest.log').write_bytes(log.read_bytes())
                if game.poll() is None:game.terminate();game.wait(timeout=8)


if __name__=='__main__':
    for dimensions in ((1280,720),(1920,1080)):run(*dimensions)
