"""Run the original post-victory screens in Adobe Player at a prepared campaign state."""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import struct
import subprocess
import threading
import time
from urllib.parse import urlsplit, parse_qs
import zipfile
import zlib

import jpype
from PIL import Image
from reference_projector import root, user, window

WORK=root/"work"
SOURCE=Path(__file__).parent/"reference/IntermissionObserver.as"


def swf_tag(tag,body):
    length=len(body)
    header=struct.pack("<H",tag<<6|min(length,63))
    return header+(struct.pack("<I",length) if length>=63 else b"")+body


def diagnostic_copy(source=SOURCE):
    ffdec=WORK/"tools/ffdec"
    java=next((WORK/"tools").glob("*/bin/server/jvm.dll"))
    jpype.startJVM(str(java),"-Xmx1g",classpath=list((ffdec/"lib").glob("*.jar")))
    flash="com.jpexs.decompiler.flash."
    J=jpype.JClass
    swf=J(flash+"SWF")
    with zipfile.ZipFile(ffdec/"flashlib/playerglobal32_0.swc") as z:
        api=swf(J("java.io.ByteArrayInputStream")(z.read("library.swf")),False)
    host=swf()
    abc_tag=J(flash+"tags.DoABC2Tag")(host)
    host.addTag(abc_tag)
    abc=abc_tag.getABC()
    index=J(flash+"abc.avm2.parser.script.AbcIndexing")()
    for a in api.getAbcList(): index.addAbc(a.getABC())
    index.addAbc(abc); index.selectAbc(abc)
    name=source.stem
    J(flash+"abc.avm2.parser.script.ActionScript3Parser").compile(
        source.read_text(),abc,index,source.name,0,0,False,name)
    buffer=J("java.io.ByteArrayOutputStream")()
    abc.saveToStream(J(flash+"abc.ABCOutputStream")(buffer))
    abc_bytes=bytes(buffer.toByteArray())
    original=(root/"stickwar2.swf").read_bytes()
    data=zlib.decompress(original[8:])
    pos=(5+4*(data[0]>>3)+7)//8+4
    while pos<len(data):
        code=struct.unpack_from("<H",data,pos)[0]
        length=code&63
        if code>>6==1: break
        if length==63: length=struct.unpack_from("<I",data,pos+2)[0]; pos+=4
        pos+=2+length
    else: raise ValueError("Original first ShowFrame missing")
    sprite=60000
    cname=name.encode()+b"\0"
    additions=(swf_tag(39,struct.pack("<HH",sprite,1)+swf_tag(1,b"")+swf_tag(0,b""))+
               swf_tag(82,struct.pack("<I",1)+cname+abc_bytes)+
               swf_tag(76,struct.pack("<HH",1,sprite)+cname)+swf_tag(26,struct.pack("<BHHB",6,sprite,sprite,0)))
    data=data[:pos]+additions+data[pos:]
    return b"CWS"+original[3:4]+struct.pack("<I",len(data)+8)+zlib.compress(data,5)


def capture(width,height,fixture,level=1):
    pending=""
    received=threading.Event()
    payload=None
    out=root/("evidence/adobe-map-repair" if level>1 else "evidence/adobe-intermission")
    out.mkdir(parents=True,exist_ok=True)
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def reply(self,body=b"",mime="text/plain"):
            self.send_response(200); self.send_header("Content-Type",mime)
            self.send_header("Content-Length",str(len(body))); self.send_header("Cache-Control","no-store")
            self.end_headers(); self.wfile.write(body)
        def do_GET(self):
            nonlocal pending
            path=urlsplit(self.path).path
            if path=="/fixture.swf": self.reply(fixture,"application/x-shockwave-flash")
            elif path=="/next":
                command,pending=pending,""
                self.reply(command.encode())
            else: self.send_error(404)
        def do_POST(self):
            nonlocal payload
            url=urlsplit(self.path)
            length=int(self.headers["Content-Length"])
            if length>width*height*4+4096: return self.send_error(413)
            payload=(url.path,parse_qs(url.query),self.rfile.read(length))
            self.reply(); received.set()
    server=ThreadingHTTPServer(("127.0.0.1",0),Handler)
    threading.Thread(target=server.serve_forever,daemon=True).start()
    try:
        with subprocess.Popen([WORK/"tools/flashplayer-sa.exe",f"http://127.0.0.1:{server.server_port}/fixture.swf"]) as player:
            try:
                time.sleep(12)
                hwnd=next(h for h,c in window(player.pid) if c=="ShockwaveFlash")
                user.SetMenu(hwnd,None); style=user.GetWindowLongW(hwnd,-16)
                user.SetWindowLongW(hwnd,-16,(style&~0x00cf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                client=wt.RECT(); user.GetClientRect(hwnd,ct.byref(client))
                assert (client.right,client.bottom)==(width,height)
                def send(code,x,y):
                    p=wt.POINT(round(x*height/700),round(y*height/700))
                    target=user.ChildWindowFromPointEx(hwnd,p,7) or hwnd
                    user.MapWindowPoints(hwnd,target,ct.byref(p),1)
                    user.PostMessageW(target,code,int(code==0x201),(p.y<<16)|(p.x&65535))
                def click(x,y):
                    for code in (0x200,0x201,0x202): send(code,x,y); time.sleep(.1)
                def request(label,screen="",points=0):
                    nonlocal pending
                    received.clear(); pending=f"{label},{width},{height},{level}"
                    if not received.wait(12): raise RuntimeError(f"Adobe timed out at {label}")
                    path,metadata,data=payload
                    if path=="/error": raise RuntimeError(data.decode())
                    if label=="prepare":
                        assert path=="/prepared"; return
                    assert path=="/frame/"+label and metadata=={"screen":[screen],"points":[str(points)]},(path,metadata)
                    image=Image.frombytes("RGBA",(width,height),zlib.decompress(data),"raw","ARGB").convert("RGB")
                    image.save(out/f"{width}x{height}-{label}.png")
                    print(label,metadata,flush=True)
                request("prepare"); time.sleep(2)
                if level>1:
                    time.sleep(3);request("map-level-"+str(level),"campaignMap",0);return
                upgrade="campaignUpgradeScreen"
                send(0x200,425,493); time.sleep(.4); request("upgrades",upgrade,1)
                click(750,664); time.sleep(.4); request("confirm-upgrades",upgrade,1)
                click(510,381); click(425,493); time.sleep(.4); request("rage",upgrade,0)
                click(750,664); time.sleep(4); request("archidon-map","campaignMap",0)
            finally: player.terminate()
    finally:
        server.shutdown(); server.server_close()


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("width",type=int); parser.add_argument("height",type=int)
    parser.add_argument("--map-level",type=int,default=1,choices=range(1,14))
    args=parser.parse_args()
    capture(args.width,args.height,diagnostic_copy(),args.map_level)
