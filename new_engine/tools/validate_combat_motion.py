#!/usr/bin/env python3
"""Inspect cooked owned poses independently of Blender and the runtime loader."""
from pathlib import Path
import hashlib,json,math,struct,sys
ROOT=Path(__file__).resolve().parents[1]
MODELS=ROOT/'projects/valwood_world/models'/('longsword_mocap'if '--longsword'in sys.argv else '')
if '--models'in sys.argv:
    MODELS=Path(sys.argv[sys.argv.index('--models')+1]).resolve()
def read(path):
    data=path.read_bytes();magic,version,size=struct.unpack_from('<III',data)
    assert magic==0x46546c67 and version==2 and size==len(data),path
    size,kind=struct.unpack_from('<II',data,12);assert kind==0x4e4f534a
    doc=json.loads(data[20:20+size]);offset=20+size;length,kind=struct.unpack_from('<II',data,offset);assert kind==0x004e4942
    blob=data[offset+8:offset+8+length]
    def accessor(index):
        a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']];components={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];form={5126:'f',5125:'I',5123:'H'}[a['componentType']];stride=v.get('byteStride',components*struct.calcsize(form));start=v.get('byteOffset',0)+a.get('byteOffset',0)
        return [row for i in range(a['count'])for row in [struct.unpack_from('<'+form*components,blob,start+i*stride)]]
    primitives=[p for m in doc['meshes']for p in m['primitives']];assert len(primitives)==1
    p=primitives[0];attributes=p['attributes'];pos=accessor(attributes['POSITION']);uv=accessor(attributes['TEXCOORD_0']);tangent=accessor(attributes['TANGENT']);normal=accessor(attributes['NORMAL']);indices=accessor(p['indices'])
    assert all(math.isfinite(x)for attribute in [pos,uv,tangent,normal]for row in attribute for x in row)
    assert all(0<=i[0]<len(pos)for i in indices)and len(indices)%3==0
    assert all(abs(sum(x*x for x in n)-1)<.002 for n in normal)
    assert all(abs(abs(t[3])-1)<.00001 for t in tangent)
    images=[]
    for image in doc['images']:
        assert 'bufferView'in image;v=doc['bufferViews'][image['bufferView']];images.append(hashlib.sha256(blob[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]).hexdigest())
    assert len(images)==3
    return pos,(uv,indices,[t[3]for t in tangent]),images

def main():
    m=json.loads((MODELS/'combat_motion_manifest.json').read_text());assert m['pose_count']==108 and len(m['assets'])==216
    if 'longsword_mocap'in m:
        rig=m['longsword_mocap'];assert rig['neutral_up_dot']>.9
        for name,info in rig['selected_clips'].items():assert hashlib.sha256((Path(rig['source_directory'])/'Animations/Unity_Skel'/name).read_bytes()).hexdigest()==info['sha256']
        weapon=m['weapon'];assert hashlib.sha256(Path(weapon['file']).read_bytes()).hexdigest()==weapon['sha256']
    source=Path(m['source']);assert hashlib.sha256(source.read_bytes()).hexdigest()==m['source_sha256']
    summaries=[];images=None
    for kind in ['body','arms']:
        base,topology,shared=read(MODELS/f'combat_{kind}_000.glb');images=images or shared;assert images==shared
        floor_min=math.inf;floor_max=-math.inf;movement={}
        for asset in (a for a in m['assets']if a['kind']==kind):
            points,t,im=read(MODELS/asset['file']);assert topology==t and images==im,asset['file']
            floor=min(p[1]for p in points);floor_min=min(floor_min,floor);floor_max=max(floor_max,floor)
            if kind=='body':assert abs(floor)<.00001,asset['file']
            assert len(points)==asset['vertices'] and len(t[1])//3==asset['triangles']
            displacement=max(math.dist(p,q)for p,q in zip(base,points));movement[asset['clip']]=max(movement.get(asset['clip'],0),displacement)
        for key in ['walk','left','right','high','low','thrust','hit','parry']:assert movement[key]>.025,(kind,key,movement[key])
        # Every authored attack starts and finishes at the common neutral.
        # This specifically detects a thrust wrist snapping at its endpoints.
        for clip in m['clips']:
            if clip['name']not in ['left','right','high','low','thrust']:continue
            for slot in [clip['base'],clip['base']+clip['count']-1]:
                points,_,_=read(MODELS/f'combat_{kind}_{slot:03}.glb');assert max(math.dist(p,q)for p,q in zip(base,points))<.00005,(kind,clip['name'],slot)
        summaries.append(dict(kind=kind,vertices=len(base),floor_range=[floor_min,floor_max],clip_max_displacement_m=movement))
    result=dict(pass_all=True,pose_assets=216,shared_texture_hashes=images,source_sha256=m['source_sha256'],source_ankle_error=m['maximum_stationary_ankle_error_source_m'],geometry=summaries)
    out=Path(sys.argv[sys.argv.index('--report')+1]).resolve()if '--report'in sys.argv else ROOT/'docs/polish/landscape-combat-2026-10-28'/('longsword-asset-validation.json'if '--longsword'in sys.argv else 'combat-asset-validation.json')
    out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
