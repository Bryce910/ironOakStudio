#!/usr/bin/env python3
"""Add the publisher's original baked indirect lighting to isolated source chunks.

Uses cached PBR maps, original per-renderer lightmap UV1/ST and world normals.
The first, already-reviewed source catalog and imported revisions are untouched.
"""
from pathlib import Path
import argparse,collections,copy,functools,json,hashlib,re
import numpy as np
from scipy.spatial import cKDTree
import cook_nature_showcase as base
from bake_nature_material_fidelity import glb_document
from repair_nature_cutout_alpha import repair_material_alpha
from nature_lightmap_sampler import LightmapSampler
from build_cinematic_diversity import Writer
from align_nature_showcase_lods import align

ROOT=base.ROOT;PRIVATE=base.PRIVATE

def bad_uv1_hashes(source):
    cache=ROOT/'docs/stronghold/2026-11-01/showcase/lightmap-uv-mismatch-hashes.json'
    if cache.exists():return set(json.loads(cache.read_text())['native_mesh_hashes'])
    painted=json.loads((PRIVATE/'showcase-painted/painted.json').read_text());by=collections.defaultdict(list)
    clean=lambda name:re.sub(r'\(Clone\)$','',name or'').strip()
    for m in source.doc['meshes']:by[(clean(m['name']),m['vertices'])].append(m)
    hashes=set();variants=[]
    for p in painted['meshes']:
        candidates=by.get((clean(p['name']),p['vertices']),[])
        if not candidates:continue
        raw=(PRIVATE/'showcase-painted'/p['binary']).read_bytes();v=np.ndarray((p['vertices'],16),'<f4',raw)
        for m in candidates:
            native=source.mesh(m['hash'])[0]
            if np.allclose(v[:,:6],native[:,:6],atol=1e-5):
                if not np.allclose(v[:,8:10],uv1(source,m['hash']),atol=1e-5):hashes.add(m['hash']);variants.append({'scene_mesh_file_id':p['scene_file_id'],'source_mesh_hash':m['hash'],'name':p['name']})
                break
    base.atomic_json(cache,{'native_mesh_hashes':sorted(hashes),'source_variants':variants,'policy':'Conservatively disable baked GI on restored FBX meshes with any known mismatched painted lightmap UV variant'})
    return hashes

@functools.lru_cache(maxsize=None)
def uv1(source,key):
    m=source.meshes[key];raw=(source.path.parent/m['binary']).read_bytes()
    return np.ndarray((m['vertices'],16),'<f4',raw)[:,8:10].copy()

def prepare_unit(source,unit,sampler,excluded,stats):
    staged=[];reference=[]
    for p in unit['placements']:
        vertex,indices=source.mesh(p['mesh']);matrix=np.asarray(p['matrix'],np.float64).reshape(4,4,order='F');v=vertex.copy()
        v[:,:3]=vertex[:,:3]@matrix[:3,:3].T+matrix[:3,3]
        v[:,3:6]=vertex[:,3:6]@np.linalg.inv(matrix[:3,:3]);v[:,3:6]/=np.maximum(np.linalg.norm(v[:,3:6],axis=1,keepdims=True),1e-10)
        st=p['lightmap_scale_offset'];st=np.array([st[k]for k in'xyzw'],np.float32)
        valid=bool(source.meshes[p['mesh']]['uv1_present']and p['mesh']not in excluded)
        light=sampler.sample_rgba(p['lightmap_index'],uv1(source,p['mesh'])*st[:2]+st[2:],v[:,3:6],valid=valid & (np.linalg.norm(v[:,3:6],axis=1)>.5))
        if not np.isfinite(light).all():raise ValueError('Nonfinite decoded source baked lighting: '+p['hierarchy'])
        if p['lod_mask']&1 and (light[:,3]>.5).any():
            mask=light[:,3]>.5;reference.append((v[mask,:3],v[mask,3:6],light[mask]))
        staged.append((p,v,indices,light))
    # Unity switches lower LODs to probes; transfer the exact near-LOD bake onto
    # reduced surfaces to keep the authored gradients stable during native LODs.
    tree=None;rp=rn=rg=None
    if reference:
        rp,rn,rg=[np.concatenate([r[i]for r in reference])for i in range(3)]
    for p,v,indices,light in staged:
        if not p['lod_mask']&1 and p['mesh']not in excluded and rp is not None:
            missing=light[:,3]<.5
            if missing.any():
                if tree is None:tree=cKDTree(rp)
                distance,index=tree.query(v[missing,:3],k=min(8,len(rp)),workers=1)
                if distance.ndim==1:distance=distance[:,None];index=index[:,None]
                similarity=np.einsum('ijk,ik->ij',rn[index],v[missing,3:6])
                scale=max(.25,float(np.linalg.norm(np.ptp(rp,axis=0)))*.05)
                weight=np.maximum(similarity,0)**4/np.maximum(distance,.01)**2
                weight[(similarity<.35)|(distance>scale)]=0
                total=weight.sum(1);ok=total>1e-12
                weighted=(rg[index,:3]*weight[:,:,None]).sum(1)/np.maximum(total[:,None],1e-12)
                target=np.flatnonzero(missing)[ok];light[target,:3]=weighted[ok];light[target,3]=1;stats['mid_vertices_transferred_from_near']+=len(target)
        stats['sampled_vertices']+=len(light);stats['vertices_with_baked_light']+=int((light[:,3]>.5).sum())
    return staged

def parts_for_chunk(source,units,results,sampler,excluded,stats):
    parts=[]
    for unit in units:
        for p,vertex,indices,light in prepare_unit(source,unit,sampler,excluded,stats):
            for sub,index in enumerate(indices):
                path=source.material(p,sub);v=vertex
                if results[path].get('planar_period'):v=base.world_xz_planar_vertices(v,results[path]['planar_period'])
                for lod in[0,1]:
                    if p['lod_mask']&(1<<lod):parts.append(({'name':p['name'],'lod':lod,'material':path,'baked_light':light},v,index))
    if parts and not any(m['lod']==0 for m,v,i in parts):parts=[(dict(m,lod=0),v,i)for m,v,i in parts]+parts
    if parts and not any(m['lod']==1 for m,v,i in parts):parts=parts+[(dict(m,lod=1),v,i)for m,v,i in parts]
    return parts

def emit(name,parts,results,output):
    near=np.concatenate([v[:,:3]for meta,v,index in parts if meta['lod']==0]);lo,hi=near.min(0),near.max(0);origin=np.array([(lo[0]+hi[0])/2,lo[1],(lo[2]+hi[2])/2],np.float32)
    materials=[];textures=[];images=[];material_indices={}
    for path in sorted({meta['material']for meta,v,index in parts}):
        info=results[path];n=len(textures)
        for role in['base','normal','arm']:
            images.append({'uri':info['paths'][role],'mimeType':'image/png'});textures.append({'source':len(images)-1,'sampler':0})
        material={'name':info['name'],'pbrMetallicRoughness':{'baseColorFactor':[1,1,1,1],'metallicFactor':1,'roughnessFactor':1,'baseColorTexture':{'index':n},'metallicRoughnessTexture':{'index':n+2}},'normalTexture':{'index':n+1},'occlusionTexture':{'index':n+2}}
        if info.get('double_sided'):material['doubleSided']=True
        if info.get('emissive_factor'):
            images.append({'uri':info['paths']['emissive'],'mimeType':'image/png'});textures.append({'source':len(images)-1,'sampler':0});factor=info['emissive_factor'];strength=max(factor)
            material.update(emissiveTexture={'index':len(textures)-1},emissiveFactor=[v/strength for v in factor]);material.setdefault('extensions',{})['KHR_materials_emissive_strength']={'emissiveStrength':strength}
        material_indices[path]=len(materials);materials.append(material)
    output.mkdir(parents=True,exist_ok=True);files=[];triangles=[]
    for lod,label in[(0,'near'),(1,'mid')]:
        writer=Writer(*copy.deepcopy((materials,textures,images)));groups=collections.defaultdict(lambda:[[],[],[],[],[]]);counts=collections.Counter()
        if any(m.get('emissiveTexture')for m in materials):writer.g['extensionsUsed']=['KHR_materials_emissive_strength']
        writer.g['asset']['extras']={'source_baked_lighting':'Unity original directional EXR lightmaps, MixedIndirectOnly sun','attribute':'_VALWOOD_BAKED_LIGHT','radiance_space':'linear','native_direct_diffuse_sun_required':True}
        for meta,vertex,index in parts:
            if meta['lod']!=lod:continue
            used,idx=np.unique(index,return_inverse=True);v=vertex[used];p=v[:,:3]-origin;n=v[:,3:6].copy();tex=v[:,6:8].copy();light=meta['baked_light'][used]
            p[:,2]*=-1;n[:,2]*=-1;tex[:,1]=1-tex[:,1];idx=idx.reshape(-1,3);n/=np.maximum(np.linalg.norm(n,axis=1,keepdims=True),1e-8)
            cross=np.cross(p[idx[:,1]]-p[idx[:,0]],p[idx[:,2]]-p[idx[:,0]]);valid=np.linalg.norm(cross,axis=1)>1e-9
            if valid.any()and np.median((cross*n[idx].sum(1)).sum(1)[valid])<0:idx=idx[:,[0,2,1]]
            mat=material_indices[meta['material']];group=groups[mat]
            for slot,array in enumerate([p,n,tex,idx+counts[mat],light]):group[slot].append(array)
            counts[mat]+=len(p)
        total=0
        for mat,group in groups.items():
            arrays=[np.concatenate(a)for a in group];writer.mesh(name+'_'+str(mat),*arrays[:4],mat)
            writer.g['meshes'][-1]['primitives'][0]['attributes']['_VALWOOD_BAKED_LIGHT']=writer.array(arrays[4],'VEC4',34962);total+=len(arrays[3])
        path=output/(name+'_'+label+'.glb');writer.save(path,embedded=True)
        if path.stat().st_size>128*1024**2:raise ValueError('Baked GI GLB exceeds native byte budget: '+name)
        files.append(str(path));triangles.append(total)
    return {'name':name,'origin':origin.tolist(),'dimensions':(hi-lo).tolist(),'files':files,'triangles':triangles,'baked_light_attribute':'_VALWOOD_BAKED_LIGHT'}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase_gi');parser.add_argument('--source',type=Path,default=base.DEFAULT_SOURCE);parser.add_argument('--base-materials',type=Path,default=base.DEFAULT_OUTPUT/'materials.json');parser.add_argument('--rock-cell',type=int,default=128);parser.add_argument('--max-chunks',type=int,default=120);args=parser.parse_args()
    args.arch_cell=64;args.exclude_group=[];args.group=[];args.max_triangles=850000;args.max_vertices=850000;args.geometry_stride=48
    args.output.mkdir(exist_ok=True);source=base.Source(args.source);base.source=source;units,promotions=base.logical_units(source,args)
    results={path:repair_material_alpha(path,info)for path,info in json.loads(args.base_materials.read_text()).items()if info['status']=='candidate'}
    chunks=base.partition(units,args,results);base.install_deduplicated_writer(results);sampler=LightmapSampler();excluded=bad_uv1_hashes(source);stats=collections.Counter();catalog=[]
    print('GI PLAN',len(chunks),'chunks;',len(excluded),'source mesh hashes excluded due UV1 mismatch',flush=True)
    for index,(name,units)in enumerate(chunks):
        parts=parts_for_chunk(source,units,results,sampler,excluded,stats);record=emit(name,parts,results,args.output/'chunks');record.update(source_groups=dict(collections.Counter(u['key'][0]for u in units)),source_placements=sum(len(u['placements'])for u in units));align(record);catalog.append(record);base.atomic_json(args.output/'catalog.json',catalog);print('GI CHUNK',index+1,'/',len(chunks),name,record['triangles'],flush=True)
    base.atomic_json(args.output/'report.json',{'chunks':len(catalog),'source':str(args.source),'source_materials':str(args.base_materials),'native_attribute':'_VALWOOD_BAKED_LIGHT','stats':dict(stats),'excluded_uv1_meshes':sorted(excluded),'lighting_mode':'Mixed sun IndirectOnly; native direct diffuse sun remains; baked local fixtures already contribute','mid_lod_transfer':'8-nearest original near surface samples, normal/distance weighted; original source irradiance retained','source_initial_catalog_unchanged':True})
    base.atomic_json(args.output/'READY.json',{'catalog':str(args.output/'catalog.json'),'chunks':len(catalog),'catalog_sha256':hashlib.sha256((args.output/'catalog.json').read_bytes()).hexdigest(),'stats':dict(stats)})
    print('GI READY',len(catalog),'chunks',dict(stats),flush=True)
if __name__=='__main__':main()
