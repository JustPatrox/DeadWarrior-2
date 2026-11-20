"""Play CampaignDeads through the real Windows input, render and save paths."""
import argparse
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
COLUMNS=("step tick camera cursorX cursorY gold garrisoned research castleShots castleHits "
         "playerStatue enemyStatue reinforcement minersQueued fightersQueued results fast lancers "
         "enemyArchers arrows tower ninjas enemyWizards enemyHealers magicHits heals enemyCastleShots "
         "nuke electric poison giants giantHits cinematic dialogue bombers waves nextWave explosions "
         "boulderShots boulderHits alliedGiants miners archers population enemyKnights selectedMilitary "
         "knightHint enemyDeads gutsShots poisonHits deadsHint alliedHealers cures alliedWizards "
         "bankQ barracksQ archerQ magicQ templeQ siegeQ deadX deadY mana fighters").split()


def run(width,height):
    out=ROOT/'evidence/windows-deads';out.mkdir(parents=True,exist_ok=True)
    prefix=f'{width}x{height}';trace=out/f'{prefix}-steps.tsv';capture=out/f'{prefix}-capture.png'
    trace.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory() as temporary:
        folder=Path(temporary);save=folder/'campaign.sav'
        upgrades=sum(1<<n for n in (1,2,3,5,8,13,17))
        save.write_text(f'DeadWarrior2-Campaign 1\n8 2 1 {upgrades:021b}\n'+'220 220 1\n'*8+'-1 0 0\n'*6,
                        encoding='utf-8',newline='\n')
        exe=Path(os.environ.get('DW2_TEST_EXE',ROOT/'out/windows-x64/DeadWarrior 2.exe'))
        with subprocess.Popen([exe,'--save-dir',folder,'--capture-client',capture,'--trace-tutorial',trace,'--magenta']) as process:
            try:
                deadline=time.monotonic()+15
                while time.monotonic()<deadline:
                    hwnd=next((h for h,c in window(process.pid) if c=='DeadWarrior2Native'),None)
                    if hwnd:break
                    time.sleep(.1)
                else:raise AssertionError('Native window missing')
                style=user.GetWindowLongW(hwnd,-16)
                user.SetWindowLongW(hwnd,-16,(style&~0x00cf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)

                def send(message,x,y,button=0):
                    px=round(width/2+(x-425)*height/700);py=round(y*height/700)
                    user.SendMessageW(hwnd,message,button,(py<<16)|(px&65535))

                def click(x,y):
                    send(0x200,x,y);send(0x201,x,y,1);send(0x202,x,y)

                def space():
                    user.SendMessageW(hwnd,0x100,32,0);user.SendMessageW(hwnd,0x101,32,0)

                def row():
                    lines=trace.read_text(encoding='utf-8').splitlines() if trace.exists() else []
                    if not lines:return {}
                    values=lines[-1].split('\t')
                    if len(values)==len(COLUMNS):return dict(zip(COLUMNS,map(float,values)))
                    return {'step':int(values[0]),'frame':int(values[1]),'saved':int(values[5]) if len(values)>5 else 0}

                def wait(check,limit=20):
                    deadline=time.monotonic()+limit
                    while time.monotonic()<deadline:
                        if process.poll() is not None:raise AssertionError(f'Native process exited: {process.returncode}')
                        current=row()
                        if current and check(current):return current
                        time.sleep(.08)
                    raise AssertionError(f'Native state did not match: {row()}')

                def shot(name):
                    picture=capture_from_game(hwnd,capture);picture.save(out/f'{prefix}-{name}.png')
                    assert not np.any(np.all(np.asarray(picture)==[255,0,255],axis=2)),name

                time.sleep(11);click(161,664);time.sleep(1.2);click(578,664)
                wait(lambda s:s['step']==101 and s['frame']==441);shot('map-entry');click(425,300)
                initial=wait(lambda s:s['step']==27)
                # The first trace is after update 1, including the original
                # Passive Income I payment (5 gold and int(3.5) mana).
                assert initial['enemyDeads']==1 and initial['population']==9 and initial['gold']==705 and initial['mana']==203,initial
                # Select the third starting Miner before the first formation
                # has moved; keep one praying while the new workers mine.
                click(100,380)
                send(0x204,500,320,2);send(0x205,500,320)
                space();wait(lambda s:s.get('selectedMilitary')==1)
                shot('entry')
                click(350,660);wait(lambda s:s.get('garrisoned')==4);shot('miners-garrisoned')
                click(490,660);wait(lambda s:s.get('garrisoned')==0)
                click(310,600);time.sleep(2.6);current=row();assert current['camera']<=201,current
                click(550-current['camera'],300);click(675,606);wait(lambda s:s.get('research',-1)>=0)
                click(30,610);click(30,610);click(554,582);wait(lambda s:s.get('fast')==1)
                next_army=next_cast=previous=0;last_tick=-1;cure=False;seen=set()
                deadline=time.monotonic()+650
                while time.monotonic()<deadline:
                    current=row()
                    if current['step'] in (28,29):break
                    tick=int(current['tick'])
                    if tick==last_tick:time.sleep(.08);continue
                    last_tick=tick
                    if not cure and current['alliedHealers']>0:
                        space();click(742,606);send(0x204,425,300,2);send(0x205,425,300);cure=True
                    if tick>=next_army:
                        next_army=tick+120
                        click(500 if current['alliedWizards']>0 or current['alliedGiants']>=2 else 425,634)
                        if current['alliedWizards']>0:space();time.sleep(.02);space()
                    gold,mana=current['gold'],current['mana']
                    swords=current['fighters']-current['alliedHealers']-current['alliedWizards']-current['alliedGiants']-current['archers']
                    # Buy only one order per fresh trace sample, so subsequent
                    # choices observe the actual post-purchase resource totals.
                    if current['miners']+current['bankQ']<10 and current['bankQ']<5 and gold>=150:click(30,610)
                    elif current['alliedHealers']+current['templeQ']<2 and gold>=300 and mana>=200:click(200,610)
                    elif current['alliedWizards']+current['magicQ']<2 and gold>=500 and mana>=500:click(260,610)
                    elif swords+current['barracksQ']<6 and gold>=100:click(88,610)
                    elif current['miners']>=10 and gold>=1500 and current['alliedGiants']+current['siegeQ']<2:click(230,665)
                    if tick>=next_cast and current['alliedWizards']>0:
                        next_cast=tick+15
                        x=(current['deadX'] if current['deadX']>=0 else 4149)-current['camera']
                        y=min(550,320+(current['deadY'] if current['deadX']>=0 else 125))
                        if -180<x<1030:
                            space()
                            for button in (675,742,812):
                                click(button,606);click(button,606);click(x,y)
                    for key,name in (('deadsHint','poison-hint'),('poisonHits','poison-impact'),('nuke','nuke'),('cures','cure')):
                        if name not in seen and current[key]>(-1 if key=='deadsHint' else 0):seen.add(name);shot(name)
                    if tick>=previous+1800:
                        previous=tick;print(f'{prefix}: tick={tick}, mages={current["alliedWizards"]}, spells={[current[k] for k in ("nuke","electric","poison")]}, statues={current["playerStatue"]}/{current["enemyStatue"]}',flush=True)
                    time.sleep(.08)
                assert current['step']==28,current
                assert current['gutsShots']>0 and current['poisonHits']>0 and current['cures']>0 and current['poison']>0,current
                wait(lambda s:s.get('results')==1);time.sleep(.4);shot('victory')
                click(580,653);wait(lambda s:s['step']==100);shot('upgrades')
                click(750,664);time.sleep(.3);click(353,381)
                wait(lambda s:s['step']==101 and s['frame']==510);shot('crawlers-map')
                click(309,644);wait(lambda s:s['step']==101 and s['saved']==1)
                record=save.read_text(encoding='utf-8').splitlines();assert record[1].split()[:2]==['9','3'],record[:2]
                report=dict(resolution=[width,height],victory=True,victoryTick=int(current['tick']),
                    gutsShots=int(current['gutsShots']),poisonHits=int(current['poisonHits']),cures=int(current['cures']),
                    spells=[int(current[k]) for k in ('nuke','electric','poison')],minersOnlyGarrison=True,
                    savedLevel=9,nextMapFrame=510,magentaPixels=0)
                (out/f'{prefix}-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
                user.PostMessageW(hwnd,0x10,0,0);assert process.wait(timeout=8)==0
                print(report,flush=True)
            finally:
                if process.poll() is None:process.terminate()
                log=exe.parent/'lastest.log'
                if log.exists():(out/f'{prefix}-lastest.log').write_bytes(log.read_bytes())


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('width',type=int);parser.add_argument('height',type=int)
    args=parser.parse_args();run(args.width,args.height)
