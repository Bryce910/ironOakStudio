#!/usr/bin/env python3
"""Isolated per-instance publisher-GI candidate, reusing frozen source PBR.

Samples original UE UV1 and source-world normals before native coordinate/UV
conversion. Original RGB paint and baked RGBA coexist as separate GLTF streams.
No live project, executable, previous catalog, shader or source image is written.
"""
from pathlib import Path
import argparse,collections,copy,hashlib,json,math,re
import numpy as np
from cook_village_publisher_scene import Source,PRIVATE,ROOT,C,EXCLUDED,KEEP_PREFIX,native_matrix,foliage,recover_ropes,emit
from recover_village_source_lightmaps import LightmapSampler

OUT=ROOT/'assets/purchased/medieval_village/publisher-gi'
BASE=ROOT/'assets/purchased/medieval_village/publisher-source-datum/catalog.json'
MATERIALS=ROOT/'assets/purchased/medieval_village/city-materials/materials-frozen.json'
LIMIT=120*1024**2

class Frozen:
    def __init__(self,path):self.rows=json.loads(Path(path).read_text())
    def get(self,name):return self.rows[name]

class PerInstanceGI:
    def __init__(self,lod=2):
        self.sampler=LightmapSampler(lod=lod);self.bindings={(str(r['component']),r['lod']):r for r in self.sampler.manifest['component_bindings']};self.cache={};self.audit=[]
    def __call__(self,piece,v,row,lod):
        component=piece['component'];m=re.search(r'_LOD(\d+)$',row['name']);effective_lod=int(m[1])if m else 0
        key=(str(component),piece.get('instance_index'),row['vertices'],effective_lod)
        if key in self.cache:return self.cache[key]
        binding=self.bindings.get((str(component),effective_lod));shared=False
        if binding is None and effective_lod:
            binding=self.bindings.get((str(component),0));shared=binding is not None
        if binding is None:rgba=np.zeros((len(v),4),np.float32)
        else:
            world=piece['source_world_unreal'];normal=v[:,3:6]@np.linalg.inv(world[:3,:3]);normal/=np.maximum(np.linalg.norm(normal,axis=1)[:,None],1e-20)
            rgba=self.sampler.sample(binding['guid'],v[:,8:10],normal,instance_index=piece.get('instance_index'))
        self.cache[key]=rgba
        self.audit.append(dict(component=component,instance_index=piece.get('instance_index'),family=piece['family'],requested_lod=lod,geometry_lod=effective_lod,
            guid=binding['guid']if binding else None,shared_lod0_binding=shared,vertices=len(v),valid=int(np.count_nonzero(rgba[:,3])),
            rgb_min=rgba[:,:3].min(0).tolist(),rgb_max=rgba[:,:3].max(0).tolist(),rgb_mean=rgba[:,:3].mean(0).tolist()))
        return rgba

def piece_cost(piece,source,cooker,lod=0):
    v,parts,row=source.geometry(piece['family'],lod,piece.get('variant'));images=set();geometry=512
    overrides=piece['override']
    for part in parts:
        name=part['material']
        if part['slot']<len(overrides)and overrides[part['slot']]:name=overrides[part['slot']]['name']
        info=cooker.get(name)
        for path in [info.get('base'),info.get('normal'),info.get('arm'),info.get('emissive'),*info.get('aux',[])]:
            if path:images.add(path)
        # Extractor has one distinct vertex per original triangle corner.
        # All attributes, bakedRGBA, optional sourceRGB and the index stream.
        per=12+12+8+8+16+16+4+(12 if info.get('facade_mode')in{1,3,4}else 0)
        geometry+=part['indices'].size*per
    return geometry,images

def plan(scene,source,cooker):
    fol,fol_audit=foliage(scene);ropes,rope_audit=recover_ropes(scene,source);groups=collections.defaultdict(list);excluded=[];costs={}
    for record in scene['placements']+fol+ropes:
        mesh=record.get('mesh')
        if not mesh:continue
        family=mesh['name']
        if family in EXCLUDED:excluded.append(dict(component=record['export_index'],mesh=family,reason=EXCLUDED[family]));continue
        identity=str(record['export_index']);component=int(identity.split(':')[0]);instance=int(identity.split(':')[1])if ':'in identity else None
        matrix=native_matrix(record['world_matrix_unreal_cm_column_major']);paint=source.paint['components'].get(str(component),{}).get('variant')
        piece=dict(family=family,variant=paint,matrix=matrix,source_world_unreal=np.asarray(record['world_matrix_unreal_cm_column_major']).reshape(4,4).T,
            override=record['properties'].get('OverrideMaterials',[]),source_component=identity,component=component,instance_index=instance,actor_label=record.get('actor_label'))
        if family=='SM_Ground_Mesh':key='gi_ground';anchor=np.zeros(3)
        else:
            arch=not record.get('foliage')and family.startswith(KEEP_PREFIX)
            distance=np.linalg.norm(matrix[[0,2],3]-[12,0]);far=arch and distance>35;cell=48. if far else 12.
            tile=np.floor(matrix[[0,2],3]/cell).astype(int);kind='arch'if arch else('foliage'if record.get('foliage')else'dressing')
            key=f'gi_{kind}_{int(cell)}_{tile[0]}_{tile[1]}';anchor=np.zeros(3);anchor[[0,2]]=tile*cell+cell/2
        piece['matrix']=matrix.copy();piece['matrix'][:3,3]-=anchor;piece['anchor']=anchor;piece['far']=family!='SM_Ground_Mesh'and arch and distance>35
        groups[key].append(piece)
    result={};estimated=0
    # Split only when actual unique source image bytes + preserved geometry
    # would breach the bounded GLB contract; source surfaces are never reduced.
    for key,pieces in groups.items():
        batch=[];geometry=0;images=set();sub=0
        def finish():
            nonlocal batch,geometry,images,sub,estimated
            if not batch:return
            name=key+('_p'+str(sub)if sub else'');size=geometry+sum(Path(p).stat().st_size for p in images)+256*1024
            far=all(p['far']for p in batch);result[name]=batch;costs[name]=dict(estimated_bytes=size,pieces=len(batch),images=len(images),far_single_lod=far)
            estimated+=size if far else size*2;batch=[];geometry=0;images=set();sub+=1
        for piece in sorted(pieces,key=lambda p:(p['family'],str(p['source_component']))):
            g,im=piece_cost(piece,source,cooker,lod=3 if piece['far']else 0);size=geometry+g+sum(Path(p).stat().st_size for p in images|im)+256*1024
            if batch and size>LIMIT:finish()
            if g+sum(Path(p).stat().st_size for p in im)+256*1024>128*1024**2:raise ValueError(('Single source piece exceeds GLB cap',piece['family']))
            batch.append(piece);geometry+=g;images|=im
        finish()
    return result,dict(groups=costs,estimated_all_bytes=estimated,source_components=sum(len(v)for v in result.values()),foliage=fol_audit,ropes=rope_audit,excluded=excluded)

def collision(pieces,source,miny):
    vertices=[];triangles=[]
    for p in pieces:
        if not p['family'].startswith(KEEP_PREFIX)or p['family']=='SM_Ground_Mesh':continue
        original=source.ucx(p['family'])
        if not original:continue
        v=np.asarray(original['vertices'])@p['matrix'][:3,:3].T+p['matrix'][:3,3];v[:,1]-=miny
        triangles.extend((np.asarray(original['triangles'])+len(vertices)).tolist());vertices.extend(v.tolist())
    return dict(vertices=vertices,triangles=triangles,source='Exact source architecture UCX transformed to the same shifted chunk frame')if vertices else None

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output',type=Path,default=OUT);ap.add_argument('--plan-only',action='store_true');ap.add_argument('--only',nargs='*');ap.add_argument('--mip',type=int,default=2);args=ap.parse_args()
    scene=json.loads((PRIVATE/'publisher-scene.json').read_text());base=json.loads(BASE.read_text());source=Source();cooker=Frozen(MATERIALS);groups,audit=plan(scene,source,cooker)
    library=[copy.deepcopy(a)for a in base['assets']if a.get('library_only')]
    if len(groups)+len(library)>128:raise ValueError(('Native definition cap',len(groups),len(library)))
    args.output.mkdir(parents=True,exist_ok=True);audit.update(asset_count=len(groups)+len(library),source_lightmap_mip=args.mip,base_catalog=str(BASE.resolve()),
        material_manifest_sha256=hashlib.sha256(MATERIALS.read_bytes()).hexdigest())
    (args.output/'plan.json').write_text(json.dumps(audit,indent=2)+'\n');print('PLAN',audit['asset_count'],'assets',audit['source_components'],'sourcecomponents',round(audit['estimated_all_bytes']/2**30,3),'GiB conservative near+MID estimate',flush=True)
    if args.plan_only:return
    sampler=PerInstanceGI(args.mip);assets=library;placements=[];actor_map=[]
    def priority(item):
        name,pieces=item;return (0 if name=='gi_ground'else 1,float(np.linalg.norm(pieces[0]['anchor'][[0,2]]-[12,0])),name)
    for i,(name,pieces)in enumerate(sorted(groups.items(),key=priority)):
        if args.only and name not in args.only:continue
        miny=float('inf');far=all(p['far']for p in pieces);lods=[('far',3)]if far else[('near',0),('mid',1)]
        for p in pieces:
            for _,lod in lods:
                v,_,_=source.geometry(p['family'],lod,p.get('variant'));pos=(v[:,:3]@C.T/100)@p['matrix'][:3,:3].T+p['matrix'][:3,3];miny=min(miny,float(pos[:,1].min()))
        files=[];stats=[]
        for label,lod in lods:
            path=args.output/'models'/(name+'_'+label+'.glb');stats.append(emit(path,pieces,source,lod,cooker,miny,gi_sampler=sampler));files.append(str(path.resolve()))
        if far:files=files*2;stats=stats*2
        assets.append(dict(name=name,files=files,pivot_offset_native=[0,miny,0],bounds=stats[0]['bounds'],collision=collision(pieces,source,miny),stats=stats,
            source_families=sorted({p['family']for p in pieces}),pieces=len(pieces),source_gi=True,far_single_lod=far))
        m=np.eye(4);m[:3,3]=pieces[0]['anchor'];placements.append(dict(asset=name,matrix_native=m.T.flatten().tolist(),spatial_batch=True))
        actor_map.extend(dict(component=p['source_component'],family=p['family'],asset=name,paint_variant=p.get('variant'))for p in pieces)
        print(f'[{i+1}/{len(groups)}] {name} {len(pieces)} pieces, {stats[0]["triangles"]}/{stats[1]["triangles"]} triangles, {stats[0]["bytes"]/2**20:.2f}MiB',flush=True)
    result=dict(schema=1,assets=assets,placements=placements,cameras=base['cameras'],ground_asset='gi_ground',source_actor_map=actor_map,
        coordinate_system=base['coordinate_system'],source_scene=base['source_scene'],source_scene_sha256=base['source_scene_sha256'],
        source_gi_semantics='UE HQ OutDiffuseLighting sampled in original UV1/source-world normals; replace diffuse ambient only; retain dynamic Movable-sun diffuse/specular',
        gi_sample_audit=sampler.audit,paint_binding=source.binding,foliage=audit['foliage'],ropes=audit['ropes'],excluded=audit['excluded'],castle_library_unplaced=base['castle_library_unplaced'],
        previous_catalog=str(BASE.resolve()),previous_catalog_sha256=hashlib.sha256(BASE.read_bytes()).hexdigest(),
        limitations=['Vertex-baked indirect approximates source per-pixel lightmaps on coarse walls; original geometry is retained without wholesale tessellation.',
        'Architecture whose source origin is farther than35m from the central presentation area uses the supplied sourceLOD3 for both native distance tiers, avoiding duplicate HIGH residency.',
        'Samples authored mip'+str(args.mip)+' for bounded CPU/disk usage; the unmodified source retains full-resolution mips.',
        'Missing separate LOD GUIDs reuse source component LOD0 binding with that LOD original UV1; exact explicit LOD GUIDs take precedence.',
        'Indirect-only sun policy is inferred from the map current Movable directional-light mobility; historical bake mobility is not recorded.',
        *base.get('limitations',[])])
    (args.output/'catalog.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n');print('READY',args.output/'catalog.json',len(assets),'assets',len(placements),'placements',flush=True)
if __name__=='__main__':main()
