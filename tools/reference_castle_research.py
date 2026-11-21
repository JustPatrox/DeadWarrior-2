"""Measure original campaign Castle Archer research and three defender positions."""
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
from reference_battle import BattleReadback
from reference_intermission import diagnostic_copy
from reference_projector import root, user, window


class Readback(BattleReadback):
    def receive(self, header, data):
        with self.condition:
            if header.startswith('error|'):
                self.error = header
            elif header.startswith('{'):
                self.state = json.loads(header)
                if data:
                    self.frame = (self.state.copy(), data)
            self.condition.notify_all()


def run(width, height):
    fixture = diagnostic_copy(Path(__file__).parent/'reference/CastleResearchObserver.as')
    feed = Readback(width, height)
    feed.out = root/'evidence/adobe-castle-research'
    feed.out.mkdir(parents=True, exist_ok=True)

    class Stream(socketserver.StreamRequestHandler):
        def handle(self):
            if self.request.recv(1, socket.MSG_PEEK) == b'<':
                self.rfile.read(len(b'<policy-file-request/>\0'))
                self.request.sendall(f'<cross-domain-policy><allow-access-from domain="127.0.0.1" to-ports="{self.server.server_address[1]}"/></cross-domain-policy>\0'.encode())
                return
            try:
                while True:
                    size = self.rfile.read(2)
                    if not size:
                        return
                    header = self.rfile.read(struct.unpack('>H', size)[0]).decode()
                    count = struct.unpack('>I', self.rfile.read(4))[0]
                    if count > width*height*4+4096:
                        raise ValueError('Invalid original reference payload')
                    if header == 'ready':
                        feed.connection = self.request
                    feed.receive(header, self.rfile.read(count))
            except (ConnectionResetError, ConnectionAbortedError):
                pass

    class Http(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            self.send_response(200)
            self.send_header('Content-Type', 'application/x-shockwave-flash')
            self.send_header('Content-Length', len(fixture))
            self.end_headers()
            self.wfile.write(fixture)

    with socketserver.ThreadingTCPServer(('127.0.0.1', 0), Stream) as tcp, ThreadingHTTPServer(('127.0.0.1', 0), Http) as http:
        tcp.daemon_threads = True
        for server in (tcp, http):
            threading.Thread(target=server.serve_forever, daemon=True).start()
        url = f'http://127.0.0.1:{http.server_port}/fixture.swf?tcp={tcp.server_address[1]}&w={width}&h={height}'
        with subprocess.Popen([root/'work/tools/flashplayer-sa.exe', url]) as player:
            try:
                feed.until(lambda s:s['page']=='mainMenu', 40)
                time.sleep(9)
                hwnd = next(h for h,c in window(player.pid) if c=='ShockwaveFlash')
                user.SetMenu(hwnd, None)
                user.SetWindowLongW(hwnd,-16,(user.GetWindowLongW(hwnd,-16)&~0xcf0000)|0x80000000)
                user.SetWindowPos(hwnd,0,0,0,width,height,0x34)
                actual=wt.RECT();user.GetClientRect(hwnd,ct.byref(actual))
                assert (actual.right,actual.bottom)==(width,height)
                active=lambda s:s['page']=='campaignGameScreen'
                feed.connection.sendall(b'E');feed.until(active,40)
                feed.connection.sendall(b'R')
                feed.until(lambda s:len(s.get('rows',[]))==3,40)
                rows=feed.state['rows']
                (feed.out/f'{width}x{height}-research.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
                assert [r['cost'] for r in rows]==[300,400,500]
                assert [r['ticks'] for r in rows]==[302,502,702]
                assert [r['count'] for r in rows]==[1,2,3] and all(r['allowed'] for r in rows)
                print('Original castle research passed',width,height,rows,flush=True)
            finally:
                player.terminate()
                player.wait(timeout=10)
                http.shutdown()
                tcp.shutdown()


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('width',type=int);parser.add_argument('height',type=int)
    args=parser.parse_args();run(args.width,args.height)
