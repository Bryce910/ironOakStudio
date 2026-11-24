#!/usr/bin/env python3
"""UV/material-only repair of immutable city boundary foundations.

The continuous generated support requires a repeating material. The purchased
Statue Rock atlas remains only on its original source boulders, with their
original atlas UVs. Positions, normals, triangles, collision and poses are
preserved exactly; this tool never writes the installed project.
"""
from pathlib import Path
import argparse,copy,hashlib,json,struct
import numpy as np
from cook_village_publisher_scene import ROOT,Source,material_writer
from build_city_boundary_foundations import Ground,NAME

KEY='BEP_City_Boundary_Tileable_Rock'

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

class GLB:
    def __init__(self,path):
        raw=Path(path).read_bytes();n=struct.unpack_from('<I',raw,12)[0]
        self.doc=json.loads(raw[20:20+n]);self.binary=raw[28+n:]
    def read(self,index):
        a=self.doc['accessors'][index];v=self.doc['bufferViews'][a['bufferView']]
        size={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
        dtype=np.dtype({5126:'<f4',5125:'<u4',5123:'<u2'}[a['componentType']])
        return np.ndarray((a['count'],size),dtype=dtype,buffer=self.binary,
            offset=v.get('byteOffset',0)+a.get('byteOffset',0),
            strides=(v.get('byteStride',size*dtype.itemsize),dtype.itemsize)).copy()

def planar_frame(positions,normals,period=7.):
    """Per-face dominant-plane UVs and exact UV-derivative tangent frame."""
    faces=np.asarray(positions,dtype=np.float64).reshape(-1,3,3)
    normals=np.asarray(normals,dtype=np.float64).reshape(-1,3,3)
    n=normals[:,0];dominant=np.argmax(abs(n),axis=1)
    uv=np.empty((len(faces),3,2),np.float64)
    for axis,projection in [(0,(2,1)),(1,(0,2)),(2,(0,1))]:
        chosen=dominant==axis;uv[chosen]=faces[chosen][:,:,projection]/period
    uv[:,:,1]=1-uv[:,:,1]
    e1=faces[:,1]-faces[:,0];e2=faces[:,2]-faces[:,0]
    a=uv[:,1]-uv[:,0];b=uv[:,2]-uv[:,0];det=a[:,0]*b[:,1]-a[:,1]*b[:,0]
    if np.min(abs(det),initial=1)<1e-12:raise ValueError('Degenerate projected shell UV triangle')
    t=(e1*b[:,1,None]-e2*a[:,1,None])/det[:,None]
    bt=(e2*a[:,0,None]-e1*b[:,0,None])/det[:,None]
    t-=n*np.sum(n*t,axis=1)[:,None];t/=np.linalg.norm(t,axis=1)[:,None]
    w=np.where(np.sum(np.cross(n,t)*bt,axis=1)<0,-1.,1.)
    frame=np.repeat(np.column_stack((t,w)),3,axis=0).astype(np.float32)
    return uv.reshape(-1,2).astype(np.float32),frame,dict(
        triangles=len(faces),minimum_absolute_uv_determinant=float(abs(det).min()),
        maximum_normal_tangent_dot=float(abs(np.sum(n*t,axis=1)).max()),
        minimum_tangent_length=float(np.linalg.norm(t,axis=1).min()),
        valid_handedness=bool(np.all(np.isin(w,[-1,1]))))

def original_boulder_uvs(base,source,lod,ground):
    arrays=[];families=[]
    for i,row in enumerate(base['boundary_foundations']['supports']):
        if not row['triangles']:continue
        points=np.asarray(row['source_footprint']);center=points.mean(0)
        for j in [0,len(points)//2]:
            direction=points[j]-center;direction/=max(np.linalg.norm(direction),1e-6)
            xz=points[j]+direction*2.1
            if ground.road_tree.query(xz)[0]<8:continue
            family=['SM_Rock_01','SM_Rock_02','SM_Rock_03'][(i+j)%3]
            v,parts,_=source.geometry(family,lod)
            if len(parts)!=1 or parts[0]['material']!='MI_Statue_Rock':raise ValueError('Unexpected source rock slots')
            used=np.unique(parts[0]['indices']);uv=v[used,6:8].copy();uv[:,1]=1-uv[:,1]
            uv1=v[used,8:10].copy();uv1[:,1]=1-uv1[:,1]
            arrays.append((uv,uv1));families.append(family)
    return np.concatenate([a[0]for a in arrays]),np.concatenate([a[1]for a in arrays]),families

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base',type=Path,default=ROOT/'assets/purchased/bep_reforged/city-opening/catalog.json')
    ap.add_argument('--output',type=Path,default=ROOT/'assets/purchased/medieval_village/boundary-foundations-tileable')
    args=ap.parse_args();base=json.loads(args.base.read_text());before_hash=sha(args.base)
    old=next(a for a in base['assets']if a['name']==NAME);new=copy.deepcopy(old)
    if args.output.exists():raise ValueError('Immutable output already exists')
    args.output.mkdir(parents=True);source=Source();ground=Ground()
    mats=json.loads((ROOT/'assets/purchased/medieval_village/city-materials/materials-frozen.json').read_text())
    rocky=json.loads((ROOT/'assets/purchased/bep_reforged/countryside-materials/ground-materials.json').read_text())['rocky']
    materials={KEY:dict(rocky,name=KEY),'MI_Statue_Rock':mats['MI_Statue_Rock']}
    files=[];stats=[];audit=[]
    for lod,label in [(0,'near'),(1,'mid')]:
        g=GLB(old['files'][lod]);primitives=g.doc['meshes'][0]['primitives']
        if len(primitives)!=2:raise ValueError('Unexpected foundation source primitive layout')
        p=primitives[0];arrays={name:g.read(index)for name,index in p['attributes'].items()};ix=g.read(p['indices']).ravel()
        uv,uv1,families=original_boulder_uvs(base,source,lod,ground);prefix=len(arrays['POSITION'])-len(uv)
        if prefix%3 or not np.array_equal(ix[:prefix],np.arange(prefix)):raise ValueError('Generated shell prefix is not exact flat triangle stream')
        if not np.array_equal(arrays['TEXCOORD_0'][:prefix],arrays['TEXCOORD_1'][:prefix]):raise ValueError('Unexpected generated UV prefix')
        if not np.array_equal(arrays['TEXCOORD_1'][prefix:],uv1):raise ValueError('Source boulder order/UV1 fingerprint changed')
        pos=arrays['POSITION'][:prefix].astype(np.float64);pos[:,1]+=old['pivot_offset_native'][1]
        shell_uv,tangent,frame_audit=planar_frame(pos,arrays['NORMAL'][:prefix],float(rocky['source_tile_metres'][0]))
        arrays['TEXCOORD_0'][:prefix]=shell_uv;arrays['TEXCOORD_1'][:prefix]=shell_uv;arrays['TANGENT'][:prefix]=tangent
        arrays['TEXCOORD_0'][prefix:]=uv
        w,mapping=material_writer(materials);outprimitives=[]
        for start,end,mat in [(0,prefix,KEY),(prefix,len(arrays['POSITION']),'MI_Statue_Rock')]:
            attrs={name:w.array(a[start:end],g.doc['accessors'][p['attributes'][name]]['type'],34962)for name,a in arrays.items()}
            indices=ix[start:end]-start
            outprimitives.append(dict(attributes=attrs,indices=w.array(indices.reshape(-1,1),'SCALAR',34963),material=mapping[mat]))
        datum=primitives[1];datum_material=len(w.g['materials']);w.g['materials'].append(copy.deepcopy(g.doc['materials'][datum['material']]))
        attrs={name:w.array(g.read(index),g.doc['accessors'][index]['type'],34962)for name,index in datum['attributes'].items()}
        outprimitives.append(dict(attributes=attrs,indices=w.array(g.read(datum['indices']),'SCALAR',34963),material=datum_material))
        path=args.output/'models'/(NAME+'_'+label+'.glb');path.parent.mkdir(exist_ok=True)
        w.g['meshes']=[dict(name=path.stem,primitives=outprimitives)];w.g['nodes']=[dict(name=path.stem,mesh=0)];w.g['scenes'][0]['nodes']=[0];w.save(path,embedded=True)
        repaired=GLB(path);newparts=repaired.doc['meshes'][0]['primitives'];checks={}
        for attribute in ['POSITION','NORMAL']:
            actual=np.concatenate([repaired.read(a['attributes'][attribute])for a in newparts[:2]])
            checks[attribute+'_byte_exact']=bool(np.array_equal(actual,g.read(p['attributes'][attribute])))
        actual_ix=np.concatenate([repaired.read(newparts[0]['indices']).ravel(),repaired.read(newparts[1]['indices']).ravel()+prefix])
        checks['triangle_indices_exact']=bool(np.array_equal(actual_ix,ix))
        checks['original_atlas_UV0_exact']=bool(np.array_equal(repaired.read(newparts[1]['attributes']['TEXCOORD_0']),uv))
        if not all(checks.values()):raise ValueError(('Geometry preservation failed',checks))
        info=dict(old['stats'][lod],bytes=path.stat().st_size,sha256=sha(path));stats.append(info);files.append(str(path.resolve()))
        audit.append(dict(lod=label,source=old['files'][lod],source_sha256=sha(old['files'][lod]),output=str(path.resolve()),
            source_boulders=len(families),shell_vertices=prefix,frame=frame_audit,**checks))
    new['files']=files;new['stats']=stats;new['material_revision']=dict(
        generated_shell=KEY,source_tile_period_metres=rocky['source_tile_metres'],
        original_boulders='MI_Statue_Rock, original source UV0 with no density scaling',
        geometry_collision_and_placements='byte/value exact',reason='Statue Rock atlas padding is not tileable')
    result=copy.deepcopy(base);result['assets']=[new if a['name']==NAME else a for a in result['assets']]
    if result['placements']!=base['placements']or new['collision']!=old['collision']:raise ValueError('Protected placement or collision changed')
    result['boundary_foundations']['material_revision']=new['material_revision']
    (args.output/'base-before.json').write_text(json.dumps(base,indent=2)+'\n')
    (args.output/'catalog.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    report=dict(status='COOKED_UV_MATERIAL_ONLY',base_sha256=before_hash,catalog=str((args.output/'catalog.json').resolve()),models=audit,
        unchanged_other_assets=all(a==b for a,b in zip(base['assets'],result['assets'])if a['name']!=NAME),
        collision_exact=True,placements_exact=True,positions_normals_and_triangles_exact=True)
    (args.output/'material-repair-audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

if __name__=='__main__':main()
