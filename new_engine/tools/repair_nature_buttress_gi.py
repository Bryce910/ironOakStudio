#!/usr/bin/env python3
"""Targeted isolated GI attribute repair; position/normal/UV/index bytes unchanged.

Uses exact original painted UV1 on confirmed fileID bindings. Other affected
vertices may borrow only the same asset instance's verified near-LOD bake,
within5cm and cosine0.95. Never samples the known mismatched restored UV1.
"""
from pathlib import Path
import argparse,collections,copy,hashlib,json,re
import numpy as np
from scipy.spatial import cKDTree
from cook_nature_showcase import ROOT,PRIVATE,Source,DEFAULT_SOURCE
from cook_nature_showcase_gi import uv1
from nature_lightmap_sampler import LightmapSampler
from bake_nature_material_fidelity import glb_document
from align_nature_showcase_lods import write_glb

def key(p):return(p['group'],re.sub(r'(?i)LOD[ _]?[0-9]+','LOD',p['hierarchy']),tuple(round(v,5)for v in p['matrix']))
def transformed(source,p):
    v,index=source.mesh(p['mesh']);m=np.array(p['matrix']).reshape(4,4,order='F');points=v[:,:3]@m[:3,:3].T+m[:3,3];normals=v[:,3:6]@np.linalg.inv(m[:3,:3]);normals/=np.maximum(np.linalg.norm(normals,axis=1,keepdims=True),1e-12)
    return v,index,points,normals
def sample(sampler,p,coords,normals):
    st=np.array([p['lightmap_scale_offset'][c]for c in 'xyzw']);return sampler.sample_rgba(p['lightmap_index'],coords*st[:2]+st[2:],normals)
def array(doc,binary,index):
    a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']];width={'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];assert a['componentType']==5126
    return np.ndarray((a['count'],width),'<f4',binary,v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',width*4),4))

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--catalog',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase_gi/catalog.json');ap.add_argument('--output',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-buttress-gi');args=ap.parse_args();args.output.mkdir(exist_ok=True);(args.output/'chunks').mkdir(exist_ok=True)
    source=Source(DEFAULT_SOURCE);sampler=LightmapSampler();bindingdoc=json.loads((ROOT/'docs/stronghold/2026-11-01/showcase/buttress-uv1-bindings.json').read_text());bindings={b['native_placement_index']:b for b in bindingdoc['bindings']};bad=set(json.loads((ROOT/'docs/stronghold/2026-11-01/showcase/lightmap-uv-mismatch-hashes.json').read_text())['native_mesh_hashes']);groups=collections.defaultdict(list)
    for index,p in enumerate(source.doc['placements']):groups[key(p)].append((index,p))
    repairs=[];stats=collections.Counter();by_lod=[[],[]]
    for index,p in enumerate(source.doc['placements']):
        if p['mesh']not in bad:continue
        v,indices,points,normals=transformed(source,p);light=np.zeros((len(v),4),np.float32);method='same-instance near transfer';near=[(j,q)for j,q in groups[key(p)]if q['lod_mask']&1 and q['mesh']not in bad]
        record={'placement_index':index,'hierarchy':p['hierarchy'],'target_hash':p['mesh'],'lightmap_index':p['lightmap_index'],'scale_offset':p['lightmap_scale_offset'],'world_matrix':p['matrix']}
        if index in bindings:
            b=bindings[index];raw=Path(b['original_painted_binary']).read_bytes();original=np.ndarray((b['original_vertices'],16),'<f4',raw)
            assert np.allclose(original[:,:6],v[:,:6],atol=1e-5,rtol=0);light=sample(sampler,p,original[:,8:10],normals);method='original painted UV1';record.update(original_binding=b)
        elif len(near)==1:
            ni,np_=near[0];nv,_,near_points,near_normals=transformed(source,np_);near_light=sample(sampler,np_,uv1(source,np_['mesh']),near_normals);valid=near_light[:,3]>.5
            if valid.any():
                rp,rn,rg=near_points[valid],near_normals[valid],near_light[valid];distance,ids=cKDTree(rp).query(points,k=min(8,len(rp)),workers=1)
                if distance.ndim==1:distance=distance[:,None];ids=ids[:,None]
                cosine=np.einsum('ijk,ik->ij',rn[ids],normals);weight=np.maximum(cosine,0)**8/np.maximum(distance,.002)**2;weight[(distance>.05)|(cosine<.95)]=0
                total=weight.sum(1);ok=total>1e-12;light[ok,:3]=(rg[ids,:3]*weight[:,:,None]).sum(1)[ok]/total[ok,None];light[ok,3]=1
                record.update(near_placement_index=ni,near_hash=np_['mesh'],near_lightmap_index=np_['lightmap_index'],near_scale_offset=np_['lightmap_scale_offset'],near_world_matrix=np_['matrix'],normal_cosine_min=.95,max_transfer_distance_m=.05)
        record.update(method=method,vertices=len(v),verified_gi_vertices=int((light[:,3]>.5).sum()));repairs.append(record);stats[method+' vertices']+=record['verified_gi_vertices'];stats['unrepaired_vertices']+=len(v)-record['verified_gi_vertices']
        if not(light[:,3]>.5).any():continue
        for lod in [0,1]:
            if p['lod_mask']&(1<<lod):
                for sub,ix in enumerate(indices):
                    used=np.unique(ix);good=used[light[used,3]>.5];by_lod[lod].append((points[good],normals[good],v[good,6:8],light[good]))
    targets=[]
    for parts in by_lod:
        if not parts:targets.append(None);continue
        p,n,u,g=[np.concatenate([part[i]for part in parts])for i in range(4)];targets.append((cKDTree(p),p,n,u,g))
    catalog=copy.deepcopy(json.loads(args.catalog.read_text()));files=[]
    for record in catalog:
        for lod,originalpath in enumerate(record['files']):
            target=targets[lod]
            if target is None:continue
            tree,rp,rn,ru,rg=target;origin=np.array(record['origin']);lo=origin-np.array([record['dimensions'][0]*.51,.1,record['dimensions'][2]*.51]);hi=origin+np.array([record['dimensions'][0]*.51,record['dimensions'][1]+.1,record['dimensions'][2]*.51])
            # Cheap exact source-bound broad phase before reading large files.
            if not np.any(np.all((rp>=lo)&(rp<=hi),axis=1)):continue
            doc,binary=glb_document(Path(originalpath));before=hashlib.sha256(binary).hexdigest();patched=0;nonlight=hashlib.sha256();nonlight_after=hashlib.sha256()
            for mesh in doc['meshes']:
                for primitive in mesh['primitives']:
                    a=primitive['attributes'];gi=a.get('_VALWOOD_BAKED_LIGHT')
                    if gi is None:continue
                    light=array(doc,binary,gi);missing=np.flatnonzero(light[:,3]<.5)
                    if not len(missing):continue
                    points=array(doc,binary,a['POSITION'])[missing].astype(float);normals=array(doc,binary,a['NORMAL'])[missing].copy();u=array(doc,binary,a['TEXCOORD_0'])[missing].copy();points[:,2]*=-1;points+=origin;normals[:,2]*=-1;u[:,1]=1-u[:,1]
                    distance,ids=tree.query(points,k=min(8,len(rp)),workers=1)
                    if distance.ndim==1:distance=distance[:,None];ids=ids[:,None]
                    cosine=np.einsum('ijk,ik->ij',rn[ids],normals);du=np.max(np.abs(ru[ids]-u[:,None]),axis=2);ok=(distance<.00025)&(cosine>.9999)&(du<.00002);exists=ok.any(1);rows=np.flatnonzero(exists);cols=ok[rows].argmax(1)
                    # Reject coincident position/normal/UV0 with genuinely
                    # different original GI, instead of selecting a chart.
                    chosen=rg[ids[rows,cols]];dif=np.max(np.abs(rg[ids[rows]]-chosen[:,None]),axis=2);allowed=np.maximum(.02,np.max(chosen[:,:3],axis=1)*.04)
                    ambiguous=np.any(ok[rows]&(dif>allowed[:,None]),axis=1);rows=rows[~ambiguous];chosen=chosen[~ambiguous]
                    light[missing[rows]]=chosen;patched+=len(rows);stats['ambiguous_chart_vertices_rejected']+=int(ambiguous.sum())
            if not patched:continue
            # Every non-GI accessor must retain exactly its previous bytes.
            olddoc,oldbinary=glb_document(Path(originalpath));giids={p['attributes']['_VALWOOD_BAKED_LIGHT']for m in doc['meshes']for p in m['primitives']if '_VALWOOD_BAKED_LIGHT'in p['attributes']}
            for ai,a in enumerate(doc['accessors']):
                if ai in giids:continue
                view=doc['bufferViews'][a['bufferView']];offset=view.get('byteOffset',0);length=view['byteLength'];nonlight.update(oldbinary[offset:offset+length]);nonlight_after.update(binary[offset:offset+length])
            assert nonlight.digest()==nonlight_after.digest()
            output=args.output/'chunks'/Path(originalpath).name;write_glb(output,doc,binary);record['files'][lod]=str(output)
            record.setdefault('file_sha256',[None]*len(record['files']))[lod]=hashlib.sha256(output.read_bytes()).hexdigest()
            record['targeted_buttress_gi_repair']=True
            files.append({'source':originalpath,'candidate':str(output),'patched_vertices':patched,'source_binary_sha256':before,'candidate_binary_sha256':hashlib.sha256(binary).hexdigest(),'non_GI_accessor_bytes_unchanged':True});print('PATCH GI',output.name,patched,flush=True)
    (args.output/'catalog.json').write_text(json.dumps(catalog,indent=2)+'\n');report={'targeted_placements':repairs,'stats':dict(stats),'files':files,'original_catalog_untouched':True,'positions_normals_uvs_indices_unchanged':True,'policy':'Only strict original UV1 binding or same-instance near bake within5cm/cosine.95; mismatched restored UV1 is never sampled'};(args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('READY',len(files),'files',dict(stats),flush=True)

if __name__=='__main__':main()
