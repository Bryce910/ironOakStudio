#!/usr/bin/env python3
"""Bake purchased dual-UV stone materials and authored prefab LODs for Valwood."""
from pathlib import Path
import collections,copy,ctypes,hashlib,json,re,subprocess,sys
import numpy as np
from PIL import Image
import yaml
from build_cinematic_diversity import Writer
from import_world_asset import cook
from prepare_nature_guardian import srgb_to_linear,linear_to_srgb

ROOT=Path(__file__).resolve().parents[1];DATA=ROOT/'assets/purchased/nature_castle'
NATIVE=ROOT.parent/'.local-tools/unity-source/citadel-native'
OUT=DATA/'citadel';SIZE=1024

def yaml_material(path):
 text=re.sub(r'^%.*\n','',path.read_text(),flags=re.M)
 text=re.sub(r'^--- !u!\d+ &-?\d+','---',text,flags=re.M)
 return next(x['Material']for x in yaml.safe_load_all(text)if x and'Material'in x)['m_SavedProperties']

class Materials:
 def __init__(self,parts):
  self.paths={e['guid']:e['path']for e in json.loads((DATA/'INVENTORY.json').read_text())['entries']}
  self.parts=parts;self.images={};self.cache_file=OUT/'material-cache.json'
  self.cache=json.loads(self.cache_file.read_text())if self.cache_file.exists()else{}
  self.cache={k:v for k,v in self.cache.items()if all(Path(p).is_file()for p in v['paths'].values())}
  library=ROOT.parent/'.local-tools/unity-source/nature_uv_raster.so'
  subprocess.run(['g++','-O2','-shared','-fPIC',str(ROOT/'tools/nature_uv_raster.cpp'),'-o',str(library)],check=True)
  self.native=ctypes.CDLL(str(library));self.native.nature_uv_raster.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_void_p]
  yy,xx=np.mgrid[:SIZE,:SIZE];self.uv=np.stack(((xx+.5)/SIZE,1-(yy+.5)/SIZE),axis=2).astype(np.float32)

 def make(self,path):
  if path in self.cache:return self.cache[path]
  saved=yaml_material(DATA/'source'/path)
  f={k:v for r in saved['m_Floats']for k,v in r.items()};c={k:v for r in saved['m_Colors']for k,v in r.items()};tex={k:v for r in saved['m_TexEnvs']for k,v in r.items()}
  repairs=[]
  if tex.get('_BaseColorMap',{}).get('m_Texture',{}).get('guid')not in self.paths and '_fireplace'in path:
   dry=DATA/'source'/path.replace('_fireplace.mat','.mat');dry_saved=yaml_material(dry)
   dry_tex={k:v for r in dry_saved['m_TexEnvs']for k,v in r.items()}
   for key in ['_BaseColorMap','_BaseNormalMap','_BaseMaskMap']:tex[key]=dry_tex[key]
   repairs.append('Unavailable lava trial base replaced with matching purchased dry pillar base; original shape atlas retained')
  def rgba(key,default=(1,1,1,1)):return np.array([c[key][v]for v in 'rgba'],dtype=np.float32)if key in c else np.array(default,dtype=np.float32)
  uv2=np.full((SIZE,SIZE,2),np.nan,np.float32);seen=set()
  for vertex,index in self.parts[path]:
   digest=hashlib.sha256(vertex[:,6:10].tobytes()+index.tobytes()).digest()
   if digest in seen:continue
   seen.add(digest);self.native.nature_uv_raster(vertex.ctypes.data,index.ctypes.data,len(index),SIZE,uv2.ctypes.data)
  missing=~np.isfinite(uv2[:,:,0]);uv2[missing]=self.uv[missing]
  base_uv=self.uv if f.get('_UV0_UV2',1)>=.5 else uv2
  shape_uv=self.uv if f.get('_Shape_UV0_UV2',1)>=.5 else uv2
  def image(key,uv,tiling=None,default=(1,1,1,1)):
   info=tex.get(key,{});guid=info.get('m_Texture',{}).get('guid')
   if not guid or (guid not in self.paths and key=='_LeaksR'):return np.broadcast_to(np.array(default,dtype=np.float32),(SIZE,SIZE,4)).copy()
   source=DATA/'source'/self.paths[guid]
   if source not in self.images:
    with Image.open(source)as raw:self.images[source]=np.asarray(raw.convert('RGBA').resize((SIZE,SIZE),Image.Resampling.LANCZOS),dtype=np.float32)/255
   transform=rgba(tiling,(1,1,0,0))if tiling else np.array([info.get('m_Scale',{}).get('x',1),info.get('m_Scale',{}).get('y',1),info.get('m_Offset',{}).get('x',0),info.get('m_Offset',{}).get('y',0)])
   xy=(uv*transform[:2]+transform[2:])%1
   x=(xy[:,:,0]*SIZE-.5)%SIZE;y=((1-xy[:,:,1])*SIZE-.5)%SIZE
   xf=np.floor(x);yf=np.floor(y);x0=xf.astype(int)%SIZE;y0=yf.astype(int)%SIZE;fx=(x-xf)[:,:,None];fy=(y-yf)[:,:,None];im=self.images[source]
   return (im[y0,x0]*(1-fx)+im[y0,(x0+1)%SIZE]*fx)*(1-fy)+(im[(y0+1)%SIZE,x0]*(1-fx)+im[(y0+1)%SIZE,(x0+1)%SIZE]*fx)*fy
  base=image('_BaseColorMap',base_uv,'_BaseTilingOffset')
  mask=image('_BaseMaskMap',base_uv,'_BaseTilingOffset',default=(0,1,1,.1))
  shape=image('_ShapeCurvAOHLeaksMask',shape_uv,'_ShapeTilingOffset',default=(.5,1,1,1))
  leaks=image('_LeaksR',base_uv,'_LeaksTilingOffset')[:,:,:1]
  tint=rgba('_BaseColor');stain=rgba('_BaseLeaksColorMultiply')
  weight=np.clip(stain[3]*(1-shape[:,:,3:4]),0,1)
  rgb=srgb_to_linear(base[:,:,:3])*tint[:3]
  rgb*=1-weight+weight*((1-leaks)**2*stain[:3]+leaks)
  curvature=np.clip(rgb+2*shape[:,:,:1]-1,0,1);reduction=f.get('_Shape_AO_Curvature_Reduction',0)
  curvature*=1-reduction+reduction*shape[:,:,1:2]
  amount=f.get('_CurvatureBlend',0);rgb=rgb*(1-amount)+curvature*amount
  normals=[]
  for key,uv,tiling,strength in [('_BaseNormalMap',base_uv,'_BaseTilingOffset',f.get('_BaseNormalScale',1)),('_ShapeNormal',shape_uv,'_ShapeTilingOffset',f.get('_ShapeNormalStrenght1',1))]:
   n=image(key,uv,tiling,default=(.5,.5,1,1))[:,:,:3]*2-1;n[:,:,:2]*=strength;n[:,:,2]=np.sqrt(np.maximum(1-(n[:,:,:2]**2).sum(2),0));normals.append(n)
  normal=np.dstack((normals[0][:,:,0]+normals[1][:,:,0],normals[0][:,:,1]+normals[1][:,:,1],normals[0][:,:,2]*normals[1][:,:,2]));normal/=np.maximum(np.linalg.norm(normal,axis=2,keepdims=True),1e-8)
  smooth=f.get('_BaseSmoothnessRemapMin',0)+mask[:,:,3:4]*(f.get('_BaseSmoothnessRemapMax',1)-f.get('_BaseSmoothnessRemapMin',0))
  smooth*=1-weight+weight*(f.get('_LeaksSmoothnessMultiply',1)*(1-leaks)+leaks)
  arm=np.concatenate((mask[:,:,1:2]*shape[:,:,1:2],np.clip(1-smooth,.32,1),mask[:,:,:1]*f.get('_BaseMetallic',1)),2)
  dest=OUT/'textures'/Path(path).stem;dest.mkdir(parents=True,exist_ok=True)
  for role,value in [('base',linear_to_srgb(rgb)),('normal',normal*.5+.5),('arm',arm)]:
   im=Image.fromarray(np.uint8(np.clip(value,0,1)*255+.5))
   if role=='arm':im=im.resize((512,512),Image.Resampling.LANCZOS)
   im.save(dest/(role+'.png'))
  result={'name':Path(path).stem,'paths':{role:str(dest/(role+'.png'))for role in ['base','normal','arm']},'snow':1 if 'snow'in Path(path).stem.lower()else 0,'uv2_coverage':float((~missing).mean()),'repairs':repairs}
  self.cache[path]=result;self.cache_file.write_text(json.dumps(self.cache,indent=2)+'\n');print('Baked material',result['name'],flush=True);return result

def main():
 OUT.mkdir(exist_ok=True);assets=[];material_parts=collections.defaultdict(list)
 for path in sorted(NATIVE.glob('*.json')):
  doc=json.loads(path.read_text());blob=path.with_suffix('.bin').read_bytes();parts=[]
  for p in doc['parts']:
   vertex=np.ndarray((p['vertices'],14),dtype='<f4',buffer=blob,offset=p['vertex_offset']).copy()
   index=np.ndarray((p['indices'],),dtype='<i4',buffer=blob,offset=p['index_offset']).copy()
   assert np.isfinite(vertex).all()and np.all(index>=0)and np.all(index<len(vertex))
   parts.append((p,vertex,index))
   if p['lod']==0:material_parts[p['material']].append((vertex,index))
  assets.append((doc,parts))
 selected=json.loads((NATIVE.parent/'citadel-selection.json').read_text())['assets']
 assert len(assets)==len(selected),'Incomplete native export'
 mats=Materials(material_parts);report=[]
 for doc,parts in assets:
  near=np.concatenate([v[:,:3]for p,v,i in parts if p['lod']==0]);lo=near.min(0);hi=near.max(0)
  origin=np.array([(lo[0]+hi[0])/2,lo[1],(lo[2]+hi[2])/2],np.float32)
  materials=[];textures=[];images=[];indices={}
  for path in sorted({p['material']for p,v,i in parts}):
   info=mats.make(path);n=len(textures)
   for role in ['base','normal','arm']:
    images.append({'uri':info['paths'][role],'mimeType':'image/png'});textures.append({'source':len(images)-1,'sampler':0})
   material={'name':info['name'],'pbrMetallicRoughness':{'baseColorFactor':[1,1,1,1],'metallicFactor':1,'roughnessFactor':1,'baseColorTexture':{'index':n},'metallicRoughnessTexture':{'index':n+2}},'normalTexture':{'index':n+1},'occlusionTexture':{'index':n+2}}
   interior=doc['name'].startswith(('hall_','altar_'))or doc['name']in ['floor','arch']
   if info['snow']and not interior:material['extensions']={'VALWOOD_materials_winter':{'parameters':[1,.86,0,0]}}
   indices[path]=len(materials);materials.append(material)
  files=[];triangles=[]
  for lod,label in [(0,'near'),(1,'mid')]:
   writer=Writer(*copy.deepcopy((materials,textures,images)));groups=collections.defaultdict(lambda:[[],[],[],[]]);counts=collections.Counter()
   for meta,vertex,index in parts:
    if meta['lod']!=lod:continue
    used,idx=np.unique(index,return_inverse=True);v=vertex[used];p=v[:,:3]-origin;n=v[:,3:6].copy();uv=v[:,6:8].copy();p[:,2]*=-1;n[:,2]*=-1;uv[:,1]=1-uv[:,1]
    idx=idx.reshape(-1,3);norm=np.linalg.norm(n,axis=1,keepdims=True);assert np.min(norm)>.5;n/=norm
    cross=np.cross(p[idx[:,1]]-p[idx[:,0]],p[idx[:,2]]-p[idx[:,0]]);valid=np.linalg.norm(cross,axis=1)>1e-9
    if valid.any()and np.median((cross*n[idx].sum(1)).sum(1)[valid])<0:idx=idx[:,[0,2,1]]
    mat=indices[meta['material']];g=groups[mat];g[0].append(p);g[1].append(n);g[2].append(uv);g[3].append(idx+counts[mat]);counts[mat]+=len(p)
   total=0
   for mat,g in groups.items():
    arrays=[np.concatenate(x)for x in g];writer.mesh(doc['name']+str(mat),*arrays,mat);total+=len(arrays[3])
   path=OUT/(doc['name']+'_'+label+'.glb');writer.save(path,embedded=True);assert path.stat().st_size<128*1024**2
   files.append(path);triangles.append(total)
  stem=cook(ROOT/'projects/valwood_world','nm_'+doc['name'],(files[0],files[1],None),1)
  record={'name':doc['name'],'stem':stem,'source':doc['source'],'dimensions':(hi-lo).tolist(),'origin':origin.tolist(),'triangles':triangles,'materials':len(materials),'files':[str(p.relative_to(ROOT))for p in files]};report.append(record)
  (OUT/'catalog.json').write_text(json.dumps(report,indent=2)+'\n');print('Cooked',doc['name'],triangles,'metres',np.round(hi-lo,2),flush=True)
 (OUT/'material-report.json').write_text(json.dumps(mats.cache,indent=2)+'\n')

if __name__=='__main__':main()
