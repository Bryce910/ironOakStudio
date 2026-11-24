#!/usr/bin/env python3
"""No GL/context: fixture images, production CPU loader and cache identity."""
import pathlib,subprocess,zlib,struct,hashlib,json,time
N=pathlib.Path(__file__).resolve().parents[1]
R=N/'review/environment-textures';D=R/'fixtures';D.mkdir(parents=True,exist_ok=True)
def png(name,w,h,pixel):
 def chunk(k,v):return struct.pack('>I',len(v))+k+v+struct.pack('>I',zlib.crc32(k+v)&0xffffffff)
 raw=b''.join(b'\0'+b''.join(bytes(pixel(x,y)) for x in range(w)) for y in range(h))
 (D/name).write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))
png('wide.png',4096,16,lambda x,y:((255 if x%2 else 0),)*3+(255,))
png('small.png',257,7,lambda x,y:(x%256,y*23,71,255))
png('odd.png',1537,769,lambda x,y:(32 if x<1536 else 255,73,119,255))
png('mask.png',1024,8,lambda x,y:(255,64,32,255) if x%2 else (0,0,0,0))
png('white.png',2048,8,lambda x,y:(255,255,255,255))
S=N/'source-v1';commands=[['gcc','-std=c99','-O2','-I'+str(S),'-c',str(S/'engine/texture_cache.c'),'-o',str(R/'texture_cache.o')],['g++','-std=c++17','-O2','-Wall','-Wextra','-I'+str(S),str(N/'tools/test_environment_texture_cpu.cpp'),str(S/'engine/world_model_cpu.cpp'),str(R/'texture_cache.o'),'-o',str(N/'tools/test_environment_texture_cpu')],[str(N/'tools/test_environment_texture_cpu'),str(D)]]
started=time.monotonic();outputs=[]
for cmd in commands:
 p=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);outputs.append(p.stdout);(R/'CPU.log').write_text('\n'.join(outputs));print(p.stdout,end='')
 if p.returncode:raise SystemExit(p.returncode)
files=['engine/world_model_cpu.cpp','engine/world_model_cpu.h','engine/world_model_pending_textures.h','engine/world_model_texture_budget.h']
receipt={'passed':True,'real_gpu':False,'gl_calls':0,'seconds':time.monotonic()-started,'commands':commands,'production_sha256':{p:hashlib.sha256((S/p).read_bytes()).hexdigest() for p in files},'result':outputs[-1].strip(),'scope':'Production CPU prepare, exact cache policy and structural certified constant-white BC7 fixture. No GPU timing or native visual claim.'}
(R/'CPU_RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n')
