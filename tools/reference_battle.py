"""Capture the original tutorial in Adobe Player using real window input and its game timer."""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import socket
import socketserver
import struct
import subprocess
import threading
import time
import zlib

from PIL import Image
from reference_intermission import diagnostic_copy
from reference_projector import root, user, window


class BattleReadback:
    def __init__(self,width,height,reinforce=False,archidon=False):
        self.width,self.height=width,height
        self.state={}
        self.frame=None
        self.connection=None
        self.condition=threading.Condition()
        self.error=""
        self.evidence={}
        self.out=root/"evidence/adobe-tutorial"
        if reinforce: self.out=self.out/"reinforced"
        if archidon: self.out=root/"evidence/adobe-archidon"
        self.out.mkdir(parents=True,exist_ok=True)

    def receive(self,header,data):
        fields=header.split("|")
        with self.condition:
            if fields[0]=="error": self.error=header
            elif len(fields)==8:
                self.state=dict(page=fields[0],frame=int(fields[1]),camera=int(fields[2]),
                                selected=int(fields[3]),step=fields[4],message=fields[5])
                if fields[6]:
                    keys=(("level","points") if fields[0]=="postGame" else
                          ("gold","mana","population","playerStatue","enemyStatue"))
                    self.state.update(zip(keys,map(float,fields[6].split(","))))
                if fields[7]:
                    keys=("team","type","x","y","health","width","height","animationFrame","bowAngle")
                    actors=[]
                    for actor in fields[7].rstrip(";").split(";"):
                        values=actor.split(",");entry={key:(float(value) if key in ("width","height") else int(value)) for key,value in zip(keys,values[:9])}
                        if len(values)>9:entry["pose"]=values[9]
                        actors.append(entry)
                    self.state["units"]=actors
                if data: self.frame=(self.state.copy(),data)
            self.condition.notify_all()

    def wait(self,condition,limit=20):
        with self.condition:
            if not self.condition.wait_for(lambda:self.error or condition(),limit) or self.error:
                raise RuntimeError(f"Adobe state did not match: {self.error or self.state}")

    def until(self,check,limit=20):
        self.wait(lambda:self.state and check(self.state),limit)

    def shot(self,name,check):
        self.until(check)
        with self.condition:
            self.frame=None
            self.connection.sendall(b"S")
            self.wait(lambda:self.frame is not None,6)
            state,pixels=self.frame
        if not check(state): raise RuntimeError(f"State changed while capturing {name}: {state}")
        image=Image.frombytes("RGBA",(self.width,self.height),zlib.decompress(pixels),"raw","ARGB").convert("RGB")
        prefix=f"{self.width}x{self.height}"
        image.save(self.out/f"{prefix}-{name}.png")
        self.evidence[name]=state
        (self.out/f"{prefix}-states.json").write_text(json.dumps(self.evidence,indent=2)+"\n")
        print(name,state,flush=True)


def run(width,height,battle=False,reinforce=False,archidon=False,shadow=False,magic=False,rebels=False,giants=False,bomber=False,knight=False):
    fixture=diagnostic_copy(Path(__file__).parent/"reference/BattleObserver.as")
    feed=BattleReadback(width,height,reinforce,archidon)
    if shadow:
        feed.out=root/"evidence/adobe-shadow"
        feed.out.mkdir(exist_ok=True)
    if magic:
        feed.out=root/"evidence/adobe-magic"
        feed.out.mkdir(exist_ok=True)
    if rebels:
        feed.out=root/"evidence/adobe-rebels"
        feed.out.mkdir(exist_ok=True)
    if giants:
        feed.out=root/"evidence/adobe-giants/prepared-death"
        feed.out.mkdir(exist_ok=True,parents=True)
    if bomber:
        feed.out=root/"evidence/adobe-bomber"
        feed.out.mkdir(exist_ok=True,parents=True)
    if knight:
        feed.out=root/"evidence/adobe-knight"
        feed.out.mkdir(exist_ok=True,parents=True)
    class Stream(socketserver.StreamRequestHandler):
        def handle(self):
            if self.request.recv(1,socket.MSG_PEEK)==b"<":
                if self.rfile.read(len(b"<policy-file-request/>\0"))!=b"<policy-file-request/>\0": return
                port=self.server.server_address[1]
                self.request.sendall(f'<cross-domain-policy><allow-access-from domain="127.0.0.1" to-ports="{port}"/></cross-domain-policy>\0'.encode())
                return
            try:
                while True:
                    size=self.rfile.read(2)
                    if len(size)!=2: return
                    header=self.rfile.read(struct.unpack(">H",size)[0]).decode()
                    count=struct.unpack(">I",self.rfile.read(4))[0]
                    if count>width*height*4+4096: raise ValueError("Invalid reference frame length")
                    if header=="ready": feed.connection=self.request
                    feed.receive(header,self.rfile.read(count))
            except (ConnectionResetError,ConnectionAbortedError): pass
    class Http(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def do_GET(self):
            if self.path.split("?")[0]!="/battle.swf": return self.send_error(404)
            self.send_response(200); self.send_header("Content-Type","application/x-shockwave-flash")
            self.send_header("Content-Length",len(fixture)); self.end_headers(); self.wfile.write(fixture)
    with socketserver.ThreadingTCPServer(("127.0.0.1",0),Stream) as tcp, ThreadingHTTPServer(("127.0.0.1",0),Http) as http:
        tcp.daemon_threads=True
        for server in (tcp,http): threading.Thread(target=server.serve_forever,daemon=True).start()
        url=f"http://127.0.0.1:{http.server_port}/battle.swf?tcp={tcp.server_address[1]}&w={width}&h={height}"
        try:
            with subprocess.Popen([root/"work/tools/flashplayer-sa.exe",url]) as player:
                try:
                    feed.until(lambda s:s["page"]=="mainMenu")
                    time.sleep(9)
                    hwnd=next(h for h,c in window(player.pid) if c=="ShockwaveFlash")
                    user.SetMenu(hwnd,None)
                    user.SetWindowLongW(hwnd,-16,(user.GetWindowLongW(hwnd,-16)&~0x00cf0000)|0x80000000)
                    user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                    actual=wt.RECT(); user.GetClientRect(hwnd,ct.byref(actual))
                    assert (actual.right,actual.bottom)==(width,height)
                    def send(code,x,y,b=0):
                        p=wt.POINT(round(x*height/700),round(y*height/700))
                        target=user.ChildWindowFromPointEx(hwnd,p,7) or hwnd
                        user.MapWindowPoints(hwnd,target,ct.byref(p),1)
                        user.PostMessageW(target,code,b,(p.y<<16)|(p.x&65535))
                    def click(x,y,right=False):
                        send(0x200,x,y); time.sleep(.1)
                        send(0x204 if right else 0x201,x,y,2 if right else 1); time.sleep(.14)
                        send(0x205 if right else 0x202,x,y)
                    def visible(step): return lambda s:s["page"]=="campaignGameScreen" and s["step"]==f"Step #{step}"
                    def settled(name,step,delay=2.5):
                        feed.until(visible(step),45); time.sleep(delay); feed.shot(name,visible(step))
                    if archidon or shadow or magic or rebels or giants or bomber or knight:
                        feed.connection.sendall(b"J" if knight else b"H" if bomber else b"F" if giants else b"E" if rebels else b"D" if magic else b"C" if shadow else b"B")
                        active=lambda s:s["page"]=="campaignGameScreen"
                        feed.until(active,30)
                        send(0x200,425,225)
                        feed.shot("entry",active)
                        if knight:
                            time.sleep(2);feed.shot("deployment",active)
                            feed.connection.sendall(b"K");time.sleep(1)
                            feed.shot("prepared-space-hint",lambda s:active(s) and "SPACE" in s["message"])
                            time.sleep(5);feed.shot("hint-expired",lambda s:active(s) and "SPACE" not in s["message"])
                            return
                        if bomber:
                            assert sum(u["type"]==14 for u in feed.evidence["entry"]["units"])==6,feed.evidence["entry"]
                            feed.connection.sendall(b"I")
                            time.sleep(.7);feed.shot("prepared-throw",active)
                            time.sleep(.5);feed.shot("prepared-rock",active)
                            time.sleep(1);feed.shot("prepared-impact",active)
                            time.sleep(2);feed.shot("prepared-bombers",active)
                            return
                        if giants:
                            time.sleep(3); feed.shot("home",active)
                            feed.connection.sendall(b"G")
                            feed.until(lambda s:active(s) and s["camera"]>3900,15)
                            start=feed.state["frame"]
                            for name,offset in (("medusa-1",45),("medusa-2",220),("medusa-3",560),
                                ("rebels-enter",900),("rebels-1",1030),("rebels-2",1250),
                                ("rebels-3",1530),("rebels-4",1790)):
                                feed.until(lambda s:active(s) and s["frame"]>=start+offset,25)
                                feed.shot(name,active)
                            feed.until(lambda s:s["page"]=="postGame",25)
                            feed.shot("giant-unlocked",lambda s:s["page"]=="postGame")
                            return
                        time.sleep(3)
                        feed.shot("home",active)
                        click(88,610); click(117,665) if magic else click(146,610) if shadow else click(56,665)
                        time.sleep(17)
                        feed.shot("training",active)
                        click(500,634); click(425,600)
                        time.sleep(18)
                        feed.shot("center",active)
                        time.sleep(12)
                        feed.shot("archers",active)
                        return
                    for x,y,delay in [(161,664,1.5),(437,664,1.5),(154,668,3),(434,639,3)]:
                        click(x,y); time.sleep(delay)
                    send(0x200,425,225)
                    settled("select",1)
                    send(0x200,580,260); time.sleep(.15); send(0x201,580,260,1); time.sleep(.25)
                    send(0x200,720,510,1); feed.until(lambda s:s["selected"]==2)
                    send(0x202,720,510); settled("move",2,4)
                    click(150,420,True); settled("scroll",3,4)
                    send(0x200,845,165); feed.until(visible(4),35)
                    send(0x200,425,225); settled("attack",4)
                    click(550,430,True); settled("return",5)
                    click(350,600); settled("miner",6)
                    if battle or reinforce:
                        camera=lambda:feed.state["camera"]
                        def message(text,limit=30):
                            match=lambda s:s["page"]=="campaignGameScreen" and text in s["message"]
                            feed.until(match,limit)
                            return match
                        click(1250-camera(),430); settled("pray",7)
                        click(1350-camera(),320,True); settled("mine-gold",8)
                        click(1590-camera(),490,True); settled("train",9)
                        click(95,610); arriving=message("A Spearton is attacking!",40)
                        feed.shot("spearton",arriving); settled("garrison",10)
                        click(350,634); settled("range",11)
                        click(532-camera(),170); settled("castle-upgrade",12)
                        click(675,606); settled("defend",13)
                        click(425,637); settled("defence-battle",14)
                        objective=message("Your objective is to destroy",70)
                        time.sleep(2); feed.shot("objective",objective)
                        click(26,610); click(88,610); click(88,610); click(110,595)
                        time.sleep(14); click(88,610); click(500,634); click(475,600)
                        feed.until(lambda s:0<s.get("enemyStatue",400)<300,150)
                        feed.shot("reinforcement",lambda s:0<s.get("enemyStatue",400)<300)
                        if reinforce:
                            # Keep the original single-wave reference separate. New
                            # trainees defend at home until the player orders an attack.
                            orders=[]
                            start=feed.state["frame"]
                            while feed.state["page"]=="campaignGameScreen" and feed.state["frame"]<start+7200:
                                state=feed.state.copy()
                                count=min(3,int(state["gold"])//150)
                                for _ in range(count): click(88,610)
                                click(500,634)
                                orders.append(dict(frame=state["frame"],gold=state["gold"],swordwrathOrders=count,
                                                   enemyStatue=state["enemyStatue"],population=state["population"]))
                                (feed.out/f"{width}x{height}-orders.json").write_text(json.dumps(orders,indent=2)+"\n")
                                feed.until(lambda s:s["page"]!="campaignGameScreen" or s["frame"]>=state["frame"]+390,30)
                        feed.until(lambda s:s["page"]=="postGame",130)
                        won=lambda s:s["page"]=="postGame" and s.get("level")==1 and s.get("points")==1
                        time.sleep(2); feed.shot("spearton-unlocked",won)
                        click(425,490); time.sleep(1.2); feed.shot("victory",won)
                finally: player.terminate()
        finally:
            tcp.shutdown(); http.shutdown()


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("width",type=int); parser.add_argument("height",type=int)
    parser.add_argument("--battle",action="store_true")
    parser.add_argument("--reinforce",action="store_true",help="recruit additional Swordwraths and order attacks after the enemy reinforcement")
    parser.add_argument("--archidon",action="store_true")
    parser.add_argument("--shadow",action="store_true")
    parser.add_argument("--magic",action="store_true")
    parser.add_argument("--rebels",action="store_true")
    parser.add_argument("--giants",action="store_true",help="prepare Giant death to inspect the original campaign cinematic")
    parser.add_argument("--bomber",action="store_true",help="record CampaignBomber entry and a prepared original Giant throw")
    parser.add_argument("--knight",action="store_true",help="record Juggerknight deployment and prepared campaign instruction")
    args=parser.parse_args(); run(args.width,args.height,args.battle,args.reinforce,args.archidon,args.shadow,args.magic,args.rebels,args.giants,args.bomber,args.knight)
