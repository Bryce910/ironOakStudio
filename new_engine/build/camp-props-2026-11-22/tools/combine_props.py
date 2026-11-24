"""Compact reviewed tent+hearth geometry into an isolated shared-texture bank."""
from pathlib import Path
import copy,hashlib,json,sys
import numpy as np
N=Path(__file__).resolve().parents[1];E=N.parents[1]
sys.path.insert(0,str(E/'tools'))
from build_opening_prologue import Glb,read_glb,array

class Importer:
    def __init__(self):
        self.g=Glb();self.images={};self.textures={};self.materials={};self.samplers={}
        self.g.doc['samplers']=[]
    def texture(self,d,b,i):
        t=d['textures'][i];im=d['images'][t['source']];v=d['bufferViews'][im['bufferView']]
        raw=b[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']];digest=hashlib.sha256(raw).hexdigest()
        if digest not in self.images:
            self.images[digest]=len(self.g.doc['images']);self.g.doc['images'].append(dict(bufferView=self.g.view(raw),mimeType=im['mimeType']))
        sampler=d.get('samplers',[{}])[t.get('sampler',0)];key=json.dumps(sampler,sort_keys=True)
        if key not in self.samplers:self.samplers[key]=len(self.g.doc['samplers']);self.g.doc['samplers'].append(copy.deepcopy(sampler))
        key=(digest,self.samplers[key])
        if key not in self.textures:self.textures[key]=len(self.g.doc['textures']);self.g.doc['textures'].append(dict(sampler=key[1],source=self.images[digest]))
        return self.textures[key]
    def material(self,d,b,i):
        key=(id(d),i)
        if key not in self.materials:
            m=copy.deepcopy(d['materials'][i])
            def remap(q):
                for k,v in q.items():
                    if k.endswith('Texture') and isinstance(v,dict) and 'index' in v:v['index']=self.texture(d,b,v['index'])
                    elif isinstance(v,dict):remap(v)
            remap(m);self.materials[key]=len(self.g.doc['materials']);self.g.doc['materials'].append(m)
        return self.materials[key]
    def add(self,path,filter_hearth=False):
        d,b=read_glb(path);stats=[]
        for mesh in d['meshes']:
            for q in mesh['primitives']:
                p=array(d,b,q['attributes']['POSITION']);n=array(d,b,q['attributes']['NORMAL']);u=array(d,b,q['attributes']['TEXCOORD_0']);ix=array(d,b,q['indices']).astype(int).reshape(-1,3)
                name=d['materials'][q['material']]['name'];kill=np.zeros(len(ix),bool)
                if filter_hearth:
                    if name in ('starting_firepit worn material','Low live coals'):kill[:]=True
                    if name in ('Charcoal and dry splits','Weathered peeled camp branches'):
                        pts=p[ix];kill=((np.abs(pts[:,:,0])<1.55)&(np.abs(pts[:,:,2])<.85)).all(1)
                keep=ix[~kill];stats.append(dict(material=name,kept=len(keep),removed=int(kill.sum())))
                if not len(keep):continue
                used=np.unique(keep);remap=np.full(len(p),-1);remap[used]=np.arange(len(used))
                self.g.mesh(p[used],n[used],u[used],remap[keep].ravel(),self.material(d,b,q['material']))
        return stats

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    out=N/'models';out.mkdir(exist_ok=True)
    reports=[]
    for lod in ['near','mid']:
        tent=N/'tent-work'/('camp_tent'+('_mid' if lod=='mid' else '')+'.glb')
        fire=N/'fire-work'/f'camp_fire_{lod}.glb'
        merge=Importer();kept=merge.add(tent,filter_hearth=True);merge.add(fire)
        merge.g.doc['asset']['generator']='Valwood campsite close-up prop pass 2026-11-22'
        dest=out/('camp_hearth'+('_mid' if lod=='mid' else '')+'.glb');dest.write_bytes(merge.g.finish())
        d,b=read_glb(dest)
        reports.append(dict(lod=lod,path=str(dest),sha256=sha(dest),bytes=dest.stat().st_size,triangles=sum(d['accessors'][q['indices']]['count']//3 for m in d['meshes'] for q in m['primitives']),materials=len(d['materials']),images=len(d['images']),tent_sha256=sha(tent),fire_sha256=sha(fire),retained_and_removed=kept))
    for file in (N/'bridge-work/models').glob('*.glb'):
        dest=out/file.name
        if dest.is_symlink():dest.unlink()
        dest.symlink_to(file)
    (N/'review/MODELS.json').write_text(json.dumps(dict(models=reports,placement_and_collisions_unchanged=True),indent=2)+'\n')
    print(json.dumps([{k:v for k,v in r.items() if k!='retained_and_removed'} for r in reports],indent=2))

if __name__=='__main__':main()
