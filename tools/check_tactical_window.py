"""Exercise prepared original-rule fixtures through the real Windows client."""
import argparse
import csv
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
EXE=Path(os.environ['DW2_TEST_EXE'])
OUT=Path(os.environ.get('DW2_TEST_EVIDENCE',ROOT/'work/tactical-window-evidence'))


def run(width,height,block_only=False):
    OUT.mkdir(parents=True,exist_ok=True);prefix=f'{width}x{height}'
    capture=OUT/f'{prefix}-capture.png'
    with tempfile.TemporaryDirectory() as temporary:
        folder=Path(temporary)
        (folder/'campaign.sav').write_text('DeadWarrior2-Campaign 1\n8 8 1 000000000000000000010\n'+'220 220 1\n'*8+'-1 0 0\n'*6)
        with subprocess.Popen([EXE,'--save-dir',folder,'--capture-client',capture,'--magenta']) as game:
            try:
                hwnd=None
                def wait(check,limit=25):
                    end=time.monotonic()+limit
                    while time.monotonic()<end:
                        if game.poll() is not None:raise AssertionError('Owned diagnostic client exited')
                        if check():return
                        time.sleep(.05)
                    raise AssertionError('Native tactical check timed out')
                def find():
                    nonlocal hwnd
                    hwnd=next((h for h,c in window(game.pid) if c=='DeadWarrior2Native'),None)
                    return bool(hwnd)
                wait(find)
                user.SetWindowLongW(hwnd,-16,(user.GetWindowLongW(hwnd,-16)&~0xcf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                def query(n,w=0):return user.SendMessageW(hwnd,0x8000+n,w,0)
                def send(msg,x,y,b=0):
                    vw=query(23)/100;vh=query(24)/100;s=min(width/vw,height/vh)
                    px=round(width/2+(x-425)*s);py=round((height-vh*s)/2+y*s)
                    user.SendMessageW(hwnd,msg,b,(py<<16)|(px&65535))
                def click(x,y):
                    for m,b in [(0x200,0),(0x201,1),(0x202,0)]:send(m,x,y,b)
                def right(x,y):send(0x204,x,y,2);send(0x205,x,y)
                def key(code):user.SendMessageW(hwnd,0x100,code,0);user.SendMessageW(hwnd,0x101,code,0)
                def units():
                    query(26)
                    return list(csv.DictReader(capture.with_suffix('.units.tsv').open(),delimiter='\t'))
                def shot(name):
                    image=capture_from_game(hwnd,capture);image.save(OUT/f'{prefix}-{name}.png')
                    assert not np.any(np.all(np.asarray(image)==[255,0,255],axis=2)),name
                wait(lambda:query(28)==1);time.sleep(1.6);click(161,664);time.sleep(1.4);click(578,664)
                # Continue opens the original campaign-map travel first.
                time.sleep(3)
                _,scale,mx,my,fx,fy=map(float,(ROOT/'assets/campaign-map-9/map-motion.tsv').read_text().splitlines()[-1].split())
                flag=next(row.split() for row in (ROOT/'assets/intermission/textures.tsv').read_text().splitlines() if row.startswith('393\t16\t'))
                bx,by,bw,bh=map(float,flag[3:]);click(mx+scale*(fx+bx+bw/2),my+scale*(fy+by+bh/2))
                wait(lambda:query(22)>0)
                if block_only:
                    assert query(33,3);click(675,606)
                    wait(lambda:units()[0]['pose']=='block' and units()[0]['frame']=='15')
                    shot('block-tooltip')
                    user.PostMessageW(hwnd,0x10,0,0);assert game.wait(timeout=10)==0
                    print('Original three-line Block tooltip captured',width,height,flush=True)
                    return
                # Direct enemy target must replace a previous Hold.
                assert query(33,1);right(500,420)
                wait(lambda:units()[0]['pose']=='attack_1');shot('giant-direct-target')
                # A Move during a live heal keeps its event and then moves.
                assert query(33,2)
                wait(lambda:units()[0]['pose']=='attack_1')
                right(700,445);shot('meric-casting')
                wait(lambda:float(units()[0]['x'])>2450)
                assert float(units()[1]['health'])>100;shot('meric-moved')
                # Original frame 15 holds; toggling out resumes movement.
                assert query(33,3);click(675,606)
                wait(lambda:units()[0]['pose']=='block' and units()[0]['frame']=='15')
                shot('block-held');time.sleep(.3);assert units()[0]['frame']=='15'
                right(700,445);wait(lambda:float(units()[0]['x'])>2450);shot('block-moved')
                assert query(33,3);click(675,606)
                wait(lambda:units()[0]['pose']=='block' and units()[0]['frame']=='15')
                click(813,678)
                wait(lambda:units()[0]['pose']=='stand')
                shot('block-hold-exits')
                # Real simulation clock captures the tower and emits both tiers.
                for fixture,tier in [(4,1),(5,2)]:
                    assert query(33,fixture)
                    wait(lambda:query(22)>=100);shot(f'tower-{tier}-progress')
                    wait(lambda:len(units())==2,22)
                    assert units()[1]['symbol']==('6456' if tier==1 else '7526')
                    key(32);assert units()[1]['selected']=='0'
                    shot(f'tower-{tier}-ghost')
                report=dict(resolution=[width,height],giantHoldReplaced=True,mericCastFinishes=True,mericMoves=True,
                            blockHoldsFrame15=True,blockMoveExits=True,blockHoldExits=True,towerCapture=True,towerSpawnI=True,towerSpawnII=True,ghostNotSelectable=True,noMagenta=True)
                (OUT/f'{prefix}-checks.json').write_text(json.dumps(report,indent=2)+'\n')
                user.PostMessageW(hwnd,0x10,0,0);assert game.wait(timeout=10)==0
                print(report,flush=True)
            finally:
                if game.poll() is None:game.terminate();game.wait(timeout=10)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('width',type=int);p.add_argument('height',type=int);p.add_argument('--block-only',action='store_true')
    args=p.parse_args();run(args.width,args.height,args.block_only)
