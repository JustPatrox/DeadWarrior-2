"""Extract castle states using the original Adobe/Util timeline controller."""
import argparse
import csv
import hashlib
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import json
from math import ceil,floor
from pathlib import Path
import struct
import subprocess
import threading
import time
from urllib.parse import urlsplit
import zlib
import jpype
from PIL import Image
from reference_intermission import diagnostic_copy
from build_menu_assets import ROOT

BUTTONS={1715:'magicShopMc',1753:'templeMc',1766:'bankMc',1808:'siegeShop',1831:'barracksMc',1865:'archeryRangeMc'}


def build(inspect=False,reference=False,details=False,component=False):
    fixture=diagnostic_copy(Path(__file__).parent/'reference/CastleObserver.as')
    J=jpype.JClass
    with J('java.io.FileInputStream')(str(ROOT/'stickwar2.swf')) as stream:
        swf=J('com.jpexs.decompiler.flash.SWF')(stream,False)
    pending='';payload=None;received=threading.Event()
    folder=ROOT/'assets/castle';folder.mkdir(exist_ok=True)
    rows=[];animations=[];files={};report=[];base=300000
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
            count=int(self.headers['Content-Length'])
            if count>128*1024*1024:return self.send_error(413)
            payload=(self.path,self.rfile.read(count));self.reply();received.set()
    with ThreadingHTTPServer(('127.0.0.1',0),Handler) as server:
        threading.Thread(target=server.serve_forever,daemon=True).start()
        try:
            with subprocess.Popen([ROOT/'work/tools/flashplayer-sa.exe',f'http://127.0.0.1:{server.server_port}/fixture.swf']) as player:
                try:
                    time.sleep(12)
                    if details:
                        for name in BUTTONS.values():
                            received.clear();pending=f'details,{name},2'
                            if not received.wait(30):raise TimeoutError(name)
                            print(name,payload[0],payload[1].decode(),flush=True)
                        return
                    if component:
                        output=ROOT/'evidence/adobe-castle-components';output.mkdir(parents=True,exist_ok=True)
                        for name in BUTTONS.values():
                            received.clear();pending=f'component,{name},60'
                            if not received.wait(30):raise TimeoutError(name)
                            path,data=payload
                            if path!='/snapshot':raise ValueError((path,data[:1000]))
                            x,y,w,h=struct.unpack_from('>iiii',data)
                            Image.frombytes('RGBA',(w,h),zlib.decompress(data[16:]),'raw','ARGB').save(output/f'{name}.png')
                        return
                    received.clear();pending='blend'
                    if not received.wait(30):raise TimeoutError('Original blend samples')
                    path,data=payload
                    if path!='/blend':raise ValueError((path,data[:1000]))
                    samples=[dict(zip(('mode','source','destination','result'),line.split(','))) for line in data.decode().splitlines()]
                    (ROOT/'work/flash-blend-samples.json').write_text(json.dumps(samples,indent=2)+'\n',encoding='utf-8',newline='\n')
                    received.clear();pending='base,neutral,0'
                    if not received.wait(30):raise TimeoutError('Original castle base')
                    path,data=payload
                    if path!='/snapshot':raise ValueError((path,data[:1000]))
                    bx,by,bw,bh=struct.unpack_from('>iiii',data)
                    baseImage=Image.frombytes('RGBA',(bw,bh),zlib.decompress(data[16:]),'raw','ARGB')
                    crop=baseImage.getchannel('A').getbbox();baseImage=baseImage.crop(crop)
                    baseImage.save(folder/'1866-10001.png')
                    baseRow=(1866,10001,'1866-10001.png',bx+crop[0],by+crop[1],baseImage.width,baseImage.height)
                    if reference:
                        previous=list(csv.reader((folder/'textures.tsv').open(encoding='utf-8'),delimiter='\t'))
                        with (folder/'textures.tsv').open('w',encoding='utf-8',newline='') as f:csv.writer(f,delimiter='\t',lineterminator='\n').writerows([baseRow,*[r for r in previous if r[0]!='1866']])
                    else:rows.append(baseRow)
                    if reference:
                        output=ROOT/'evidence/adobe-castle';output.mkdir(parents=True,exist_ok=True);snapshots=[]
                        for name in ('neutral',*BUTTONS.values()):
                            for tick in (1,5,15,30,60,240,400):
                                received.clear();pending=f'snapshot,{name},{tick}'
                                if not received.wait(30):raise TimeoutError((name,tick))
                                path,data=payload
                                if path!='/snapshot':raise ValueError((path,data[:1000]))
                                x,y,w,h=struct.unpack_from('>iiii',data)
                                Image.frombytes('RGBA',(w,h),zlib.decompress(data[16:]),'raw','ARGB').save(output/f'{name}-{tick}.png')
                                snapshots.append(dict(name=name,tick=tick,x=x,y=y,width=w,height=h))
                            print('Original castle reference:',name,flush=True)
                        (output/'bounds.json').write_text(json.dumps(snapshots,indent=2)+'\n',encoding='utf-8',newline='\n')
                        return
                    for owner,name in BUTTONS.items():
                        bounds=swf.getCharacter(owner).getTimeline().getDisplayRectWithFilters()
                        x,y=floor(bounds.Xmin/20)-2,floor(bounds.Ymin/20)-2
                        w,h=ceil(bounds.Xmax/20)-x+2,ceil(bounds.Ymax/20)-y+2
                        for state in (1,2,3):
                            received.clear();pending=f'layers,{name},{state}'
                            if not received.wait(30):raise TimeoutError((name,state,'layers'))
                            path,data=payload
                            if path!='/layers':raise ValueError((path,data[:1000]))
                            lines=data.decode().splitlines()
                            layers=[line.split('|') for line in lines[1:]]
                            assert int(lines[0])==len(layers)
                            for layer,blend,label in layers:
                                received.clear();pending=f'{"inspect" if inspect else "bake"},{name},{state},{x},{y},{w},{h},{layer}'
                                if not received.wait(120):raise TimeoutError((name,state,layer))
                                path,data=payload
                                if path!='/frames':raise ValueError((path,data[:1000]))
                                prefix,period,count=struct.unpack_from('>iii',data);position=12
                                if not inspect:
                                    for i in range(count):
                                        bx,by,bw,bh,size=struct.unpack_from('>iiiiI',data,position);position+=20
                                        raw=zlib.decompress(data[position:position+size]);position+=size
                                        image=Image.frombytes('RGBA',(bw,bh),raw,'raw','ARGB')
                                        crop=image.getchannel('A').getbbox()
                                        if crop is None:bx=by=0;crop=(0,0,1,1)
                                        image=image.crop(crop)
                                        digest=hashlib.sha256(image.tobytes()+struct.pack('>ii',image.width,image.height)).digest()
                                        file=files.get(digest)
                                        if file is None:
                                            file=f'{owner}-{base+i+1}.png';files[digest]=file;image.save(folder/file)
                                        rows.append((owner,base+i+1,file,bx+crop[0],by+crop[1],image.width,image.height))
                                    assert position==len(data)
                                    animations.append((owner,state,int(layer),base,prefix,period,count,blend));base+=count
                                entry=dict(owner=owner,name=name,state=state,layer=int(layer),blend=blend,prefix=prefix,period=period,frames=count)
                                report.append(entry);print(entry,flush=True)
                    if not inspect:
                        for file,values in (('textures.tsv',rows),('states.tsv',animations)):
                            with (folder/file).open('w',encoding='utf-8',newline='') as f:csv.writer(f,delimiter='\t',lineterminator='\n').writerows(values)
                    (ROOT/('work/castle-source-inspection.json' if inspect else 'work/castle-source-extraction.json')).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
                finally:player.terminate();player.wait(timeout=8)
        finally:server.shutdown()


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--inspect',action='store_true');p.add_argument('--reference',action='store_true');p.add_argument('--details',action='store_true');p.add_argument('--component',action='store_true');a=p.parse_args();build(a.inspect,a.reference,a.details,a.component)
