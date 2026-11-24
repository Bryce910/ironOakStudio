#!/usr/bin/env python3
"""Conform purchased Thomas garments to the runtime's immutable skeletal bind.

Geometry is fitted offline using anatomical segment frames. Source face, UVs,
materials and topology are retained; the five source spine segments and cloth
helpers are reduced to the runtime's anatomical joints. No downloaded code runs.
"""
from pathlib import Path
import copy, hashlib, importlib.util, json, re, sys
import numpy as np

N=Path(__file__).resolve().parents[1]
E=N.parents[1]
I=E/'assets/characters/incoming/thomas_kubold/inspection'
B=E/'build/camp-atmosphere-2026-11-20'
sys.path.insert(0,str(E/'tools'))
import finalize_modular_knight_glb as glb
spec=importlib.util.spec_from_file_location('compact_body',E/'build/plain-father-2026-11-18/tools/build_story_hero.py')
old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def unit(a):
    a=np.array(a,dtype=float);return a/max(np.linalg.norm(a),1e-12)
def frame(direction,secondary=(0,0,1)):
    y=unit(direction);x=np.cross(y,secondary)
    if np.linalg.norm(x)<.01:x=np.cross(y,(1,0,0))
    x=unit(x);return np.stack([x,y,unit(np.cross(x,y))],axis=1)
def acc(d,b,a,kind,comp=5126):
    a=np.ascontiguousarray(a,dtype={5126:'<f4',5123:'<u2',5125:'<u4'}[comp])
    if a.ndim==1:a=a[:,None]
    i=glb.append_accessor(d,b,a,{'componentType':comp,'type':kind})
    if kind=='VEC3':d['accessors'][i].update(min=a.min(0).tolist(),max=a.max(0).tolist())
    return i

def main():
    geo=json.loads((I/'thomas_geometry.json').read_text());raw=np.load(I/'thomas_geometry.npz')
    d,b,_=glb.load(B/'motion/rigs/sword.glb');d.pop('animations',None)
    original_primitives=copy.deepcopy(d['meshes'][0]['primitives'])
    palette={d['nodes'][n]['name']:i for i,n in enumerate(d['skins'][0]['joints'])}
    ib=glb.array(d,b,d['skins'][0]['inverseBindMatrices']).reshape(-1,4,4).transpose(0,2,1)
    target={name:np.linalg.inv(ib[i])[:3,3].astype(float) for name,i in palette.items()}
    bones=geo['bones'];si={v['name']:i for i,v in enumerate(bones)}
    source={v['name']:np.array(v['head'],float) for v in bones}
    bone_mapping={};rotations={};origins={}
    # Landmarks prevent five-to-three spine reduction collapsing the torso.
    sk=['pelvis','spine_03','spine_04','spine_05','neck_01','head']
    tk=['pelvis','spine_01','spine_02','spine_03','neck_01','head']
    sy=np.array([source[n][1] for n in sk]);tp=np.stack([target[n] for n in tk])
    def torso_point(p):return np.array([p[0],*[0,0]])+np.array([0,np.interp(p[1],sy,tp[:,1]),np.interp(p[1],sy,tp[:,2])])+np.array([0,0,p[2]-np.interp(p[1],sy,[source[n][2] for n in sk])])
    torso_map={'spine_01':'pelvis','spine_02':'spine_01','spine_03':'spine_01','spine_04':'spine_02','spine_05':'spine_03','neck_02':'neck_01'}
    child={}
    for side in ['l','r']:
        for a,c in [('clavicle','upperarm'),('upperarm','lowerarm'),('lowerarm','hand'),('thigh','calf'),('calf','foot'),('foot','ball')]:child[a+'_'+side]=c+'_'+side
        child['hand_'+side]='middle_01_'+side
        for digit in ['thumb','index','middle','ring','pinky']:
            for k in [1,2]:child[f'{digit}_{k:02}_{side}']=f'{digit}_{k+1:02}_{side}'
    def resolve(name):
        if name in bone_mapping:return
        ix=si[name];bone=bones[ix];parent=bones[bone['parent']]['name'] if bone['parent']>=0 else None
        if name in torso_map or name in ['pelvis','neck_01','head']:
            mapped=torso_map.get(name,name);R=np.eye(3)
            origin=torso_point(source[name]) if name!='head' else target['head']
        elif name in palette and name in child and child[name] in source and child[name] in target:
            end=child[name]
            second=np.array([0,0,1.])
            if name.startswith('foot_'):second=np.array([0,1.,0])
            if name.startswith('hand_'):
                side=name[-1];sv=source['index_01_'+side]-source['pinky_01_'+side];tv=target['index_01_'+side]-target['pinky_01_'+side]
                R=frame(target[end]-target[name],tv)@frame(source[end]-source[name],sv).T
            else:R=frame(target[end]-target[name],second)@frame(source[end]-source[name],second).T
            mapped=name;origin=target[name]
        elif name in palette and ('_03_' in name or name.startswith('ball_')) and parent:
            resolve(parent);mapped=name;R=rotations[parent];origin=target[name]
        elif 'twist_' in name:
            part=name.split('_twist_')[0];side=name[-1];anchor=part+'_'+side;resolve(anchor)
            R=rotations[anchor];origin=origins[anchor]+R@(source[name]-source[anchor])
            mapped=part+'_twist_01_'+side
            if mapped not in palette:mapped=anchor
        elif '_metacarpal_' in name:
            anchor='hand_'+name[-1];resolve(anchor);mapped=anchor;R=rotations[anchor];origin=origins[anchor]+R@(source[name]-source[anchor])
        elif name in palette:
            mapped=name;R=np.eye(3);origin=target[name]
        elif parent:
            resolve(parent);mapped=bone_mapping[parent];R=rotations[parent];origin=origins[parent]+R@(source[name]-source[parent])
        else:mapped='root';R=np.eye(3);origin=target['root']
        bone_mapping[name]=mapped;rotations[name]=R;origins[name]=origin
    for name in si:resolve(name)
    rr=np.stack([rotations[v['name']] for v in bones]);tt=np.stack([origins[v['name']]-rotations[v['name']]@source[v['name']] for v in bones])
    mapped_palette=np.array([palette[bone_mapping[v['name']]] for v in bones],np.uint16)
    # Converter records refer to files derived from the actual Unity material.
    material_doc=json.loads((I/'materials_converted.json').read_text())
    material_rows=material_doc.get('materials',material_doc)
    material_cache={};image_cache={}
    def material(name):
        if name in material_cache:return material_cache[name]
        row=material_rows[name]
        if 'gltf' in row:mat=copy.deepcopy(row['gltf'])
        elif 'material' in row:mat=copy.deepcopy(row['material'])
        else:
            mat={'pbrMetallicRoughness':{key:copy.deepcopy(row[key]) for key in ['baseColorFactor','metallicFactor','roughnessFactor'] if key in row}}
            for key in ['alphaMode','alphaCutoff','doubleSided']:
                if key in row:mat[key]=row[key]
        for pathkey,texturekey in [('baseColor','baseColorTexture'),('orm','metallicRoughnessTexture'),('normal','normalTexture')]:
            value=row.get(pathkey) or row.get('textures',{}).get(pathkey)
            if not value:continue
            p=Path(value if isinstance(value,str) else value['path'])
            if not p.is_absolute():p=I/p
            payload=p.read_bytes();digest=hashlib.sha256(payload).hexdigest()
            if digest not in image_cache:
                ix=len(d['images']);d['images'].append({'name':p.name,'bufferView':glb.append_view(d,b,payload),'mimeType':'image/png' if payload[:4]==b'\x89PNG' else 'image/jpeg'})
                tx=len(d['textures']);d['textures'].append({'source':ix,'sampler':0});image_cache[digest]=tx
            tx=image_cache[digest]
            container=mat.setdefault('pbrMetallicRoughness',{}) if pathkey!='normal' else mat
            container.setdefault(texturekey,{})['index']=tx
            if pathkey=='orm':mat['occlusionTexture']={'index':tx}
        mat['name']='Thomas_'+name
        ix=len(d['materials']);d['materials'].append(mat);material_cache[name]=ix;return ix
    primitives=[];fp=[];head=[];report=[]
    for group in geo['groups']:
        if group['material'] in ['eyeShadow_M','eyeTear_M']:continue
        pre=group['prefix'];p=raw[pre+'_positions'].astype(float);norm=raw[pre+'_normals'].astype(float);uv=raw[pre+'_uvs'];j=raw[pre+'_joints'];w=raw[pre+'_weights'];tri=raw[pre+'_indices'].reshape(-1,3)
        transformed=np.einsum('nkij,nj->nki',rr[j],p)+tt[j]
        p=np.sum(transformed*w[:,:,None],axis=1)
        norm=np.sum(np.einsum('nkij,nj->nki',rr[j],norm)*w[:,:,None],axis=1);norm/=np.maximum(np.linalg.norm(norm,axis=1)[:,None],1e-12)
        # Merge identical mapped influences before the runtime four-weight cap.
        mj=mapped_palette[j];ww=np.zeros((len(p),len(palette)),np.float32)
        for k in range(w.shape[1]):np.add.at(ww,(np.arange(len(p)),mj[:,k]),w[:,k])
        order=np.argsort(-ww,axis=1)[:,:4];weights=np.take_along_axis(ww,order,axis=1);dropped=1-weights.sum(1);weights/=weights.sum(1)[:,None]
        # FBX carries triangulation duplicates with sub-micron positions and
        # tiny normal roundoff. Weld only matching UV/skin/basis corners; keep
        # the first original attribute values rather than quantizing output.
        signature=np.c_[np.round(p,6),np.round(norm,5),np.round(uv,6),order,np.round(weights,6)]
        _,first,inverse=np.unique(signature,axis=0,return_index=True,return_inverse=True)
        p=p[first];norm=norm[first];uv=uv[first];order=order[first];weights=weights[first];tri=inverse[tri]
        # First-person sleeves are semantic triangle subsets, excluding torso.
        groups=[tri];armset=[v for name,v in palette.items() if any(s in name for s in ['upperarm','lowerarm','hand','thumb','index','middle','ring','pinky'])]
        is_sleeve=group['mesh']=='Tom_Jacket'
        if is_sleeve:
            aw=np.sum(weights*np.isin(order,armset),axis=1);mask=np.mean(aw[tri],axis=1)>.55
            distal=[v for name,v in palette.items() if any(s in name for s in ['lowerarm','hand','thumb','index','middle','ring','pinky'])]
            dw=np.sum(weights*np.isin(order,distal),axis=1)
            forearm=mask&(np.mean(dw[tri],axis=1)>.65)&(np.sum(dw[tri]>.5,axis=1)>=2)
            groups=[tri[~mask],tri[mask&~forearm],tri[forearm]]
        for k,tris in enumerate(groups):
            if not len(tris):continue
            used,remap=np.unique(tris.reshape(-1),return_inverse=True)
            attrs={'POSITION':acc(d,b,p[used],'VEC3'),'NORMAL':acc(d,b,norm[used],'VEC3'),'TEXCOORD_0':acc(d,b,uv[used],'VEC2'),'JOINTS_0':acc(d,b,order[used],'VEC4',5123),'WEIGHTS_0':acc(d,b,weights[used],'VEC4')}
            ix=len(primitives);primitives.append({'attributes':attrs,'indices':acc(d,b,remap,'SCALAR',5125),'material':material(group['material']),'mode':4})
            if group['mesh']=='Male_Arm_3' or is_sleeve and k==2:fp.append(ix)
            if group['mesh'] in ['Thomas_Head_Optimized','Tom_Hair']:head.append(ix)
        report.append(dict(mesh=group['mesh'],material=group['material'],vertices=len(p),triangles=len(tri),bounds=[p.min(0).tolist(),p.max(0).tolist()],max_discarded_weight=float(dropped.max())))
    assert fp and head
    d['meshes'][0]['primitives']=primitives;d['meshes'][0]['name']='Thomas_Civilian'
    out=N/'assets/story_hero';(out/'civilian').mkdir(parents=True,exist_ok=True)
    cd,cb=old.compact(copy.deepcopy(d),bytearray(b));(out/'civilian/body.glb').write_bytes(glb.encode(cd,cb));(out/'civilian/appearance.cfg').write_text('VALWOOD_KNIGHT_APPEARANCE 1\nbody "body.glb"\nfirst_person '+str(len(fp))+' '+' '.join(map(str,fp))+'\nend\n')
    # Preserve the new man's face and hair when his existing knight armor equips.
    ad=copy.deepcopy(d);ad['meshes'][0]['primitives']=original_primitives[:15]+[copy.deepcopy(primitives[i]) for i in head];ad['meshes'][0]['name']='Thomas_Armored'
    ad,ab=old.compact(ad,bytearray(b))
    if (out/'armored').is_symlink():(out/'armored').unlink()
    (out/'armored').mkdir(parents=True,exist_ok=True);(out/'armored/body.glb').write_bytes(glb.encode(ad,ab));(out/'armored/appearance.cfg').write_text('VALWOOD_KNIGHT_APPEARANCE 1\nbody "body.glb"\nfirst_person 3 0 5 9\nend\n')
    imagebytes=sum(cd['bufferViews'][im['bufferView']]['byteLength'] for im in cd['images']);assert imagebytes<25*1024**2,imagebytes
    result=dict(schema='valwood.thomas-body.v1',passed=True,groups=report,first_person=fp,head_primitives=head,bone_mapping=bone_mapping,source=str(I/'thomas_geometry.npz'),source_sha256=sha(I/'thomas_geometry.npz'),canonical_sha256=sha(B/'motion/rigs/sword.glb'),source_fbx_sha256=sha(E/'assets/characters/incoming/thomas_kubold/thomas/Assets/David Grette/Adventure_Pack/Thomas/FBX/Thomas/SK_Thomas.fbx'),embedded_image_bytes=imagebytes,outputs={str(out/f'{s}/body.glb'):sha(out/f'{s}/body.glb') for s in ['civilian','armored']},limitations=['Neutral face only: source facial morphs are preserved in purchased source, not supported by current runtime.','Source cloth and hair helper joints mapped to anatomical parents; Unity cloth physics not ported.','Anatomical fit to existing combat bind preserves runtime equipment and collision contracts.'])
    (out/'THOMAS_BUILD.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k not in ['groups','bone_mapping']},indent=2))
if __name__=='__main__':main()
