#!/usr/bin/env python3
"""Offline extension of the owned knight bank; no game, GPU or source writes.

Prepare using ordinary Python, then run build_combat_motion.py in background
Blender with --longsword --character-polish and the same fresh --output path.
"""
from pathlib import Path
import argparse
import copy
import hashlib
import io
import json
import math
import re
import struct

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/sword-guard/v3/visual_longsword_mocap'
DEFAULT=ROOT/'build/character-polish/assets-v1'
EXTRA_CLIPS=[('hit_left',8),('hit_right',8),('hit_high',8),('hit_low',8),('hit_thrust',8),('death_back',24)]
EXTRA_NAMES={name for name,_ in EXTRA_CLIPS}
SOURCES={
 'hit_left':'Hitreact/MCU_am_LongS_Uni_Hitreact_Light_Left_StanceR.FBX',
 'hit_right':'Hitreact/MCU_am_LongS_Uni_Hitreact_Light_Right_StanceR.FBX',
 'hit_high':'Hitreact/MCU_am_LongS_Uni_Hitreact_Light_Front_Mid_StanceR.FBX',
 'hit_low':'Hitreact/MCU_am_LongS_Uni_Hitreact_Light_FrontRight_Low_StanceR.FBX',
 'hit_thrust':'Hitreact/MCU_am_LongS_Uni_Hitreact_Light_Front_Mid_StanceR.FBX',
 'death_back':'Death/MCU_am_LongS_Death_Back_01.FBX'}
DEATH_SECONDS=1.8
HIT_SECONDS=.32
MAPS=('basecolor','normal_opengl','orm')


def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def save_json(path,value):
    path.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')


def read_glb(path):
    raw=path.read_bytes()
    assert struct.unpack_from('<II',raw)==(0x46546c67,2)
    length=struct.unpack_from('<I',raw,12)[0]
    return json.loads(raw[20:20+length]),raw[28+length:]


def view(doc,blob,index):
    item=doc['bufferViews'][index];start=item.get('byteOffset',0)
    return blob[start:start+item['byteLength']]


def attribute(doc,blob,name):
    prim=doc['meshes'][0]['primitives'][0]
    acc=doc['accessors'][prim['indices'] if name=='indices' else prim['attributes'][name]]
    data=view(doc,blob,acc['bufferView'])
    return acc,data[acc.get('byteOffset',0):]


def write_glb(path,doc,blob):
    doc['buffers'][0]['byteLength']=len(blob)
    data=json.dumps(doc,separators=(',',':')).encode();data+=b' '*(-len(data)%4)
    blob=bytes(blob);blob+=b'\0'*(-len(blob)%4)
    path.write_bytes(struct.pack('<III',0x46546c67,2,28+len(data)+len(blob))+
        struct.pack('<II',len(data),0x4e4f534a)+data+struct.pack('<II',len(blob),0x004e4942)+blob)


def prepare(output):
    import numpy as np
    from PIL import Image
    output.mkdir(parents=True,exist_ok=False)
    textures=output/'textures';textures.mkdir()
    source=ROOT/'assets/purchased/shield_knight/source/Textures/Texture set 1'
    provenance={}
    def load(name,mode):
        path=source/('Knight_'+name+'.png')
        with Image.open(path) as image:
            assert image.size==(4096,4096)
            result=image.convert(mode).resize((2048,2048),Image.Resampling.LANCZOS)
        provenance[path.name]={'sha256':sha(path),'size':[4096,4096]}
        return result
    base_doc,base_blob=read_glb(BASE/'combat_body_000.glb')
    base=view(base_doc,base_blob,base_doc['images'][0]['bufferView'])
    with Image.open(io.BytesIO(base))as image:assert image.size==(2048,2048),'Accepted albedo must remain2K'
    (textures/'basecolor.png').write_bytes(base)
    normal=np.asarray(load('Normal','RGB'),dtype=np.float32)/127.5-1.
    normal/=np.maximum(np.linalg.norm(normal,axis=2,keepdims=True),1e-6)
    normal=Image.fromarray(np.uint8(np.clip(np.rint((normal+1.)*127.5),0,255)))
    metal=load('MetallicSmoothness','RGBA')
    orm=Image.merge('RGB',(load('AO','L'),Image.eval(metal.getchannel('A'),lambda x:255-x),metal.getchannel('R')))
    for name,image in [('normal_opengl',normal),('orm',orm)]:image.save(textures/(name+'.png'),compress_level=6)
    save_json(output/'TEXTURE_PROVENANCE.json',dict(schema_version=1,source_maps=provenance,
        derived={name:{'sha256':sha(textures/(name+'.png')),'size':[2048,2048]}for name in MAPS},
        conversion='Accepted2K albedo bytes unchanged; source normal green retained, renormalized; AO/1-smoothness/metal packed RGB. No baked lighting.',
        preserved_albedo={'file':str(BASE/'combat_body_000.glb'),'sha256':hashlib.sha256(base).hexdigest(),'bit_exact':True},
        source_scripts_executed=False,source_modified=False,unique_rgba8_mips_mib=64.))
    print('CHARACTER_POLISH_PREPARED',output,flush=True)


class Extension:
    def __init__(self,output):
        import numpy as np
        import import_village_shield_knight as knight
        import retarget_longsword as retarget
        self.output=output;self.records=[];self.sockets=[];self.preserved=[]
        assert (output/'TEXTURE_PROVENANCE.json').is_file(),'Run --prepare first'
        assert not (output/'combat_motion_manifest.json').exists(),'Refuse existing cooked bank'
        knight.DERIVED=output/'textures';retarget.FILES.update(SOURCES)
        self.images=[(knight.DERIVED/(name+'.png')).read_bytes()for name in MAPS]
        text=(BASE/'world_combat_weapon_sockets.inc').read_text()
        for line in text.splitlines():
            values=[float(v)for v in re.findall(r'(-?\d+\.\d+)f',line)]
            if len(values)==6:self.sockets.append([values[:3],values[3:]])
        assert len(self.sockets)==108
        self.reference_origin=None;self.current_origin=None;self.floor_lift=0
        self.death_source_first=None;self.reference_hip=None
        self.last_wrist_adjustment=0
        self.source_hashes={str(ROOT/'assets/purchased/shield_knight/source/Knight with shield.blend'):sha(ROOT/'assets/purchased/shield_knight/source/Knight with shield.blend')}
        for file in sorted(set(SOURCES.values())):
            path=retarget.ANIM/file;self.source_hashes[str(path)]=sha(path)

    def repack(self,source,destination):
        doc,blob=read_glb(source);doc=copy.deepcopy(doc)
        replacements={im['bufferView']:self.images[i]for i,im in enumerate(doc['images'])}
        for image,data in zip(doc['images'],self.images):
            image['mimeType']='image/jpeg'if data.startswith(b'\xff\xd8')else'image/png'
        result=bytearray()
        for i,item in enumerate(doc['bufferViews']):
            data=replacements.get(i,view(doc,blob,i));result.extend(b'\0'*(-len(result)%4))
            item['byteOffset']=len(result);item['byteLength']=len(data);result.extend(data)
        doc['materials'][0]['name']='Owned knight green gold shared 2K PBR'
        doc.setdefault('extras',{})['character_polish_version']=1
        write_glb(destination,doc,result)

    def preserve(self,slot,entries):
        import numpy as np
        for kind in ('body','arms'):
            source=BASE/f'combat_{kind}_{slot:03}.glb';dest=self.output/source.name
            self.repack(source,dest)
            before,old=read_glb(source);after,new=read_glb(dest)
            for name in ('POSITION','NORMAL','TEXCOORD_0','TANGENT','indices'):
                assert attribute(before,old,name)==attribute(after,new,name),(source,name,'baseline geometry changed')
            acc,data=attribute(after,new,'POSITION');points=np.frombuffer(data,dtype='<f4',count=acc['count']*3).reshape(-1,3)
            index,_=attribute(after,new,'indices')
            entries.append(dict(file=dest.name,clip='preserved',sample=slot,kind=kind,vertices=acc['count'],
                triangles=index['count']//3,bytes=dest.stat().st_size,bounds_min=points.min(0).tolist(),bounds_max=points.max(0).tolist()))
            self.preserved.append(dict(file=dest.name,source_sha256=sha(source),sha256=sha(dest),geometry_bit_exact=True))

    def pose(self,mocap,name,t):
        from mathutils import Matrix,Vector,Quaternion
        import retarget_longsword as retarget
        if name=='death_back':
            src,C,cq,offset,rest,scale=mocap.rigs[retarget.FILES[name]]
            frame=mocap.frame(name,t)
            pose=mocap.sample(name,frame)
            # Preserve root travel in one neutral-feet coordinate system.
            # A real collapse must not be recentered around its hips each frame.
            if self.death_source_first is None:
                mocap.sample(name,mocap.frame(name,0))
                self.death_source_first=src.matrix_world@src.pose.bones['pelvis'].head
                pose=mocap.sample(name,frame)
            import bpy
            bpy.context.scene.frame_set(math.floor(frame),subframe=frame%1);bpy.context.view_layer.update()
            delta=C@((src.matrix_world@src.pose.bones['pelvis'].head)-self.death_source_first)*scale
            for bone in mocap.target.pose.bones:bone.matrix_basis=Matrix.LocRotScale(*pose[bone.name])
            bpy.context.view_layer.update()
            hip=mocap.target.pose.bones['Hips'];matrix=hip.matrix.copy()
            matrix.translation=mocap.target.matrix_world.inverted()@(self.reference_hip+delta)
            hip.matrix=matrix;bpy.context.view_layer.update()
            captured={bone.name:bone.matrix_basis.copy().decompose()for bone in mocap.target.pose.bones}
            # Only blend the beginning; the final source collapse is held.
            return retarget.mix(mocap.neutral,captured,retarget.smooth(t/.10))
        # Start at the visible impact, then play the owned recovery to neutral.
        impact={'hit_left':.24,'hit_right':.24,'hit_high':.22,'hit_low':.23,'hit_thrust':.24}[name]
        phase=impact+(1-impact)*t
        captured=mocap.sample(name,mocap.frame(name,phase))
        if name=='hit_high':
            captured=dict(captured)
            p,q,s=captured['Spine1'];captured['Spine1']=(p,q@Quaternion(Vector((1,0,0)),.11*(1-t)),s)
        return retarget.mix(mocap.neutral,captured,1-retarget.smooth(max(0,(t-.72)/.28)))

    def constrain_grip(self,mocap,ik,arm,world):
        """Keep the captured flinch while making the shared hilt reachable.

        The body-hit sources briefly release a hand. Holding their distant
        wrist unchanged while forcing the other hand onto it overextends the
        target rig. Project only that wrist into the two reachable arm spheres.
        """
        import bpy
        from mathutils import Matrix
        right=world@arm.pose.bones['RightHand'].matrix
        original=right.translation.copy();target=original.copy()
        offset=right.to_3x3()@mocap.grip[0]
        spheres=[]
        for side in ['Right','Left']:
            points=[world@arm.pose.bones[side+name].head for name in ['Arm','ForeArm','Hand']]
            radius=((points[1]-points[0]).length+(points[2]-points[1]).length)*.94
            spheres.append((points[0]-(offset if side=='Left'else offset*0),radius))
        for _ in range(24):
            for center,radius in spheres:
                delta=target-center
                if delta.length>radius:target=center+delta.normalized()*radius
        assert all((target-center).length<radius+.0001 for center,radius in spheres)
        rotation=world.to_quaternion().inverted()@right.to_quaternion()
        ik('RightArm','RightForeArm','RightHand',target)
        wrist=arm.pose.bones['RightHand'];wrist.matrix=Matrix.LocRotScale(wrist.head,rotation,wrist.matrix.to_scale())
        bpy.context.view_layer.update();mocap.constrain_grip(ik)
        self.last_wrist_adjustment=(target-original).length

    def record(self,name,t,slot,points,normals,indices,hand,sword_start,sword_end,arm,world,basis,scale,ankle_error,grip_error):
        import numpy as np
        sword=points[sword_start:sword_end];center=sword.mean(0);axis=np.linalg.eigh(np.cov(sword.T))[1][:,-1];along=(sword-center)@axis
        ends=[sword[along<=along.min()+.008].mean(0),sword[along>=along.max()-.008].mean(0)]
        grip=hand*scale;tip=max(ends,key=lambda v:np.linalg.norm(v-grip));base=grip+(tip-grip)*.18
        assert slot==len(self.sockets);self.sockets.append([base.tolist(),tip.tolist()])
        bones={}
        for label in ['Hips','Spine2','Head','LeftFoot','RightFoot']:
            point=np.asarray(tuple(world@arm.pose.bones[label].head))-self.current_origin;point[2]+=self.floor_lift
            bones[label]=(point@basis.T*scale).tolist()
        lengths=np.linalg.norm(normals,axis=1)
        assert np.isfinite(points).all()and np.isfinite(normals).all()and np.min(lengths)>.5
        self.records.append(dict(clip=name,t=t,slot=slot,bounds_min=points.min(0).tolist(),bounds_max=points.max(0).tolist(),
            bones=bones,source_floor_lift_m=self.floor_lift*scale,ankle_error_m=ankle_error*scale,
            left_grip_error_m=grip_error*scale,sword_length_m=float(np.linalg.norm(tip-base)),
            wrist_reach_adaptation_m=self.last_wrist_adjustment*scale if name!='death_back'else 0))

    def finish(self,manifest):
        import numpy as np
        assert len(self.sockets)==172 and len(self.preserved)==216 and len(self.records)==64
        rows=np.asarray(self.sockets,dtype='<f4');assert np.isfinite(rows).all()and np.max(np.abs(rows))<5
        binary=struct.pack('<8sI',b'VWMOT172',172)+rows.tobytes()
        (self.output/'combat_weapon_sockets.bin').write_bytes(binary)
        lines=['// Generated offline from owned geometry; first108 socket rows preserved.',
            'static constexpr unsigned combat_polish_pose_count=172;',
            'static constexpr float combat_polish_hit_seconds=.32f;',
            'static constexpr float combat_polish_death_seconds=1.8f;',
            'static constexpr float combat_weapon_sockets[172][2][3]={']
        for row in self.sockets:lines.append('    {{'+','.join(f'{v:.9f}f'for v in row[0])+'},{'+','.join(f'{v:.9f}f'for v in row[1])+'}},')
        lines.append('};');(self.output/'world_combat_weapon_sockets.inc').write_text('\n'.join(lines)+'\n')
        for path,digest in self.source_hashes.items():assert sha(path)==digest,'Purchased source changed'
        texture_proof=json.loads((self.output/'TEXTURE_PROVENANCE.json').read_text())
        for name,record in texture_proof['source_maps'].items():
            assert sha(ROOT/'assets/purchased/shield_knight/source/Textures/Texture set 1'/name)==record['sha256']
        for record in self.preserved:assert sha(BASE/record['file'])==record['source_sha256'],'Accepted bank changed'
        manifest.update(character_polish_version=1,hit_seconds=HIT_SECONDS,death_seconds=DEATH_SECONDS,
            source_fps=30,death_source_frame_range=[1,87],death_source_seconds=86/30,
            death_retiming='Full captured2.867s collapse resampled over1.8s; no neutral exit fade; fixed neutral-feet origin.',
            directional_hit_adaptation='Four owned Light flesh-hit FBXs; FrontMid reused for high with .11rad downward Spine1 adaptation.',
            textures=json.loads((self.output/'TEXTURE_PROVENANCE.json').read_text()),
            extension_source_hashes=self.source_hashes,baseline_preservation=self.preserved,
            socket_binary=dict(file='combat_weapon_sockets.bin',sha256=sha(self.output/'combat_weapon_sockets.bin')),
            geometry_measurements=self.records)
        save_json(self.output/'CHARACTER_POLISH_GEOMETRY.json',dict(source_modified=False,source_scripts_executed=False,
            death_seconds=DEATH_SECONDS,hit_seconds=HIT_SECONDS,measurements=self.records))


def validate(output):
    import numpy as np
    from PIL import Image
    manifest=json.loads((output/'combat_motion_manifest.json').read_text())
    assert manifest['pose_count']==172 and len(manifest['assets'])==344
    expected=[(108,'hit_left',8),(116,'hit_right',8),(124,'hit_high',8),(132,'hit_low',8),(140,'hit_thrust',8),(148,'death_back',24)]
    assert [(x['base'],x['name'],x['count'])for x in manifest['clips'][-6:]]==expected
    topology={};image_hashes=None;count=0
    for item in manifest['assets']:
        path=output/item['file'];doc,blob=read_glb(path)
        images=[view(doc,blob,im['bufferView'])for im in doc['images']]
        hashes=[hashlib.sha256(raw).hexdigest()for raw in images]
        if image_hashes is None:image_hashes=hashes
        assert hashes==image_hashes
        for raw in images:
            with Image.open(io.BytesIO(raw))as image:assert image.size==(2048,2048)
        kind=item['kind'];acc,data=attribute(doc,blob,'POSITION');positions=np.frombuffer(data,dtype='<f4',count=acc['count']*3)
        assert np.isfinite(positions).all()
        shape=(attribute(doc,blob,'TEXCOORD_0'),attribute(doc,blob,'indices'))
        if kind not in topology:topology[kind]=shape
        assert shape==topology[kind],(path,'topology/UV mismatch')
        count+=1
    rows=(output/'combat_weapon_sockets.bin').read_bytes();assert len(rows)==12+172*24
    assert struct.unpack_from('<8sI',rows)==(b'VWMOT172',172)
    measurements=manifest['geometry_measurements'];reaction=[x for x in measurements if x['clip']!='death_back'];death=[x for x in measurements if x['clip']=='death_back']
    assert max(x['left_grip_error_m']for x in reaction)<.003
    assert max(x['ankle_error_m']for x in reaction)<.003
    feet_drift=max(float(np.linalg.norm(np.asarray(x['bones'][foot])-np.asarray(
        [q for q in reaction if q['clip']==x['clip']][-1]['bones'][foot])))
        for x in reaction for foot in ['LeftFoot','RightFoot'])
    assert feet_drift<.01,('Cooked reaction foot drift',feet_drift)
    assert death[-1]['bounds_max'][1]<1.05,('collapse did not reach ground',death[-1]['bounds_max'])
    assert death[-1]['bones']['Hips'][1]<.65
    report=dict(passed=True,models=count,poses=172,first108_geometry_bit_exact=True,shared_unique_2k_images=3,
        max_reaction_grip_error_m=max(x['left_grip_error_m']for x in reaction),
        max_reaction_ankle_error_m=max(x['ankle_error_m']for x in reaction),
        max_cooked_reaction_foot_drift_m=feet_drift,
        final_collapse_bounds=death[-1]['bounds_max'],socket_sha256=sha(output/'combat_weapon_sockets.bin'))
    save_json(output/'VALIDATION.json',report);print(json.dumps(report,indent=2))


def release(output):
    assert not(output/'CHARACTER_POLISH.json').exists(),'Refuse an existing immutable release'
    proof=json.loads((output/'VALIDATION.json').read_text());assert proof['passed']is True
    manifest=json.loads((output/'combat_motion_manifest.json').read_text())
    assert manifest['pose_count']==172 and len(manifest['assets'])==344
    textures=output/'textures';archive=output.parent/(output.name+'-texture-inputs')
    assert textures.is_dir()and not archive.exists()
    textures.rename(archive) # Cooker-only inputs are not runtime bank contents.
    names=sorted([x['file']for x in manifest['assets']]+['combat_weapon_sockets.bin',
        'world_combat_weapon_sockets.inc','combat_motion_manifest.json','TEXTURE_PROVENANCE.json',
        'CHARACTER_POLISH_GEOMETRY.json','VALIDATION.json'])
    assert len(names)==350 and {p.name for p in output.iterdir()}==set(names)
    files=[dict(file=name,sha256=sha(output/name))for name in names]
    record=dict(schema='valwood.character-polish-assets',version=1,ready=True,pose_count=172,
        asset_count=344,texture_size=2048,hit_seconds=HIT_SECONDS,death_seconds=DEATH_SECONDS,files=files,
        total_payload_bytes=sum((output/name).stat().st_size for name in names),
        source_scripts_executed=False,source_modified=False)
    save_json(output/'CHARACTER_POLISH.json',record)
    print(json.dumps(dict(marker=str(output/'CHARACTER_POLISH.json'),sha256=sha(output/'CHARACTER_POLISH.json'),
        files=len(files),payload_bytes=record['total_payload_bytes']),indent=2))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=DEFAULT)
    modes=parser.add_mutually_exclusive_group(required=True);modes.add_argument('--prepare',action='store_true');modes.add_argument('--validate',action='store_true');modes.add_argument('--release',action='store_true')
    args=parser.parse_args();(prepare if args.prepare else release if args.release else validate)(args.output.resolve())
