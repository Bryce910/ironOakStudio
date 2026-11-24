#!/usr/bin/env python3
"""Immutable material/contact cleanup of the authored distant city plinth.

Preserves the original publisher architecture, all placement matrices, fields,
and collision. Produces a candidate base catalog; never installs or runs GPU.
"""
from pathlib import Path
import argparse,copy,hashlib,json
import numpy as np
from cook_village_publisher_scene import ROOT,Source,material_writer,C
from repair_city_boundary_materials import GLB,planar_frame

NAME='SM_City_Castle_Backdrop_Plinth'
KEY='BEP_City_Plinth_Tileable_Rock'
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base',type=Path,default=ROOT/'assets/purchased/bep_reforged/city-opening/catalog.json')
    ap.add_argument('--output',type=Path,default=ROOT/'assets/purchased/medieval_village/city-transition-cleanup')
    ap.add_argument('--contact-corrections',type=Path,help='Exact contact audit; bury unsupported authored boulders by0.25m')
    args=ap.parse_args();base=json.loads(args.base.read_text());old=next(a for a in base['assets']if a['name']==NAME)
    if args.output.exists():raise ValueError('Immutable output already exists')
    args.output.mkdir(parents=True);s=Source();new=copy.deepcopy(old);files=[];stats=[];audit=[];corrections={}
    if args.contact_corrections:
        contacts=json.loads(args.contact_corrections.read_text())
        for row in contacts['boulders']:
            if row['minimum_surface_gap']is not None:
                corrections[row['index']]=max(corrections.get(row['index'],-np.inf),row['minimum_surface_gap'])
        corrections={index:-(gap+.25)for index,gap in corrections.items()if gap>.05}
    mats=json.loads((ROOT/'assets/purchased/medieval_village/city-materials/materials-frozen.json').read_text())
    rocky=json.loads((ROOT/'assets/purchased/bep_reforged/countryside-materials/ground-materials.json').read_text())['rocky']
    materials={KEY:dict(rocky,name=KEY),'MI_Statue_Rock':mats['MI_Statue_Rock']}
    for lod,label in [(0,'near'),(1,'mid')]:
        original_uv=[];fingerprints=[];counts=[]
        for rock in base['new_background']['rocks']:
            v,parts,_=s.geometry(rock['source_mesh'],lod)
            if len(parts)!=1 or parts[0]['material']!='MI_Statue_Rock':raise ValueError('Unexpected source rock slots')
            used=np.unique(parts[0]['indices']);uv=v[used,6:8].copy();uv[:,1]=1-uv[:,1];original_uv.append(uv)
            counts.append(len(used))
            uv1=v[used,8:10].copy();uv1[:,1]=1-uv1[:,1];fingerprints.append(uv1)
        uv=np.concatenate(original_uv);uv1=np.concatenate(fingerprints)
        g=GLB(old['files'][lod]);oldparts=g.doc['meshes'][0]['primitives'];p=oldparts[0]
        if len(oldparts)!=2:raise ValueError('Unexpected original plinth primitive count')
        arrays={name:g.read(i)for name,i in p['attributes'].items()};ix=g.read(p['indices']).ravel();prefix=len(arrays['POSITION'])-len(uv)
        if not np.array_equal(ix[:prefix],np.arange(prefix)):raise ValueError('Unexpected generated shell triangle stream')
        if not np.array_equal(arrays['TEXCOORD_1'][prefix:],uv1):raise ValueError('Source boulder order/UV1 fingerprint changed')
        pos=arrays['POSITION'][:prefix].astype(np.float64);pos[:,1]+=old['pivot_offset_native'][1]
        shell_uv,tangent,frame=planar_frame(pos,arrays['NORMAL'][:prefix],float(rocky['source_tile_metres'][0]))
        arrays['TEXCOORD_0'][:prefix]=shell_uv;arrays['TEXCOORD_1'][:prefix]=shell_uv;arrays['TANGENT'][:prefix]=tangent
        arrays['TEXCOORD_0'][prefix:]=uv
        expected_position=arrays['POSITION'].copy();offset=prefix
        for index,count in enumerate(counts):
            if index in corrections:expected_position[offset:offset+count,1]+=corrections[index]
            offset+=count
        arrays['POSITION']=expected_position
        w,mapping=material_writer(materials);primitives=[]
        for start,end,key in [(0,prefix,KEY),(prefix,len(arrays['POSITION']),'MI_Statue_Rock')]:
            attrs={n:w.array(a[start:end],g.doc['accessors'][p['attributes'][n]]['type'],34962)for n,a in arrays.items()}
            primitives.append(dict(attributes=attrs,indices=w.array((ix[start:end]-start).reshape(-1,1),'SCALAR',34963),material=mapping[key]))
        d=oldparts[1];mi=len(w.g['materials']);w.g['materials'].append(copy.deepcopy(g.doc['materials'][d['material']]))
        attrs={n:w.array(g.read(i),g.doc['accessors'][i]['type'],34962)for n,i in d['attributes'].items()}
        primitives.append(dict(attributes=attrs,indices=w.array(g.read(d['indices']),'SCALAR',34963),material=mi))
        path=args.output/'models'/(NAME+'_'+label+'.glb');path.parent.mkdir(exist_ok=True)
        w.g['meshes']=[dict(name=path.stem,primitives=primitives)];w.g['nodes']=[dict(name=path.stem,mesh=0)];w.g['scenes'][0]['nodes']=[0];w.save(path,embedded=True)
        repacked=GLB(path);newparts=repacked.doc['meshes'][0]['primitives'];checks={}
        for attribute in ['POSITION','NORMAL']:
            actual=np.concatenate([repacked.read(q['attributes'][attribute])for q in newparts[:2]])
            expected=expected_position if attribute=='POSITION'else g.read(p['attributes'][attribute])
            checks[attribute+'_authorized_result_byte_exact']=bool(np.array_equal(actual,expected))
        newix=np.concatenate([repacked.read(newparts[0]['indices']).ravel(),repacked.read(newparts[1]['indices']).ravel()+prefix])
        checks['triangle_indices_exact']=bool(np.array_equal(newix,ix))
        checks['original_atlas_UV0_exact']=bool(np.array_equal(repacked.read(newparts[1]['attributes']['TEXCOORD_0']),uv))
        if not all(checks.values()):raise ValueError(('Source geometry preservation failed',checks))
        info=dict(old['stats'][lod],bytes=path.stat().st_size,sha256=sha(path),bounds=[expected_position.min(0).tolist(),expected_position.max(0).tolist()]);files.append(str(path.resolve()));stats.append(info)
        audit.append(dict(lod=label,original_file=old['files'][lod],original_sha256=sha(old['files'][lod]),output=str(path.resolve()),
            shell_vertices=prefix,source_boulders=len(original_uv),frame=frame,**checks))
    new.update(files=files,stats=stats,bounds=stats[0]['bounds'],provenance='Authored full-footprint support with repeating BEP rocky shell; owned source boulders retain original atlas UV0 and unsupported decorative boulders settle into contact')
    new['material_revision']=dict(shell_material=KEY,source_tile_metres=rocky['source_tile_metres'],source_boulders='85 original atlas UV0s; no density scaling',geometry='Original shell, normals and triangles byte exact; explicit decorative boulder vertical corrections only',boulder_vertical_corrections=corrections)
    result=copy.deepcopy(base);result['assets']=[new if a['name']==NAME else a for a in result['assets']]
    revision=result['authored_support_revision'];revision.update(replaces_asset_sha256=hashlib.sha256(json.dumps(old,sort_keys=True,separators=(',',':')).encode()).hexdigest(),
        replaces_model_hashes=[r['sha256']for r in old['stats']],new_model_hashes=[r['sha256']for r in stats],
        source_base_catalog=str(args.base.resolve()),source_base_catalog_sha256=sha(args.base),material_revision=new['material_revision'])
    result['new_background']['support_revision']=copy.deepcopy(revision)
    for index,rock in enumerate(result['new_background']['rocks']):
        rock['uv_density_scale']=1.
        if index in corrections:
            rock['source_native_position'][1]+=corrections[index];rock['contact_vertical_correction']=corrections[index]
    if result['placements']!=base['placements']:raise ValueError('Original placement changed')
    for a,b in zip(base['assets'],result['assets']):
        if a['name']!=NAME and a!=b:raise ValueError('Other asset changed')
    report=dict(status='COOKED_PLINTH_MATERIAL_ONLY',base_catalog=str(args.base.resolve()),base_sha256=sha(args.base),
        candidate_catalog=str((args.output/'catalog.json').resolve()),models=audit,
        all_original_architecture_and_placements_exact=True,fields_and_collision_unchanged=True,
        authored_boulder_contact_corrections=corrections,
        foundation_revision='Existing foreground tileable foundation remains unchanged')
    (args.output/'base-before.json').write_text(json.dumps(base,indent=2)+'\n')
    (args.output/'catalog.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    (args.output/'material-audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

if __name__=='__main__':main()
