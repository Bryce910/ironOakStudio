from pathlib import Path
import importlib.util,json,hashlib,sys,time,struct
import numpy as np
from PIL import Image
n=Path('/home/computer/Desktop/final_engine/new_engine/build/arena-material-quality-2026-11-15');e=n.parents[1]
spec=importlib.util.spec_from_file_location('convert',e/'tools/prepare_modular_knight_materials.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
# Keep the established source conversion and normal-basis corrections.
pack=e/'assets/purchased/modular_knights';inv=pack/'inventory.json'
print('inventory candidates',list(pack.glob('*inventory*')),flush=True)
if not inv.exists():
 for p in pack.rglob('*inventory*.json'):
  if 'entries' in json.loads(p.read_text()):inv=p;break
out=n/'character-textures';out.mkdir(exist_ok=True)
converter=m.Converter(pack/'source',inv,out/'high-source',profile='high')
registry={};inputs={};sources={};stats={};masks={};errors=[]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
# FNV via the system compiler helper avoids Python's per-byte overhead.
import ctypes,subprocess
helper=n/'review/hash.cpp';helper.write_text('#include <cstdint>\n#include <cstddef>\nextern "C" std::uint64_t hash(const unsigned char*p,std::size_t n){std::uint64_t h=14695981039346656037ull;for(std::size_t i=0;i<n;++i){h^=p[i];h*=1099511628211ull;}return h;}\n')
subprocess.run(['g++','-shared','-fPIC','-O2',str(helper),'-o',str(helper.with_suffix('.so'))],check=True)
lib=ctypes.CDLL(str(helper.with_suffix('.so')));lib.hash.argtypes=[ctypes.c_char_p,ctypes.c_size_t];lib.hash.restype=ctypes.c_uint64
fnv=lambda b:int(lib.hash(b,len(b)))
files=[e/'build/arena-character-roster-2026-11-14/knights_roster/materials/materials-gltf.json',pack/'derived/materials-balanced/materials.json']
from concurrent.futures import ThreadPoolExecutor,as_completed
from threading import Lock
claimed=set();claim_lock=Lock()
allrows=[]
for f in files:
 rows=json.loads(f.read_text());inputs[str(f)]=sha(f);allrows.extend(rows.items())
def process(item):
 key,row=item;local={};localstats={}
 import re
 stem=re.sub(r"[^A-Za-z0-9_-]+","_",Path(key).stem).strip("_")+"-"+hashlib.sha256(key.encode()).hexdigest()[:12]
 base_ext='.png' if row['alpha_mode']=='MASK' else '.jpg'
 hi={role:str(converter.output/(stem+suffix)) for role,suffix in [('base','-base'+base_ext),('normal','-normal.png'),('orm','-orm.png')]}
 try:
  for role,path in hi.items():
   if role=='normal' and not row.get(role):continue
   with Image.open(path) as im:im.load()
 except (OSError,ValueError,SyntaxError):hi=converter.material(key)
 for role in ('base','normal','orm'):
  if not row.get(role):continue
  old=Path(row[role]);oldbytes=old.read_bytes();h=fnv(oldbytes);identity=(h,len(oldbytes))
  with claim_lock:
   if identity in claimed:continue
   claimed.add(identity)
  high=Image.open(hi[role]).copy()
  if role=='normal' and row.get('normal_basis_conversion',{}).get('additional_green_inversion'):
   a=np.array(high.convert('RGB'));a[:,:,1]=255-a[:,:,1];high=Image.fromarray(a)
  variants=[]
  for level in ('512','1024','2048'):
   d=out/level;d.mkdir(exist_ok=True)
   ext='.png' if role!='base' or row['alpha_mode']=='MASK' else '.jpg';p=d/(f'{h:016x}-{len(oldbytes)}'+ext)
   valid_existing=False
   if p.exists():
    try:
     with Image.open(p) as check:check.load()
     valid_existing=True
    except (OSError,ValueError,SyntaxError):pass
   if not valid_existing:
    if level=='1024':p.write_bytes(oldbytes)
    elif level=='2048':
     if ext=='.jpg':high.convert('RGB').save(p,quality=94,subsampling=0,optimize=False)
     else:high.save(p,optimize=False)
    else:
     low=m.fit(Image.open(old),512)
     if role=='normal':
      a=np.asarray(low.convert('RGB'),dtype=np.float32)/127.5-1;a/=np.maximum(np.linalg.norm(a,axis=2,keepdims=True),1e-8);low=Image.fromarray(np.rint(np.clip(a*.5+.5,0,1)*255).astype('uint8'))
     if ext=='.jpg':low.convert('RGB').save(p,quality=94,subsampling=0,optimize=False)
     else:low.save(p,optimize=False)
   b=p.read_bytes();im=Image.open(p);variants.append((str(p.relative_to(out)),len(b),fnv(b)))
   localstats.setdefault(level,{'images':0,'encoded':0,'rgba_mips':0});s=localstats[level];s['images']+=1;s['encoded']+=len(b);w,hh=im.size
   while True:
    s['rgba_mips']+=w*hh*4
    if w==hh==1:break
    w=max(1,w//2);hh=max(1,hh//2)
  local[identity]=variants
 return key,local,localstats
with ThreadPoolExecutor(max_workers=3) as pool:
 futures={pool.submit(process,item):item[0] for item in allrows}
 for i,future in enumerate(as_completed(futures)):
  try:key,rows,totals=future.result()
  except Exception as error:
   import traceback
   print("FAILED",futures[future],flush=True);traceback.print_exc();errors.append(str(error));continue
  sources[key]=True
  for identity,variants in rows.items():
   if identity in registry:
    assert registry[identity]==variants
   else:registry[identity]=variants
  print('prepared',i+1,'/',len(allrows),key,flush=True)
# Derive final totals from distinct registry files only.
for level in ('512','1024','2048'):
 stats[level]={'images':0,'encoded':0,'rgba_mips':0}
 for variants in registry.values():
  path,size,h=variants[('512','1024','2048').index(level)];w,hh=Image.open(out/path).size;s=stats[level];s['images']+=1;s['encoded']+=size
  while True:
   s['rgba_mips']+=w*hh*4
   if w==hh==1:break
   w=max(1,w//2);hh=max(1,hh//2)
lines=['VALWOOD_CHARACTER_TEXTURES 1']
for (h,length),variants in sorted(registry.items()):
 lines.append(f'{h} {length} '+' '.join(f'"{path}" {size} {check}' for path,size,check in variants))
(out/'textures.manifest').write_text('\n'.join(lines)+'\n')
(n/'review/TEXTURE_SOURCE_REVIEW.json').write_text(json.dumps(dict(source_materials=len(sources),registered_images=len(registry),profiles=stats,inputs=inputs,manifest_sha256=sha(out/'textures.manifest'),source_files_modified=False,normal_basis_preserved=True),indent=2)+'\n')
print('DONE',stats,'ERRORS',errors,flush=True)
