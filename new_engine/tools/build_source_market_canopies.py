#!/usr/bin/env python3
"""Drop-in source-PBR market cloth; no skeleton, runtime or layout changes.

Stage first, then publish after comparison captures. Original generated stalls,
their collision records, and the purchased texture sources remain unchanged.
The retained generated inputs make the small postprocess reproducible without
recooking the complete village. Run this after rebuilding the prototype kit.
"""
from pathlib import Path
import argparse, copy, hashlib, io, json, math, os, shutil, struct, sys
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
MODELS = ROOT/'projects/valwood_world/models'
DEST = ROOT/'assets/purchased/market_canopies'
SOURCE = ROOT/'assets/purchased/medieval_village/prepared'
FAMILIES = {'red': ('T_Tent_1', (.76,.01,.99,.45)),
            'blue': ('T_Tent_2', (.025,.02,.66,.975)),
            'gold': ('T_Tent_3', (.02,.02,.98,.98))}
WOOD = 'T_Tileable_Wood_Trims_01'
sys.path.insert(0, str(ROOT/'tools'))
from audit_village_pose_compatibility import glb, values

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def encoded_glb(doc, binary):
    text = json.dumps(doc, separators=(',', ':')).encode()
    text += b' '*(-len(text)%4); binary = bytes(binary)+b'\0'*(-len(binary)%4)
    return (struct.pack('<III',0x46546c67,2,28+len(text)+len(binary))+
            struct.pack('<II',len(text),0x4e4f534a)+text+
            struct.pack('<II',len(binary),0x004e4942)+binary)

def retain_inputs():
    backup=DEST/'before'; backup.mkdir(parents=True,exist_ok=True)
    for color in FAMILIES:
        for suffix in ['', '_mid']:
            name=f'village_stall_{color}{suffix}.glb'
            if not (backup/name).exists(): shutil.copy2(MODELS/name,backup/name)
    if not (backup/'village_asset_manifest.json').exists():
        shutil.copy2(MODELS/'village_asset_manifest.json',backup/'village_asset_manifest.json')
    return backup

def prepare():
    prepared=DEST/'prepared'; prepared.mkdir(parents=True,exist_ok=True)
    records=[]
    for color,(stem,rect) in FAMILIES.items():
        for channel,size in [('BC',1024),('N',512),('ARM',256)]:
            source=SOURCE/(stem+'_'+channel+'.png'); original=Image.open(source).convert('RGB')
            w,h=original.size; box=tuple(round(v*(w if i%2==0 else h)) for i,v in enumerate(rect))
            pixels=original.crop(box).resize((size,size),Image.Resampling.LANCZOS)
            if channel=='N':
                a=np.asarray(pixels,dtype=np.float64)/127.5-1
                a/=np.maximum(np.linalg.norm(a,axis=2,keepdims=True),1e-5)
                pixels=Image.fromarray(np.uint8(np.clip(np.rint((a+1)*127.5),0,255)))
            target=prepared/f'{color}_{channel}.png'; pixels.save(target,optimize=True)
            records.append(dict(color=color,channel=channel,source=str(source.relative_to(ROOT)),source_sha256=sha(source),
                                source_pixel_crop=list(box),normalized_crop=list(rect),path=str(target.relative_to(ROOT)),
                                sha256=sha(target),dimensions=[size,size]))
    return records

def wood_uv(xyz,normal,index):
    """Point grain along each authored beam/board's longest planar axis."""
    result=np.zeros((len(xyz),2),dtype='<f4'); unseen=set(range(len(index)//3))
    owners={}
    for tri,ids in enumerate(index.reshape(-1,3)):
        for v in ids:owners.setdefault(int(v),[]).append(tri)
    while unseen:
        queue=[unseen.pop()];ids=set()
        while queue:
            t=queue.pop()
            for v in index[t*3:t*3+3]:
                ids.add(int(v))
                for other in owners[int(v)]:
                    if other in unseen:unseen.remove(other);queue.append(other)
        ids=sorted(ids);points=xyz[ids]; spans=np.ptp(points,axis=0)
        axes=np.argsort(spans)[::-1];along,across=int(axes[0]),int(axes[1])
        # Resident source trim sheet: 2m longitudinal repeat, .8m across its
        # three board bands. Grain follows vertical posts and horizontal rails.
        result[ids,0]=(points[:,along]-points[:,along].min())/2.
        result[ids,1]=.025+(points[:,across]-points[:,across].min())/.8
    return result

def cloth(lod):
    nx,nz=(24,12) if lod=='near' else (12,6)
    positions=[];normals=[];uv=[];indices=[]
    def height(x,z):
        arch=math.sin((x+2)*math.pi/4);cross=1-(z/1.5)**2
        return 2.7+.6*arch-.095*cross*arch*arch+.012*math.sin(x*math.pi*3)*cross*arch
    def vertex(x,z):
        y=height(x,z);e=1e-4
        dx=(height(x+e,z)-height(x-e,z))/(2*e);dz=(height(x,z+e)-height(x,z-e))/(2*e)
        n=np.array([-dx,1,-dz]);n/=np.linalg.norm(n)
        positions.append((x,y,z));normals.append(n);uv.append(((x+2)/4,(z+1.5)/3))
    for j in range(nz+1):
        for i in range(nx+1):vertex(-2+4*i/nx,-1.5+3*j/nz)
    for j in range(nz):
        for i in range(nx):
            a=j*(nx+1)+i;b=a+nx+1;indices.extend([a,b,b+1,a,b+1,a+1])
    for sign in [-1,1]:
        start=len(positions)
        for i in range(nx+1):
            x=-2+4*i/nx;top=height(x,1.5*sign)
            hem=.17+.025*math.sin((x+2)*2.3)+.012*math.sin((x+2)*5.1)
            for y,v in [(top,.91),(top-hem,.995)]:
                positions.append((x,y,1.5*sign));normals.append((0,0,sign));uv.append(((x+2)/4,v))
        for i in range(nx):
            a=start+i*2
            face=[a,a+1,a+3,a,a+3,a+2]
            if sign<0:face=[face[k] for k in [0,2,1,3,5,4]]
            indices.extend(face)
    return [np.asarray(v,dtype='<f4') for v in [positions,normals,uv]]+[np.asarray(indices,dtype='<u4')]

def cook_one(original,color,lod,target):
    old,binary=glb(original);assert len(old['nodes'])==1 and set(old['nodes'][0])=={'mesh','name'}
    primitives=old['meshes'][0]['primitives']
    wood=next(p for p in primitives if old['materials'][p['material']]['name']=='wood')
    a=wood['attributes'];xyz=values(old,binary,a['POSITION']);normal=values(old,binary,a['NORMAL']);idx=values(old,binary,wood['indices']).ravel()
    doc=dict(asset=dict(version='2.0',generator='Valwood source-PBR market canopy cook1'),
             buffers=[{}],bufferViews=[],accessors=[],images=[],textures=[],
             samplers=[dict(magFilter=9729,minFilter=9987,wrapS=10497,wrapT=10497),dict(magFilter=9729,minFilter=9987,wrapS=33071,wrapT=33071)],
             materials=[],meshes=[dict(name=old['meshes'][0].get('name','Source market canopy'),primitives=[])],
             nodes=copy.deepcopy(old['nodes']),scenes=[dict(nodes=[0])],scene=0)
    data=bytearray()
    def view(payload,target=None):
        data.extend(b'\0'*(-len(data)%4));v=dict(buffer=0,byteOffset=len(data),byteLength=len(payload))
        if target:v['target']=target
        data.extend(payload);doc['bufferViews'].append(v);return len(doc['bufferViews'])-1
    def accessor(array,kind,position=False):
        array=np.asarray(array);entry=dict(bufferView=view(array.tobytes(),34963 if kind=='SCALAR' else 34962),componentType=5125 if kind=='SCALAR' else 5126,count=len(array),type=kind)
        if position:entry.update(min=array.min(axis=0).tolist(),max=array.max(axis=0).tolist())
        doc['accessors'].append(entry);return len(doc['accessors'])-1
    def texture(path,sampler):
        image=len(doc['images']);doc['images'].append(dict(name=path.stem,mimeType='image/png',bufferView=view(path.read_bytes())))
        doc['textures'].append(dict(source=image,sampler=sampler));return len(doc['textures'])-1
    def material(label,paths,sampler):
        base,n,arm=[texture(path,sampler) for path in paths]
        doc['materials'].append(dict(name=label,alphaMode='OPAQUE',doubleSided=True,
          pbrMetallicRoughness=dict(baseColorTexture=dict(index=base),metallicRoughnessTexture=dict(index=arm),metallicFactor=1,roughnessFactor=1),
          normalTexture=dict(index=n,scale=1),occlusionTexture=dict(index=arm,strength=1)))
    material('wood',[SOURCE/(WOOD+'_'+c+'.png') for c in ['BC','N','ARM']],0)
    material('PurchasedSourceCloth_'+color,[DEST/'prepared'/(color+'_'+c+'.png') for c in ['BC','N','ARM']],1)
    for m,(p,n,u,ind) in enumerate([(xyz,normal,wood_uv(xyz,normal,idx),idx),cloth(lod)]):
        p,n,u=np.asarray(p,dtype='<f4'),np.asarray(n,dtype='<f4'),np.asarray(u,dtype='<f4');ind=np.asarray(ind,dtype='<u4')
        doc['meshes'][0]['primitives'].append(dict(attributes=dict(POSITION=accessor(p,'VEC3',True),NORMAL=accessor(n,'VEC3'),TEXCOORD_0=accessor(u,'VEC2')),indices=accessor(ind,'SCALAR'),material=m))
    doc['buffers'][0]['byteLength']=len(data);target.write_bytes(encoded_glb(doc,data))

def stage():
    backup=retain_inputs();records=prepare();out=DEST/'stage';out.mkdir(exist_ok=True)
    manifest=json.loads((backup/'village_asset_manifest.json').read_text())
    manifest['purchased_market_material_provenance']='assets/purchased/market_canopies/PROVENANCE.json'
    selected={f'village_stall_{c}' for c in FAMILIES}
    for row in manifest['assets']:
        name=row['asset'];target=out/(name+'.glb')
        if row['family'] not in selected:
            if not target.exists():target.symlink_to(MODELS/(name+'.glb'))
            continue
        color=row['family'].rsplit('_',1)[1];cook_one(backup/(name+'.glb'),color,row['lod'],target)
        doc,binary=glb(target);tris=sum(doc['accessors'][p['indices']]['count']//3 for p in doc['meshes'][0]['primitives'])
        row.update(bytes=target.stat().st_size,sha256=sha(target),triangles=tris,materials=2,
                   source='Authored drop-in structure with user-purchased Medieval Village cloth and resident source wood PBR',
                   license='Mixed: authored structure/CC0 base; purchased proprietary material source, no open redistribution claim',
                   source_canopy=dict(recipe=1,color_alias=color,source_cloth=FAMILIES[color][0],cloth_sizes=[1024,512,256],
                     wood_source=WOOD,wood_images_shared_with_purchased_houses=True,structural_geometry_preserved=True,
                     collision_and_countertop_preserved=True,old_triangles=320,old_primitives=3))
    (out/'village_asset_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    provenance=dict(schema=1,purchased_source='User-provided Medieval_Village_ALL.rar and matching source project',originals_modified=False,
        retained_generated_inputs=str(backup.relative_to(ROOT)),textures=records,
        wood_sources=[dict(path=str((SOURCE/(WOOD+'_'+c+'.png')).relative_to(ROOT)),sha256=sha(SOURCE/(WOOD+'_'+c+'.png')))for c in ['BC','N','ARM']],
        adaptation='Authored same-footprint canopy surface and restrained uneven hem; coherent source cloth panel crops, normal renormalization after reduction; no baked illumination, no source subsurface/cloth-simulation claim',
        licensing='Purchased proprietary pixels remain proprietary; no CC0 or redistribution claim for converted stalls')
    (DEST/'PROVENANCE.json').write_text(json.dumps(provenance,indent=2)+'\n')
    validate(out)

def validate(directory):
    from build_castle_village_assets import validate as validate_kit
    backup=DEST/'before';manifest=json.loads((directory/'village_asset_manifest.json').read_text())
    before=json.loads((backup/'village_asset_manifest.json').read_text());old={x['asset']:x for x in before['assets']};proof=[];cloth_images={};wood_hashes={sha(SOURCE/(WOOD+'_'+c+'.png')) for c in ['BC','N','ARM']}
    donor,donor_binary=glb(MODELS/'purchased_village_house_e_near.glb')
    resident=set()
    for image in donor['images']:
        v=donor['bufferViews'][image['bufferView']];resident.add(hashlib.sha256(donor_binary[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]).hexdigest())
    assert wood_hashes<=resident,'Wood must use exact encoded pixels already resident on purchased architecture'
    for row in manifest['assets']:
        if not row.get('source_canopy'):
            assert sha(directory/(row['asset']+'.glb'))==old[row['asset']]['sha256'],'Unselected model changed'
            continue
        path=directory/(row['asset']+'.glb');doc,binary=glb(path);prior,pb=glb(backup/path.name)
        assert sha(path)==row['sha256'] and len(doc['materials'])==2 and len(doc['meshes'][0]['primitives'])==2
        assert row['collision_boxes']==old[row['asset']]['collision_boxes'] and row['maximum_design_dimensions']==old[row['asset']]['maximum_design_dimensions']
        p=doc['meshes'][0]['primitives'][0];q=prior['meshes'][0]['primitives'][0]
        for key in ['POSITION','NORMAL']:
            assert np.array_equal(values(doc,binary,p['attributes'][key]),values(prior,pb,q['attributes'][key]))
        assert np.array_equal(values(doc,binary,p['indices']).ravel(),values(prior,pb,q['indices']).ravel())
        bounds=[]
        for prim in doc['meshes'][0]['primitives']:
            v=values(doc,binary,prim['attributes']['POSITION']);n=values(doc,binary,prim['attributes']['NORMAL']);uv=values(doc,binary,prim['attributes']['TEXCOORD_0']);ind=values(doc,binary,prim['indices']).ravel()
            assert np.isfinite(v).all() and np.isfinite(n).all() and np.isfinite(uv).all()
            assert np.max(abs(np.linalg.norm(n,axis=1)-1))<1e-5
            tri=v[ind.reshape(-1,3)];cross=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]);area=np.linalg.norm(cross,axis=1)
            assert area.min()>1e-8 and np.min(np.sum(cross*n[ind.reshape(-1,3)].mean(axis=1),axis=1))>0
            bounds.append((v.min(axis=0),v.max(axis=0)))
        actual=np.array([np.min([x[0]for x in bounds],axis=0),np.max([x[1]for x in bounds],axis=0)])
        assert np.array_equal(actual,np.asarray(old[row['asset']]['bounds_y_up'],dtype=np.float32)),(path,actual)
        assert row['triangles']<=old[row['asset']]['triangles']+1000
        images=[]
        for i,im in enumerate(doc['images']):
            view=doc['bufferViews'][im['bufferView']];encoded=binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']];h=hashlib.sha256(encoded).hexdigest();images.append(h)
            if i>=3:cloth_images[h]=Image.open(io.BytesIO(encoded)).size
        assert set(images[:3])==wood_hashes
        for m in doc['materials']:
            assert m['alphaMode']=='OPAQUE' and m['doubleSided'] and 'normalTexture'in m and 'occlusionTexture'in m
            assert m['pbrMetallicRoughness'].get('baseColorFactor',[1]*4)==[1]*4
        proof.append(dict(asset=row['asset'],sha256=sha(path),old_sha256=sha(backup/path.name),triangles=row['triangles'],primitives=2,
                          exact_bounds=actual.tolist(),structure_positions_normals_indices_exact=True,collision_boxes_exact=True,cloth_image_hashes=images[3:]))
    memory=sum(w*h*4*4/3 for w,h in cloth_images.values());assert len(proof)==6 and len(cloth_images)==9 and memory<=21*1048576
    for color in FAMILIES:
        a,b=[x for x in proof if x['asset'] in [f'village_stall_{color}',f'village_stall_{color}_mid']]
        assert a['cloth_image_hashes']==b['cloth_image_hashes']
    validate_kit(directory)
    result=dict(schema=1,assets=proof,new_cloth_images=9,new_cloth_rgba8_mips_mib=memory/1048576,
                new_wood_images=0,wood_identical_to_resident_purchased_source=True,characters_changed=False,
                geometry_limit='At most 1,000 additional triangles and 3 primitives per stall; actual 2 primitives',
                limitations='Source cloth subsurface/transmission is not reproduced by these standard opaque PBR assets')
    (DEST/'VALIDATION.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS source market canopy:6 GLBs,2 primitives each,exact structure/bounds/collisions;9 cloth images,21 MiB new;wood dedupes',flush=True)
    return result

def publish():
    stage=DEST/'stage';result=validate(stage)
    for row in result['assets']:
        source=stage/(row['asset']+'.glb');pending=MODELS/(row['asset']+'.glb.pending');shutil.copy2(source,pending);os.replace(pending,MODELS/(row['asset']+'.glb'))
    shutil.copy2(stage/'village_asset_manifest.json',MODELS/'village_asset_manifest.json')
    print('PUBLISHED source market canopy6GLBs; characters and placements unchanged',flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--stage',action='store_true');p.add_argument('--publish',action='store_true');p.add_argument('--validate',action='store_true');a=p.parse_args()
    if a.stage:stage()
    if a.publish:publish()
    if a.validate:validate(MODELS)
