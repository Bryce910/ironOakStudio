#!/usr/bin/env python3
"""Private joust terrain: extend the flat course and feather into owned hills."""
from pathlib import Path
import json,hashlib,math,struct,shutil,os
import numpy as np
ST=Path(__file__).resolve().parent;P=ST/'joust-project';G=P/'ground';R=ST/'review';R.mkdir(exist_ok=True)
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
if not G.is_symlink():raise SystemExit('Refusing to overwrite an already private ground directory; inspect its prior receipt.')
origin=G.resolve();before={str(p.relative_to(origin)):sha(p)for p in sorted(origin.rglob('*'))if p.is_file()};raw=(origin/'showcase.height').read_bytes();magic,w,h,x0,z0,sx,sz=struct.unpack_from('<8sIIdddd',raw);assert magic==b'VWHGT1\0\0'and w==h==513 and len(raw)==48+4*w*h
old=np.frombuffer(raw,dtype='<f4',offset=48).reshape(h,w).copy();assert np.isfinite(old).all();x=x0+np.arange(w)*sx+1800;z=z0+np.arange(h)*sz+10500;xx,zz=np.meshgrid(x,z);distance=np.hypot(np.maximum(np.abs(xx)-23.,0.),np.maximum(np.abs(zz)-63.,0.));t=np.clip(distance/24.,0.,1.);blend=t*t*(3.-2.*t);new=(64.*(1.-blend)+old*blend).astype('<f4');assert np.isfinite(new).all();assert np.array_equal(new[distance>=24],old[distance>=24]);assert np.all(new[distance==0]==64.)
# Verify the complete bilinear cells touching the visible42x122m venue.
i0=int(math.floor((-1821.-x0)/sx));i1=int(math.ceil((-1779.-x0)/sx));j0=int(math.floor((-10561.-z0)/sz));j1=int(math.ceil((-10439.-z0)/sz));support=new[j0:j1+1,i0:i1+1];assert support.max()<=64.02;assert support.min()==64.
def sample(a,wx,wz):
 u=(wx-x0)/sx;v=(wz-z0)/sz;i=int(math.floor(u));j=int(math.floor(v));tx=u-i;ty=v-j;return float((a[j,i]*(1-tx)+a[j,i+1]*tx)*(1-ty)+(a[j+1,i]*(1-tx)+a[j+1,i+1]*tx)*ty)
probes=[]
for lz in [-61.,-50.,-40.,-30.,0.,30.,40.,50.,61.]:
 for lx in [-21.,-6.,-1.7,0.,1.7,6.,21.]:probes.append({'local':[lx,lz],'before_y':sample(old,lx-1800,lz-10500),'after_y':sample(new,lx-1800,lz-10500)})
# Prepare the complete physical copy before switching the private project.
tmp=P/'.joust-ground.pending';assert not tmp.exists();shutil.copytree(origin,tmp,symlinks=False);(tmp/'showcase.height').write_bytes(raw[:48]+new.tobytes());after={str(p.relative_to(tmp)):sha(p)for p in sorted(tmp.rglob('*'))if p.is_file()};changed=[n for n in before if before[n]!=after[n]];assert changed==['showcase.height'];assert before.keys()==after.keys();G.unlink();os.replace(tmp,G)
# Read-only review of authored root heights; the world owner can reground them.
root_review=[]
for f in sorted((P/'scenery').glob('*instances.txt')):
 for line_no,line in enumerate(f.read_text().splitlines(),1):
  cols=line.split()
  if not cols or cols[0].startswith('#')or len(cols)<14:continue
  try:wx=(float(cols[1])-.5)*100800.;wz=(float(cols[2])-.5)*(100800.*2818./5009.);y=float(cols[13])
  except ValueError:continue
  if not(x0<=wx<x0+(w-1)*sx and z0<=wz<z0+(h-1)*sz):continue
  prior=sample(old,wx,wz);now=sample(new,wx,wz);delta=prior-now
  if abs(delta)>.08 and any(word in cols[0]for word in ['tree','nature_','bush','shrub','grass','fern','rock']):root_review.append({'file':str(f),'line':line_no,'alias':cols[0],'x':wx,'z':wz,'authored_y':y,'old_ground_y':prior,'new_ground_y':now,'ground_change_m':-delta,'new_root_offset_m':y-now})
report={'schema':'valwood.joust.private-ground-review.v1','passed':True,'private_copy':str(G),'original_shared_ground':str(origin),'shared_ground_modified':False,'height_only_change':changed,'old_height_sha256':before['showcase.height'],'new_height_sha256':after['showcase.height'],'height_header_preserved':True,'dimensions':[w,h],'origin':[x0,z0],'spacing':[sx,sz],'flat_world_center':[-1800.,-10500.],'flat_half_size_m':[23.,63.],'flat_height_y':64.,'feather_m':24.,'feather_method':'Euclidean distance outside rectangle, cubic smoothstep; original samples retained unchanged at and beyond24m.','venue_half_size_m':[21.,61.],'venue_touching_bilinear_cell_min_y':float(support.min()),'venue_touching_bilinear_cell_max_y':float(support.max()),'venue_maximum_allowed_y':64.02,'visible_sand_y':64.08,'minimum_sand_clearance_m':float(64.08-support.max()),'changed_height_samples':int(np.count_nonzero(old!=new)),'maximum_height_change_m':float(np.max(np.abs(new-old))),'before_file_hashes':before,'after_file_hashes':after,'source_shared_hashes_unchanged':all(sha(origin/n)==s for n,s in before.items()),'probes':probes,'authored_roots_affected':len(root_review),'root_followup_examples':sorted(root_review,key=lambda x:abs(x['ground_change_m']),reverse=True)[:30],'source_code_modified':False,'GPU_used':False,'inputs':{str(Path(__file__).resolve()):sha(__file__)},'limits':['CPU height-field check only; native terrain shading, course framing and scenery root placement need separate visual acceptance.','Scenery files are unchanged. Existing explicit root heights near the flattened or feathered area may need re-grounding.']}
assert report['source_shared_hashes_unchanged'];(R/'JOUST_GROUND_CPU_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k]for k in ['passed','old_height_sha256','new_height_sha256','height_only_change','venue_touching_bilinear_cell_min_y','venue_touching_bilinear_cell_max_y','minimum_sand_clearance_m','changed_height_samples','maximum_height_change_m','authored_roots_affected']}))
