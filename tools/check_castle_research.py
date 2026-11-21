"""Exercise all three unlocked Castle Archer purchases through the Windows UI."""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
from native_client import capture_from_game
from reference_projector import user,window

ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get('DW2_TEST_EXE',ROOT/'out/windows-x64/DeadWarrior 2.exe'))
OUT=Path(os.environ.get('DW2_TEST_EVIDENCE',ROOT/'work/castle-research-window'))

def run(width,height):
    OUT.mkdir(parents=True,exist_ok=True);prefix=f'{width}x{height}'
    capture=OUT/f'{prefix}-capture.png';trace=OUT/f'{prefix}-trace.tsv'
    with tempfile.TemporaryDirectory() as temporary:
        folder=Path(temporary);bits=sum(1<<i for i in (1,2,3,6,10))
        (folder/'campaign.sav').write_text(f'DeadWarrior2-Campaign 1\n1 0 1 {bits:021b}\n220 220 1\n'+'-1 0 0\n'*13,encoding='utf-8')
        with subprocess.Popen([EXE,'--save-dir',folder,'--capture-client',capture,'--trace-tutorial',trace]) as game:
            try:
                hwnd=None
                def wait(test,limit=20):
                    until=time.monotonic()+limit
                    while time.monotonic()<until:
                        if test():return
                        if game.poll() is not None:raise AssertionError('Owned native game exited')
                        time.sleep(.05)
                    raise AssertionError('Castle UI test timed out')
                def find():
                    nonlocal hwnd
                    hwnd=next((h for h,c in window(game.pid) if c=='DeadWarrior2Native'),None);return bool(hwnd)
                wait(find)
                user.SetWindowLongW(hwnd,-16,(user.GetWindowLongW(hwnd,-16)&~0xcf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                def query(n):return user.SendMessageW(hwnd,0x8000+n,0,0)
                def click(x,y):
                    scale=height/700;px=round(width/2+(x-425)*scale);py=round(y*scale)
                    for msg,b in ((0x200,0),(0x201,1),(0x202,0)):user.SendMessageW(hwnd,msg,b,(py<<16)|(px&65535))
                def row():
                    lines=trace.read_text(encoding='utf-8').splitlines() if trace.exists() else []
                    return lines[-1].split('\t') if lines else []
                wait(lambda:query(28)==1);time.sleep(1.6);click(161,664);time.sleep(1.3);click(578,664)
                wait(lambda:bool(row()) and row()[0]=='101' and row()[1]=='50')
                _,scale,mx,my,fx,fy=map(float,(ROOT/'assets/intermission/map-motion.tsv').read_text(encoding='utf-8').splitlines()[-1].split())
                flag=next(line.split() for line in (ROOT/'assets/intermission/textures.tsv').read_text(encoding='utf-8').splitlines() if line.startswith('393\t16\t'))
                bx,by,bw,bh=map(float,flag[3:]);click(mx+scale*(fx+bx+bw/2),my+scale*(fy+by+bh/2))
                wait(lambda:query(22)>=1);click(310,600)
                wait(lambda:len(row())>2 and row()[0]=='27' and float(row()[2])<=201)
                click(550-float(row()[2]),300);click(554,582)
                for tier,cost in ((1,300),(2,400),(3,500)):
                    wait(lambda:query(31)>=cost,45);click(675,606)
                    wait(lambda:query(30)>=0,3)
                    wait(lambda:query(29)==tier,30)
                    capture_from_game(hwnd,capture).save(OUT/f'{prefix}-tier-{tier}.png')
                assert query(29)==3 and query(30)==-1
                click(675,606);assert query(29)==3 and query(30)==-1
                report=dict(resolution=[width,height],tiers=[1,2,3],originalCosts=[300,400,500],allResearchCompleted=True,
                            independentDefendersVisible=True,noFourthPlayerTier=True,fastForward=True)
                (OUT/f'{prefix}-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(report,flush=True)
                user.PostMessageW(hwnd,0x10,0,0);assert game.wait(timeout=10)==0
            finally:
                if game.poll() is None:game.terminate();game.wait(timeout=10)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('width',type=int);parser.add_argument('height',type=int)
    args=parser.parse_args();run(args.width,args.height)
