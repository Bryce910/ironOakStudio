#!/usr/bin/env python3
"""Recover exact original painted UV1 bindings from scene/prefab file IDs.

Only unambiguous geometry-and-world-transform matches may become GI repair
candidates. This does not change source scene files, cooked GLBs or imports.
"""
from pathlib import Path
import collections,functools,json,re
import numpy as np
import yaml
from cook_nature_showcase import ROOT,PRIVATE,Source,DEFAULT_SOURCE

def trs(d):
    p=np.array([d.get('m_LocalPosition',{}).get(a,0)for a in 'xyz']);s=np.array([d.get('m_LocalScale',{}).get(a,1)for a in 'xyz'])
    x,y,z,w=[d.get('m_LocalRotation',{}).get(a,1 if a=='w'else 0)for a in 'xyzw'];q=np.array([x,y,z,w],dtype=float);q/=max(np.linalg.norm(q),1e-20);x,y,z,w=q
    r=np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])
    m=np.eye(4);m[:3,:3]=r*s;m[:3,3]=p;return m

class Resolver:
    def __init__(self):
        self.layout=json.loads((ROOT/'docs/stronghold/2026-11-01/showcase/publisher-scene-layout.json').read_text());self.prefabs={p['id']:p for p in self.layout['prefabs']}
    @functools.lru_cache(maxsize=64)
    def prefab(self,path):
        text=(PRIVATE/'asset-bridge'/path).read_text();records={}
        for match in re.finditer(r'(?m)^--- !u!(\d+) &(-?\d+)(?: stripped)?\n([\s\S]*?)(?=^--- !u!|\Z)',text):
            if int(match[1])not in (1,4,33):continue
            records[int(match[2])]=next(iter(yaml.load(match[3],Loader=yaml.CSafeLoader).values()))
        return records
    @functools.lru_cache(maxsize=None)
    def scene_world(self,ident):
        if not ident:return np.eye(4)
        d=self.layout['transforms'][str(ident)]
        if 'm_LocalPosition'in d:return self.scene_world(d.get('m_Father',{}).get('fileID',0))@trs(d)
        return self.instance_world(d['m_PrefabInstance']['fileID'],d['m_CorrespondingSourceObject']['fileID'])
    @functools.lru_cache(maxsize=None)
    def instance_world(self,pid,tid):
        p=self.prefabs[pid];d=dict(self.prefab(p['source_path'])[tid])
        if 'm_LocalPosition'not in d:raise ValueError('Unexpanded nested transform '+str((pid,tid)))
        for key in ('m_LocalPosition','m_LocalRotation','m_LocalScale'):d[key]=dict(d.get(key,{}))
        for mod in p['m_Modification']['m_Modifications']:
            if mod['target']['fileID']!=tid:continue
            a=mod['propertyPath'].split('.')
            if len(a)==2 and a[0]in ('m_LocalPosition','m_LocalRotation','m_LocalScale'):d[a[0]][a[1]]=float(mod.get('value')or 0)
        father=d.get('m_Father',{}).get('fileID',0)
        parent=self.instance_world(pid,father)if father else self.scene_world(p['m_Modification']['m_TransformParent']['fileID'])
        return parent@trs(d)
    def mesh_world(self,override):
        p=self.prefabs[override['prefab_instance_id']];records=self.prefab(p['source_path']);mesh=records[override['target']['fileID']];obj=records[mesh['m_GameObject']['fileID']]
        transform=next(c['component']['fileID']for c in obj['m_Component']if 'm_LocalPosition'in records.get(c['component']['fileID'],{}))
        return self.instance_world(p['id'],transform),obj['m_Name'],transform

def main():
    source=Source(DEFAULT_SOURCE);painted=json.loads((PRIVATE/'showcase-painted/painted.json').read_text());by={m['scene_file_id']:m for m in painted['meshes']};bad=set(json.loads((ROOT/'docs/stronghold/2026-11-01/showcase/lightmap-uv-mismatch-hashes.json').read_text())['native_mesh_hashes']);targets=[(i,p)for i,p in enumerate(source.doc['placements'])if p['mesh']in bad];resolver=Resolver();bindings=[];rejections=[]
    for override in painted['overrides']:
        original=by.get(override['mesh_scene_file_id'])
        if not original or not original['name'].startswith('Buttress_level0_00_r_')or 'LOD0' in original['name']:continue
        try:matrix,name,transform=resolver.mesh_world(override)
        except (KeyError,ValueError)as error:rejections.append({'override':override,'reason':str(error)});continue
        candidates=[]
        for index,p in targets:
            if p['name']!=name or source.meshes[p['mesh']]['vertices']!=original['vertices']:continue
            native_matrix=np.array(p['matrix']).reshape(4,4,order='F');error=np.abs(matrix-native_matrix)
            if error[:3,3].max()>.002 or error[:3,:3].max()>.00002:continue
            raw=(PRIVATE/'showcase-painted'/original['binary']).read_bytes();v16=np.ndarray((original['vertices'],16),'<f4',raw);native,_=source.mesh(p['mesh'])
            if not np.allclose(v16[:,:6],native[:,:6],atol=1e-5,rtol=0):continue
            if not np.allclose(v16[:,6:8],native[:,6:8],atol=1e-5,rtol=0):continue
            candidates.append((index,p,float(error.max())))
        if len(candidates)!=1:rejections.append({'override':override,'name':name,'reason':'Matched '+str(len(candidates))+' native placements'});continue
        index,p,error=candidates[0];bindings.append({'native_placement_index':index,'native_mesh_hash':p['mesh'],'native_hierarchy':p['hierarchy'],'original_prefab_instance_id':override['prefab_instance_id'],'original_target_file_id':override['target']['fileID'],'original_transform_file_id':transform,'original_scene_mesh_file_id':original['scene_file_id'],'original_painted_binary':str(PRIVATE/'showcase-painted'/original['binary']),'original_vertices':original['vertices'],'world_matrix':p['matrix'],'maximum_matrix_error':error,'lightmap_index':p['lightmap_index'],'lightmap_scale_offset':p['lightmap_scale_offset'],'position_normal_uv0_exact_match':True})
    counts=collections.Counter(b['native_placement_index']for b in bindings)
    if any(n!=1 for n in counts.values()):raise ValueError('Multiple original bindings for one native placement')
    report={'bindings':bindings,'rejections':rejections,'excluded_native_placements':len(targets),'strict_bound_placements':len(bindings),'unbound_remain_disabled':len(targets)-len(bindings),'source_original_ids_preserved':True,'policy':'Require unique source prefab child fileID and full world matrix, then exact local position/normal/UV0 equality; sample original UV1 only on confirmed binding'}
    path=ROOT/'docs/stronghold/2026-11-01/showcase/buttress-uv1-bindings.json';path.write_text(json.dumps(report,indent=2)+'\n');print('BOUND',len(bindings),'of',len(targets),'REJECTED',len(rejections));print(collections.Counter(x['reason']for x in rejections));print(path)

if __name__=='__main__':main()
