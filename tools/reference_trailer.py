"""Extract authored trailer frames in Adobe Player, with its original scripts."""
import argparse
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
from inventory import Reader


def silent_capture_copy(source):
    original=source.read_bytes();body=zlib.decompress(original[8:])
    reader=Reader(body);reader.rect();reader.uint(2);reader.uint(2)
    pos=reader.bit//8;output=bytearray(body[:pos]);removed=0
    while pos<len(body):
        start=pos;word=struct.unpack_from('<H',body,pos)[0];pos+=2;code,size=word>>6,word&63
        if size==63:size=struct.unpack_from('<I',body,pos)[0];pos+=4
        pos+=size
        if code in (18,19,45):removed+=1
        else:output.extend(body[start:pos])
    assert removed==3085,removed
    return b'CWS'+original[3:4]+struct.pack('<I',len(output)+8)+zlib.compress(output,5)


def run(wide=False,all_frames=False,out_override=None,selected=None,screenfx=False):
    base=root/'work/trailer-source'
    out=base/('frames-wide' if wide else 'frames-original')
    if not all_frames:out=base/('reference-wide' if wide else 'reference-original')
    if out_override is not None:out=Path(out_override).resolve()
    out.mkdir(parents=True,exist_ok=True)
    if selected is not None:
        assert not all_frames and selected==sorted(set(selected)) and all(3<=f<=3235 for f in selected)
    width,height=(1280,720) if wide else (720,480)
    source=base/'source.swf'
    if all_frames:
        source=base/'silent-capture.swf';source.write_bytes(silent_capture_copy(base/'source.swf'))
    fixture=diagnostic_copy(root/'tools/reference/TrailerObserver.as',source)
    condition=threading.Condition();state={'ready':False,'done':False,'rows':[],'beforeRows':[],'connection':None,'error':None}

    class Stream(socketserver.StreamRequestHandler):
        def handle(self):
            if self.request.recv(1,socket.MSG_PEEK)==b'<':
                self.rfile.read(len(b'<policy-file-request/>\0'))
                self.request.sendall(f'<cross-domain-policy><allow-access-from domain="127.0.0.1" to-ports="{self.server.server_address[1]}"/></cross-domain-policy>\0'.encode())
                return
            try:
                while True:
                    data=self.rfile.read(2)
                    if not data:return
                    header=self.rfile.read(struct.unpack('>H',data)[0]).decode()
                    count=struct.unpack('>I',self.rfile.read(4))[0]
                    assert count<=width*height*4+4096
                    pixels=self.rfile.read(count)
                    with condition:
                        state['connection']=self.request
                        if header in ('ready','done'):state[header]=True
                        elif header.startswith('tick|'):print(header,flush=True)
                        elif header.startswith('{'):
                            row=json.loads(header)
                            assert row['width']==width and row['height']==height
                            destination=out/'before' if row.get('variant')=='before' else out
                            destination.mkdir(exist_ok=True)
                            Image.frombytes('RGBA',(width,height),zlib.decompress(pixels),'raw','ARGB').convert('RGB').save(destination/f'{row["frame"]:04}.png')
                            state['beforeRows' if row.get('variant')=='before' else 'rows'].append(row)
                            print('Frame',row['frame'],'camera',row['camera'],'variant',row.get('variant','source'),flush=True)
                        else:state['error']=header
                        condition.notify_all()
            except Exception as error:
                with condition:state['error']=repr(error);condition.notify_all()

    class Http(BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def do_GET(self):
            self.send_response(200);self.send_header('Content-Type','application/x-shockwave-flash')
            self.send_header('Content-Length',str(len(fixture)));self.end_headers();self.wfile.write(fixture)

    with socketserver.ThreadingTCPServer(('127.0.0.1',0),Stream) as tcp,ThreadingHTTPServer(('127.0.0.1',0),Http) as http:
        tcp.daemon_threads=True
        for server in (tcp,http):threading.Thread(target=server.serve_forever,daemon=True).start()
        def wait(key,timeout):
            end=time.monotonic()+timeout
            with condition:
                if state['error']:raise AssertionError(state['error'])
                while not state[key]:
                    if state['error']:raise AssertionError(state['error'])
                    remaining=end-time.monotonic()
                    if remaining<=0:raise TimeoutError((key,[r['frame'] for r in state['rows']][-5:]))
                    condition.wait(min(remaining,1))
        selected_query='' if selected is None else '&frames='+','.join(map(str,selected))
        url=f'http://127.0.0.1:{http.server_port}/trailer.swf?tcp={tcp.server_address[1]}&wide={int(wide)}&all={int(all_frames)}&screenfx={int(screenfx)}'+selected_query
        with subprocess.Popen([root/'work/tools/flashplayer-sa.exe',url]) as player:
            try:
                wait('ready',45)
                hwnd=next(h for h,c in window(player.pid) if c=='ShockwaveFlash')
                user.SetMenu(hwnd,None)
                user.SetWindowLongW(hwnd,-16,(user.GetWindowLongW(hwnd,-16)&~0xcf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                time.sleep(.3);state['connection'].sendall(b'P')
                wait('done',900 if all_frames else 180)
                frames=[r['frame'] for r in state['rows']]
                if all_frames:assert frames==list(range(3,3236)),(len(frames),frames[:5],frames[-5:])
                elif selected is not None:assert frames==selected,(frames,selected)
                else:assert len(frames)==12 and frames[0]==3 and frames[-1]==3235,frames
                (out/'frames.json').write_text(json.dumps(state['rows'],indent=2)+'\n')
                if state['beforeRows']:
                    assert [r['frame'] for r in state['beforeRows']]==frames
                    (out/'before-frames.json').write_text(json.dumps(state['beforeRows'],indent=2)+'\n')
                print('Original trailer extraction passed',len(frames),wide,flush=True)
            finally:
                (out/'diagnostic.json').write_text(json.dumps({k:v for k,v in state.items() if k!='connection'},indent=2)+'\n')
                if player.poll() is None:player.terminate()
                player.wait(timeout=10)
                for server in (tcp,http):server.shutdown()


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--wide',action='store_true');p.add_argument('--all',action='store_true')
    p.add_argument('--out',type=Path);p.add_argument('--frames',type=int,nargs='+');p.add_argument('--screenfx',action='store_true')
    a=p.parse_args();run(a.wide,a.all,a.out,a.frames,a.screenfx)
