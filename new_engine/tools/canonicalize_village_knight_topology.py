#!/usr/bin/env python3
"""Fix pose-dependent knight tessellation by changing index bytes only.

Vertex ordering/UVs are already identical. Some source quads choose another
diagonal during each posed export. Preserve one authored sampled topology for
all poses; verify identical oriented boundaries and signed UV area, bounded
surface-area change, and no additional collapsed triangles before publication.
"""
from pathlib import Path
from collections import Counter
import argparse,hashlib,json,os,shutil,struct,sys
import numpy as np
from audit_village_pose_compatibility import ROOT,MODELS,glb,values

def sha(data):return hashlib.sha256(data).hexdigest()
def boundary(triangles):
    edges=Counter()
    for tri in triangles:
        for a,b in zip(map(int,tri),map(int,[tri[1],tri[2],tri[0]])):
            if edges[b,a]:edges[b,a]-=1
            else:edges[a,b]+=1
    return {edge:count for edge,count in edges.items() if count}
def canonicalize_knight_poses(directory,records,publish=True,backup=None):
    directory=Path(directory);selected=[r for r in records if r['role']=='knight']
    if not selected:return []
    reference=directory/'village_character_knight_walk_00.glb';base_doc,base_bin=glb(reference)
    base=[p for n in base_doc['nodes'] if 'mesh' in n for p in base_doc['meshes'][n['mesh']]['primitives']]
    canonical=[values(base_doc,base_bin,p['indices']) for p in base];staged=[];proof=[]
    for record in selected:
        path=directory/record['file'];raw=path.read_bytes();doc,binary=glb(path);changed=bytearray(raw)
        primitives=[p for n in doc['nodes'] if 'mesh' in n for p in doc['meshes'][n['mesh']]['primitives']]
        assert len(primitives)==len(base)
        json_size=struct.unpack_from('<I',raw,12)[0];bin_start=28+json_size;ranges=[];reports=[]
        for primitive,base_primitive,new in zip(primitives,base,canonical):
            old=values(doc,binary,primitive['indices']);attrs=primitive['attributes'];source_attrs=base_primitive['attributes']
            assert old.shape==new.shape and attrs.keys()==source_attrs.keys()
            assert np.array_equal(values(doc,binary,attrs['TEXCOORD_0']),values(base_doc,base_bin,source_attrs['TEXCOORD_0']))
            assert values(doc,binary,attrs['POSITION']).shape==values(base_doc,base_bin,source_attrs['POSITION']).shape
            assert doc['materials'][primitive['material']]==base_doc['materials'][base_primitive['material']]
            xyz=values(doc,binary,attrs['POSITION']);uv=values(doc,binary,attrs['TEXCOORD_0']);old_tri=old.reshape(-1,3);new_tri=new.reshape(-1,3)
            assert boundary(old_tri)==boundary(new_tri),'Canonicalization would change the surface boundary'
            def area(points,tri):return np.linalg.norm(np.cross(points[tri[:,1]]-points[tri[:,0]],points[tri[:,2]]-points[tri[:,0]]),axis=1)*.5
            old_area,new_area=area(xyz,old_tri),area(xyz,new_tri)
            assert np.array_equal(old_area<1e-12,new_area<1e-12),'Canonicalization adds/relocates a collapsed triangle'
            relative=abs(float(new_area.sum()/old_area.sum()-1.));assert relative<.001
            def uv_area(tri):return np.cross(uv[tri[:,1]]-uv[tri[:,0]],uv[tri[:,2]]-uv[tri[:,0]])*.5
            assert abs(float(uv_area(old_tri).sum()-uv_area(new_tri).sum()))<1e-7
            accessor=doc['accessors'][primitive['indices']];view=doc['bufferViews'][accessor['bufferView']]
            assert not view.get('byteStride') and not accessor.get('sparse') and old.dtype==new.dtype
            offset=bin_start+view.get('byteOffset',0)+accessor.get('byteOffset',0);encoded=new.tobytes()
            assert len(encoded)==old.nbytes and int(old.min())==int(new.min()) and int(old.max())==int(new.max())
            changed[offset:offset+len(encoded)]=encoded;ranges.append((offset,offset+len(encoded)))
            reports.append(dict(index_elements=int(new.size),changed_elements=int(np.sum(old!=new)),canonical_index_sha256=sha(encoded),oriented_boundary_exact=True,signed_uv_area_exact=True,additional_degenerate_triangles=0,relative_surface_area_difference=relative))
        cursor=0
        for first,last in sorted(ranges):assert changed[cursor:first]==raw[cursor:first];cursor=last
        assert changed[cursor:]==raw[cursor:] and len(changed)==len(raw)
        before=sha(raw);after=sha(changed)
        proof.append(dict(file=path.name,before_sha256=before,after_sha256=after,bytes=len(raw),only_index_byte_ranges_changed=True,all_vertex_normal_uv_image_and_metadata_bytes_exact=True,primitives=reports))
        staged.append((path,bytes(changed)))
        record.update(sha256=after,bytes=len(changed),canonical_topology=dict(reference=reference.name,index_sha256=[r['canonical_index_sha256'] for r in reports],method='Fixed reference sampled tessellation; only index bytes replaced; oriented boundaries/UV area exact; all other bytes unchanged'))
    if publish:
        if backup:
            backup=Path(backup);backup.mkdir(parents=True,exist_ok=True)
            for path,_ in staged:
                if not (backup/path.name).exists():shutil.copy2(path,backup/path.name)
        for path,data in staged:
            if sha(path.read_bytes())==sha(data):continue
            temporary=path.with_suffix('.glb.pending');temporary.write_bytes(data);os.replace(temporary,path)
    return proof

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--publish',action='store_true');parser.add_argument('--directory',type=Path,default=MODELS);args=parser.parse_args()
    manifest_path=args.directory/'village_character_manifest.json';manifest=json.loads(manifest_path.read_text())
    backup=ROOT/'assets/characters/knight-topology-before'
    if args.publish:
        backup.mkdir(parents=True,exist_ok=True)
        if not (backup/manifest_path.name).exists():shutil.copy2(manifest_path,backup/manifest_path.name)
    proof=canonicalize_knight_poses(args.directory,manifest['assets'],args.publish,backup if args.publish else None)
    if args.publish:
        manifest['total_bytes']=sum(r['bytes'] for r in manifest['assets']);pending=manifest_path.with_suffix('.json.pending');pending.write_text(json.dumps(manifest,indent=2)+'\n');os.replace(pending,manifest_path)
    report=dict(schema=1,published=args.publish,source_downloads_modified=False,texture_bytes_modified=False,pose_count=len(proof),reference='village_character_knight_walk_00.glb',records=proof)
    (ROOT/'assets/characters/knight-topology-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print('KNIGHT_CANONICAL_TOPOLOGY',len(proof),'poses; published',args.publish,'; max surface-area delta',max(p['relative_surface_area_difference'] for r in proof for p in r['primitives']),'; only index bytes changed',flush=True)

if __name__=='__main__':main()
