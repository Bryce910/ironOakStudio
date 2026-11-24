#!/usr/bin/env python3
"""Preserve the accepted visual bank; replace only authored guard geometry."""
from pathlib import Path
import argparse,copy,hashlib,json,os,re,struct
import numpy as np

def sha(raw):return hashlib.sha256(raw).hexdigest()
def read(path):
    raw=path.read_bytes();assert struct.unpack_from('<II',raw)==(0x46546c67,2)
    size=struct.unpack_from('<I',raw,12)[0]
    return json.loads(raw[20:20+size]),raw[28+size:],raw
def view(doc,blob,index):
    v=doc['bufferViews'][index];start=v.get('byteOffset',0)
    return blob[start:start+v['byteLength']]
def attr(doc,blob,name):
    prim=doc['meshes'][0]['primitives'][0]
    a=doc['accessors'][prim['attributes'][name]]
    return a,view(doc,blob,a['bufferView'])
def save(path,doc,blob):
    js=json.dumps(doc,separators=(',',':')).encode();js+=b' '*(-len(js)%4)
    blob=bytes(blob);blob+=b'\0'*(-len(blob)%4)
    raw=struct.pack('<III',0x46546c67,2,28+len(js)+len(blob))+struct.pack('<II',len(js),0x4e4f534a)+js+struct.pack('<II',len(blob),0x004e4942)+blob
    path.write_bytes(raw);return raw

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--raw',type=Path,required=True);parser.add_argument('--base',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--sockets',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    geometry=json.loads((args.raw/'held_guard_geometry.json').read_text());poses={q['slot']:q for q in geometry['poses']}
    records=[];changed=[];material_hashes=None
    for source in sorted(args.base.glob('combat_*.glb')):
        slot=int(source.stem.split('_')[-1]);dest=args.output/source.name
        old_doc,old_blob,old_raw=read(source)
        old_images=[sha(view(old_doc,old_blob,im['bufferView']))for im in old_doc['images']]
        material_hashes=material_hashes or old_images;assert old_images==material_hashes
        is_guard=72<=slot<92;is_neutral=is_guard and (slot-72)%4==0
        if not is_guard or is_neutral:
            os.link(source,dest);result=old_raw
        else:
            doc=copy.deepcopy(old_doc);blob=bytearray(old_blob)
            raw_doc,raw_blob,_=read(args.raw/source.name)
            # UVs and indices retain the existing fixed topology and atlas.
            assert attr(doc,blob,'TEXCOORD_0')[1]==attr(raw_doc,raw_blob,'TEXCOORD_0')[1]
            prim=doc['meshes'][0]['primitives'][0];raw_prim=raw_doc['meshes'][0]['primitives'][0]
            index=doc['accessors'][prim['indices']];raw_index=raw_doc['accessors'][raw_prim['indices']]
            assert view(doc,blob,index['bufferView'])==view(raw_doc,raw_blob,raw_index['bufferView'])
            for name in ['POSITION','NORMAL','TANGENT']:
                a,b=attr(doc,blob,name);a2,b2=attr(raw_doc,raw_blob,name)
                assert a['count']==a2['count'] and a['type']==a2['type'] and len(b)==len(b2)
                v=doc['bufferViews'][a['bufferView']];start=v.get('byteOffset',0)
                blob[start:start+len(b2)]=b2
                for k in ['min','max']:
                    if k in a2:a[k]=a2[k]
            doc.setdefault('extras',{})['held_guard_derivative']=dict(source_sha256=sha(old_raw),geometry_only=True)
            result=save(dest,doc,blob)
            d,b,_=read(dest);assert d['materials']==old_doc['materials'] and d['images']==old_doc['images']
            assert [sha(view(d,b,im['bufferView']))for im in d['images']]==old_images
            changed.append(source.name)
        records.append(dict(file=source.name,source_sha256=sha(old_raw),sha256=sha(result),changed=result!=old_raw,bytes=len(result)))
    assert len(records)==216 and len(changed)==30
    # Preserve all 88 non-guard socket rows; replace 15 held/intermediate rows.
    socket_source=args.sockets.read_text();rows=[]
    for line in socket_source.splitlines():
        n=[float(v)for v in re.findall(r'(-?\d+\.\d+)f',line)]
        if len(n)==6:rows.append([n[:3],n[3:]])
    assert len(rows)==108
    for slot,q in poses.items():
        if (slot-72)%4:rows[slot]=[q['base'],q['tip']]
    lines=['// Isolated held-guard derivative; non-guard sockets unchanged.','static constexpr float combat_weapon_sockets[108][2][3]={']
    for row in rows:lines.append('    {{'+','.join(f'{v:.9f}f'for v in row[0])+'},{'+','.join(f'{v:.9f}f'for v in row[1])+'}},')
    lines.append('};');socket_out=args.output/'world_combat_weapon_sockets.inc';socket_out.write_text('\n'.join(lines)+'\n')
    max_target=max(q['right_hand_target_error_m']for q in geometry['measurements']);max_grip=max(q['left_hand_grip_error_m']for q in geometry['measurements']);max_ankle=max(q['ankle_error_m']for q in geometry['measurements'])
    assert max_target<.003,(max_target,'unreachable wrist target')
    assert max_grip<.001 and max_ankle<.001
    final={q['clip']:q for q in geometry['poses']if q['sample']==3}
    assert final['guard_left']['tip'][0]>.50 and final['guard_right']['tip'][0]<-.65
    assert final['guard_left']['hand'][1]<1.50 and final['guard_right']['hand'][1]<1.50
    assert all(q['base'][2]>.10 and q['tip'][2]>.10 for q in final.values())
    assert final['guard_high']['base'][1]>1.60 and final['guard_low']['base'][1]<1.30
    report=dict(passed=True,bank=str(args.output.resolve()),base_bank=str(args.base.resolve()),model_count=216,changed_guard_models=changed,unchanged_models=186,neutral_endpoints_bit_exact=True,materials_and_textures_bit_exact=True,topology_and_uv_bit_exact=True,maximum_wrist_target_error_m=max_target,maximum_two_hand_grip_error_m=max_grip,maximum_ankle_error_m=max_ankle,held_guard_poses=final,sockets=dict(path=str(socket_out.resolve()),sha256=sha(socket_out.read_bytes())),models=records)
    (args.output/'HELD_GUARD_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items()if k not in ['models','changed_guard_models']},indent=2))
if __name__=='__main__':main()
