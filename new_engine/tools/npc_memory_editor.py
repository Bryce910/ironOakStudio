#!/usr/bin/env python3
"""Serve the bundled offline NPC memory editor on loopback only.

No upload/install/write API, third-party scripts, cloud storage or accounts.
The browser explicitly downloads project and engine files. This server serves
only four fixed application resources; it exposes no directory listing.
"""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
from urllib.request import urlopen
import argparse
import json
import os
import subprocess
import sys
import tempfile
import time
import webbrowser

ROOT=Path(__file__).resolve().parent/'npc_memory_editor'/'dist'
APP='valwood-memory-studio-v1'
FILES={'/':('index.html','text/html; charset=utf-8'),'/index.html':('index.html','text/html; charset=utf-8'),'/style.css':('style.css','text/css; charset=utf-8'),'/app.mjs':('app.mjs','text/javascript; charset=utf-8'),'/memory.mjs':('memory.mjs','text/javascript; charset=utf-8')}

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        path=urlsplit(self.path).path
        if path=='/health':
            body=json.dumps({'app':APP,'pid':os.getpid()}).encode();mime='application/json'
        elif path in FILES:
            file,mime=FILES[path]
            try:body=(ROOT/file).read_bytes()
            except OSError:self.send_error(503,'Editor files unavailable');return
        else:self.send_error(404);return
        self.send_response(200);self.send_header('Content-Type',mime);self.send_header('Content-Length',str(len(body)))
        self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff')
        self.send_header('Content-Security-Policy',"default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; connect-src 'none'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'")
        self.end_headers();self.wfile.write(body)
    def log_message(self,fmt,*args):
        if args and str(args[1] if len(args)>1 else '').startswith(('4','5')):super().log_message(fmt,*args)

def health(port):
    try:
        with urlopen(f'http://127.0.0.1:{port}/health',timeout=.35) as response:
            info=json.load(response)
            return info if info.get('app')==APP else None
    except (OSError,ValueError):return None

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--serve',action='store_true');parser.add_argument('--no-browser',action='store_true');parser.add_argument('--port',type=int,default=8871);args=parser.parse_args()
    if not 1024<=args.port<=65525:parser.error('Port must be1024..65525')
    if args.serve:
        server=ThreadingHTTPServer(('127.0.0.1',args.port),Handler)
        print(f'Memory Studio ready at http://127.0.0.1:{args.port}',flush=True)
        try:server.serve_forever()
        except KeyboardInterrupt:pass
        finally:server.server_close()
        return
    info=None
    for port in range(args.port,args.port+10):
        info=health(port)
        if info:break
        with tempfile.TemporaryFile() as log:
            child=subprocess.Popen([sys.executable,str(Path(__file__).resolve()),'--serve','--port',str(port)],stdin=subprocess.DEVNULL,stdout=log,stderr=log,start_new_session=True)
            for _ in range(20):
                if child.poll() is not None:break
                info=health(port)
                if info:break
                time.sleep(.1)
            if info:break
            if child.poll() is None:child.terminate();child.wait(timeout=5)
    if not info:raise SystemExit('Could not open a local editor port.')
    url=f'http://127.0.0.1:{port}/'
    print(json.dumps({'url':url,**info}),flush=True)
    if not args.no_browser:webbrowser.open(url,new=2)

if __name__=='__main__':main()
