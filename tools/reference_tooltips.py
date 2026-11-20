"""Compare prepared original TipBox components in Adobe Player with extracted art."""
import argparse
import ctypes as ct
from ctypes import wintypes as wt
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import json
from pathlib import Path
import subprocess
import threading
import time
from urllib.parse import urlsplit,parse_qs
import zlib
import numpy as np
from PIL import Image
from reference_intermission import diagnostic_copy
from reference_projector import root,user,window


def run(width,height):
    fixture=diagnostic_copy(Path(__file__).parent/'reference/TooltipObserver.as')
    pending='';payload=None;received=threading.Event()
    out=root/'evidence/adobe-tooltips';out.mkdir(parents=True,exist_ok=True)
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def reply(self,body=b'',kind='text/plain'):
            self.send_response(200);self.send_header('Content-Type',kind);self.send_header('Content-Length',str(len(body)))
            self.end_headers();self.wfile.write(body)
        def do_GET(self):
            nonlocal pending
            path=urlsplit(self.path).path
            if path=='/fixture.swf':self.reply(fixture,'application/x-shockwave-flash')
            elif path=='/next':
                command,pending=pending,'';self.reply(command.encode())
            else:self.send_error(404)
        def do_POST(self):
            nonlocal payload
            count=int(self.headers['Content-Length']);assert count<=width*height*4+4096
            payload=(self.path,self.rfile.read(count));self.reply();received.set()
    with ThreadingHTTPServer(('127.0.0.1',0),Handler) as server:
        threading.Thread(target=server.serve_forever,daemon=True).start()
        try:
            with subprocess.Popen([root/'work/tools/flashplayer-sa.exe',f'http://127.0.0.1:{server.server_port}/fixture.swf']) as player:
                try:
                    time.sleep(12);hwnd=next(h for h,c in window(player.pid) if c=='ShockwaveFlash')
                    user.SetMenu(hwnd,None);style=user.GetWindowLongW(hwnd,-16)
                    user.SetWindowLongW(hwnd,-16,(style&~0x00cf0000)|0x80000000);user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                    deadline=time.monotonic()+5;r=wt.RECT()
                    while time.monotonic()<deadline:
                        user.GetClientRect(hwnd,ct.byref(r))
                        if (r.right,r.bottom)==(width,height):break
                        time.sleep(.05)
                    assert (r.right,r.bottom)==(width,height),(r.right,r.bottom)
                    reports=[]
                    for index,name,title in ((1,'miner','Miner'),(5,'ninja','Shadowrath'),(9,'castle','Castle Archer')):
                        received.clear();pending=f'{name},{width},{height}';assert received.wait(15),name
                        path,data=payload;url=urlsplit(path)
                        assert url.path=='/frame/'+name,(path,data[:200])
                        assert parse_qs(url.query)=={'title':[title]},path
                        actual=Image.frombytes('RGBA',(width,height),zlib.decompress(data),'raw','ARGB').convert('RGB')
                        actual.save(out/f'{width}x{height}-{name}.png')
                        art=Image.open(root/f'assets/tooltips/1119-{20000+index}.png').convert('RGBA')
                        s=height/700;expected=Image.new('RGB',(width,height),(102,102,102))
                        scaled=art.resize((round(art.width*s/1.5),round(art.height*s/1.5)),Image.Resampling.BILINEAR)
                        expected.paste(scaled,(round(1.65*s),round(370.6*s)),scaled)
                        a=np.asarray(actual).astype(float);b=np.asarray(expected).astype(float)
                        roi=(np.abs(a-102).max(axis=2)>2)|(np.abs(b-102).max(axis=2)>2)
                        error=float(np.abs(a-b)[roi].mean());assert error<12,(name,error)
                        reports.append(dict(name=name,sourceComponent='TipBox/toolBoxMc:1119',meanPixelError=round(error,3)))
                    (out/f'{width}x{height}-checks.json').write_text(json.dumps(reports,indent=2)+'\n',encoding='utf-8',newline='\n')
                    print(reports,flush=True)
                finally:player.terminate();player.wait(timeout=8)
        finally:server.shutdown()


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('width',type=int);p.add_argument('height',type=int)
    a=p.parse_args();run(a.width,a.height)
