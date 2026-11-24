#!/usr/bin/env python3
"""Close the copied display-room wall returns in a private C14 art revision."""
from pathlib import Path
import argparse,copy,hashlib,json,math,re
import numpy as np
from build_opening_prologue import Glb,read_glb,array
from opening_home_reference_enclosure import _encode,_ProbeField,PROBE_REPORT,append_enclosure_physics,TEXTURES

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'build/journey-opening/candidate-v13/project'
CANDIDATE=ROOT/'build/journey-opening/candidate-v14/project'
OUT=ROOT/'docs/apartment-walls/2026-11-05/geometry-v2'
HOME=np.array([-3500.,600.,-11000.])
# x, original jamb end, required floor-union perimeter, source sampling direction
RETURNS=[(-6.,-9.,-9.25,1),(0.,-6.,-6.25,1),(4.,-6.,-6.25,1),
 (8.5,-6.,-6.25,1),(13.,-6.,-6.25,1),(18.,-6.,-8.25,1),
 (23.,-6.,-8.25,1),(27.5,-6.,-8.25,1),(30.5,-5.,-5.25,1),
 (33.5,-5.,-8.25,1),(8.5,-3.,-2.75,-1)]

def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def write(p,v):
    with p.open('x') as f:json.dump(v,f,indent=2,allow_nan=False);f.write('\n')
def clip(poly,axis,value,sign):
    out=[]
    for a,b in zip(poly,poly[1:]+poly[:1]):
        da=(a[axis]-value)*sign;db=(b[axis]-value)*sign
        if da>=-1e-8:out.append(a)
        if (da>1e-8 and db< -1e-8)or(da< -1e-8 and db>1e-8):out.append(a+(b-a)*(da/(da-db)))
    return out

def stage():
    OUT.mkdir(parents=True,exist_ok=False)
    protected=['world.studio','opening/home.physics','opening/room-controls.settings','opening/room-lights.settings',
        'opening.config','opening/first-journey.settings','opening/first-journey.routes.json','scenery/authored_instances.txt']
    before={n:sha(SOURCE/n) for n in protected}
    source_model=SOURCE/'models/opening/home_enclosure.glb';doc,binary=read_glb(source_model)
    original_primitives=copy.deepcopy(doc['meshes'][0]['primitives'])
    g=Glb();g.doc=copy.deepcopy(doc);g.binary=bytearray(binary)
    field=_ProbeField(PROBE_REPORT);model_inputs={str(source_model.resolve()):sha(source_model)}
    image_keys={}
    for i,im in enumerate(g.doc['images']):
        v=g.doc['bufferViews'][im['bufferView']];raw=binary[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]
        image_keys[(hashlib.sha256(raw).hexdigest(),im['mimeType'])]=i
    mats={m['name']:i for i,m in enumerate(g.doc['materials'])}
    def material(sd,sb,number):
        m=copy.deepcopy(sd['materials'][number])
        if m['name'] in mats:return mats[m['name']]
        for pk,key in TEXTURES:
            parent=m if pk is None else m.get(pk,{})
            if key not in parent:continue
            st=sd['textures'][parent[key]['index']];im=sd['images'][st['source']]
            assert 'uri'not in im
            v=sd['bufferViews'][im['bufferView']];raw=sb[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]
            ik=(hashlib.sha256(raw).hexdigest(),im['mimeType'])
            if ik not in image_keys:
                target=copy.deepcopy(im);target['bufferView']=g.view(raw);g.doc['images'].append(target);image_keys[ik]=len(g.doc['images'])-1
            target=copy.deepcopy(st);target['source']=image_keys[ik]
            if'sampler'in st:
                sm=sd['samplers'][st['sampler']]
                if sm not in g.doc['samplers']:g.doc['samplers'].append(copy.deepcopy(sm))
                target['sampler']=g.doc['samplers'].index(sm)
            g.doc['textures'].append(target);parent[key]['index']=len(g.doc['textures'])-1
        g.doc['materials'].append(m);mats[m['name']]=len(g.doc['materials'])-1;return len(g.doc['materials'])-1
    # Exact existing source wall profiles: retain their plaster, skirting,
    # UVs, normals and encoded maps; only the new return span is stretched.
    wall_parts=[];layout=json.loads((SOURCE/'opening/layout.json').read_text())
    for record in layout['render_meshes']:
        if not Path(record['path']).name.startswith('finward_scene_'):continue
        path=SOURCE/record['path'];sd,sb=read_glb(path);model_inputs[str(path.resolve())]=sha(path)
        for p in sd['meshes'][0]['primitives']:
            if not sd['materials'][p['material']]['name'].startswith('Finward Wall_'):continue
            pos=array(sd,sb,p['attributes']['POSITION'])+record['offset'];norm=array(sd,sb,p['attributes']['NORMAL'])
            uv=array(sd,sb,p['attributes']['TEXCOORD_0']);uv1=array(sd,sb,p['attributes']['TEXCOORD_1']);uv2=array(sd,sb,p['attributes']['TEXCOORD_2'])
            values=np.column_stack((pos,norm,uv,uv1,uv2));idx=array(sd,sb,p['indices']).reshape(-1,3)
            wall_parts.append((values,idx,sd,sb,p['material']))
    exported=json.loads((ROOT/'assets/purchased/finward_house_props/scene_export/scene.json').read_text())
    plain=[];door_colors={}
    for p in exported['placements']:
        if not p['lod_mask']&1 or not p['hierarchy'].startswith('Building/Wall_'):continue
        lo,hi=p['bounds_min'],p['bounds_max'];low=np.array([lo['x'],lo['y'],-hi['z']]);high=np.array([hi['x'],hi['y'],-lo['z']])
        if high[0]-low[0]>.4 or high[2]-low[2]<2.5:continue
        x=float(p['matrix'][12]);side=1 if (low[0]+high[0])/2>x else-1
        name='Finward '+Path(p['materials'][0].split('#')[0]).stem
        if '_door'in p['hierarchy']:door_colors[(round(x,3),side)]=name
        else:plain.append(dict(low=low,high=high,x=x,side=side,name=name,hierarchy=p['hierarchy']))
    parts={};records=[];collpos=[];collidx=[];added_vertices=added_triangles=0
    def emit(mat,pos,norm,uv,uv1,uv2,idx):
        nonlocal added_vertices,added_triangles
        light=field.sample(pos,norm);local=pos-[0.,-.155,0.]
        parts.setdefault(mat,[]).append((local,norm,uv,uv1,uv2,idx,light))
        added_vertices+=len(pos);added_triangles+=len(idx)
    def box(lo,hi,mat,visual=True,physical=True):
        p=[];n=[];uv=[];ix=[]
        for axis in range(3):
            a,b=(axis+1)%3,(axis+2)%3
            for sign in (-1,1):
                corners=[(0,0),(1,0),(1,1),(0,1)]
                if sign<0:corners=corners[::-1]
                off=len(p)
                for aa,bb in corners:
                    q=np.array(lo,float);q[axis]=lo[axis] if sign<0 else hi[axis];q[a]=lo[a]+aa*(hi[a]-lo[a]);q[b]=lo[b]+bb*(hi[b]-lo[b]);nn=np.zeros(3);nn[axis]=sign
                    p.append(q);n.append(nn);uv.append([aa,bb])
                ix.extend([[off,off+1,off+2],[off,off+2,off+3]])
        p=np.asarray(p);idx=np.asarray(ix);norm=np.asarray(n);uv=np.asarray(uv)
        if visual:emit(mat,p,norm,uv,np.zeros_like(uv),np.zeros_like(uv),idx)
        if physical:
            start=len(collpos);collpos.extend(p.tolist());collidx.extend((idx+start).tolist())
    neutral=mats['Finward Wall_default']
    for x,end,perimeter,direction in RETURNS:
        target=sorted([end,perimeter]);count=0;materials=set()
        selected=[]
        for side in (-1,1):
            color=door_colors[(round(x,3),side)]
            options=[p for p in plain if p['name']==color]
            assert options,(x,side,color)
            profile=min(options,key=lambda p:(p['side']!=side,abs(p['x']-x)))
            selected.append((side,profile))
        for side,profile in selected:
          # These source panels are extruded along Z. A middle .12m strip
          # retains their exact skirting/cornice profile without copying every
          # SH subdivision across a3–4m panel into a short .25m return.
          middle=(profile['high'][2]+profile['low'][2])*.5
          strip_low,strip_high=middle-.06,middle+.06
          scale=(target[1]-target[0])/.12;flip=side/profile['side']
          for values,idx,sd,sb,mi in wall_parts:
            if sd['materials'][mi]['name']!=profile['name']:continue
            tri=values[idx];lo=profile['low']-1e-6;hi=profile['high']+1e-6;lo[2]=strip_low;hi[2]=strip_high
            mask=np.all(tri[:,:,:3].max(1)>=lo-1e-6,axis=1)&np.all(tri[:,:,:3].min(1)<=hi+1e-6,axis=1)
            triangles=[]
            for triangle in tri[mask]:
                poly=list(triangle)
                for axis in range(3):
                    poly=clip(poly,axis,lo[axis],1) if poly else[];poly=clip(poly,axis,hi[axis],-1)if poly else[]
                for i in range(1,len(poly)-1):
                    t=np.array([poly[0],poly[i],poly[i+1]])
                    if np.linalg.norm(np.cross(t[1,:3]-t[0,:3],t[2,:3]-t[0,:3]))>1e-10:triangles.append(t)
            if not triangles:continue
            # Continue each source face's affine UV gradient per world metre;
            # the longer new span does not stretch a12cm patch of plaster.
            for t in triangles:
                edges=(t[1:,:3]-t[0,:3]).T
                weights=np.linalg.lstsq(edges,np.array([0.,0.,1.]),rcond=None)[0]
                gradient=(t[1:,6:8]-t[0,6:8]).T@weights
                t[:,6:8]+=(t[:,2]-strip_low)[:,None]*(scale-1)*gradient
            if flip<0:triangles=[t[[0,2,1]]for t in triangles]
            v=np.concatenate(triangles);v[:,0]=x+(v[:,0]-profile['x'])*flip;v[:,2]=target[0]+(v[:,2]-strip_low)*scale
            v[:,3]/=flip;v[:,5]/=scale;v[:,3:6]/=np.maximum(np.linalg.norm(v[:,3:6],axis=1,keepdims=True),1e-12)
            mat=material(sd,sb,mi);emit(mat,v[:,:3],v[:,3:6],v[:,6:8],v[:,8:10],v[:,10:12],np.arange(len(v)).reshape(-1,3));count+=len(triangles);materials.add(g.doc['materials'][mat]['name'])
        assert count>20,(x,end,count)
        bounds_lo=[x-.178,-.015,target[0]];bounds_hi=[x+.178,2.62,target[1]]
        # Plain wall profiles own the sole colored X surfaces. Do not emit
        # coplanar central-core faces or a copied doorway's vertical trim.
        # Toe/top meet the old shell, and the terminal cap joins the perimeter;
        # the retained source jamb already closes the other end.
        box(bounds_lo,[x+.178,0.,target[1]],neutral,physical=False)
        box([x-.178,2.6,target[0]],bounds_hi,neutral,physical=False)
        box([x-.178,0,perimeter-.0005],[x+.178,2.6,perimeter+.0005],neutral,physical=False)
        box(bounds_lo,bounds_hi,neutral,visual=False)
        records.append(dict(kind='partition return',center_x=x,old_end_z=end,perimeter_z=perimeter,minimum=bounds_lo,maximum=bounds_hi,source_profile_triangles=count,source_materials=sorted(materials),doorways_unchanged=True))
    # Join stepped floor-union corners rather than leave .072m parallel slits.
    for lo,hi in [([.178,-.015,-6.265],[.265,2.62,-6.0]),([17.735,-.015,-8.265],[17.822,2.62,-6.0]),([27.678,-.015,-8.265],[27.765,2.62,-6.0]),([33.235,-.015,-8.265],[33.322,2.62,-5.0])]:
        box(lo,hi,neutral);records.append(dict(kind='stepped perimeter connector',minimum=lo,maximum=hi))
    for mat,pieces in parts.items():
        off=np.cumsum([0]+[len(p[0])for p in pieces[:-1]]);a=[np.concatenate([p[i]for p in pieces])for i in (0,1,2,3,4,6)]
        attrs={name:g.accessor(val,kind)for name,val,kind in zip(['POSITION','NORMAL','TEXCOORD_0','TEXCOORD_1','TEXCOORD_2','_VALWOOD_BAKED_LIGHT'],a,['VEC3','VEC3','VEC2','VEC2','VEC2','VEC4'])}
        index=np.concatenate([p[5]+o for p,o in zip(pieces,off)])
        g.doc['meshes'][0]['primitives'].append(dict(attributes=attrs,indices=g.accessor(index.reshape(-1),'SCALAR'),material=mat,mode=4))
    g.doc['buffers'][0]['byteLength']=len(g.binary)
    payload=_encode(g.doc,bytes(g.binary));assert bytes(g.binary[:len(binary)])==binary
    assert g.doc['meshes'][0]['primitives'][:len(original_primitives)]==original_primitives
    model=OUT/'home_enclosure.glb';model.write_bytes(payload)
    physics=(SOURCE/'opening/home.physics').read_bytes();report=dict(collision_bodies_added=1,collision_vertices_native=collpos,collision_triangles=collidx)
    newphysics=append_enclosure_physics(physics,report,HOME)
    assert newphysics.startswith(physics)
    (OUT/'home.physics').write_bytes(newphysics)
    result=dict(schema='valwood.apartment-continuous-wall-returns',version=1,staged=True,source_project=str(SOURCE),target_project=str(CANDIDATE),
        source_files_sha256={str(SOURCE/n):s for n,s in before.items()},source_model_sha256=model_inputs,model=str(model),model_sha256=sha(model),source_enclosure_sha256=sha(source_model),
        physics=str(OUT/'home.physics'),physics_sha256=sha(OUT/'home.physics'),existing_physics_byte_exact_prefix=True,existing_body_count=14,new_body_count=15,
        source_geometry_and_material_payload_preserved=True,source_binary_exact_prefix=True,source_primitives_exact=True,existing_doorways_and_switches_unchanged=True,
        added_vertices=added_vertices,added_triangles=added_triangles,new_structural_boxes=records,collision_vertices=collpos,collision_triangles=collidx,
        candidate_before_sha256={str(CANDIDATE/p):sha(CANDIDATE/p)for p in ('models/opening/home_enclosure.glb','models/opening/home_enclosure_mid.glb','opening/home.physics','opening/enclosure.json','opening/layout.json','opening/partition-returns.json')},
        geometry_offset_native=[0,-.155,0],physics_origin=HOME.tolist(),new_material_names=[m['name']for m in g.doc['materials'][len(doc['materials']):]],
        lighting='Existing exact baked vertices untouched; new return profiles use interpolated original source SH probes.',gpu_used=False)
    assert before=={n:sha(SOURCE/n)for n in protected}
    write(OUT/'RETURNS.json',result);print(json.dumps({k:result[k]for k in ['staged','model_sha256','physics_sha256','added_vertices','added_triangles','new_body_count']},indent=2))

def apply():
    review=json.loads((OUT/'RETURNS.json').read_text());assert review['staged']
    for p,s in review['source_files_sha256'].items():assert sha(p)==s
    for p,s in review['source_model_sha256'].items():assert sha(p)==s
    assert sha(review['model'])==review['model_sha256'] and sha(review['physics'])==review['physics_sha256']
    assert CANDIDATE.is_dir()
    for p,s in review['candidate_before_sha256'].items():assert sha(p)==s
    for suffix in ('','_mid'):
        q=CANDIDATE/('models/opening/home_enclosure'+suffix+'.glb');assert q.is_symlink()
    for name in ('home.physics','enclosure.json','layout.json','partition-returns.json'):assert not(CANDIDATE/'opening'/name).is_symlink()
    # Stage complete bytes first; never write through any shared model link.
    for suffix in ('','_mid'):
        q=CANDIDATE/('models/opening/home_enclosure'+suffix+'.glb');q.unlink();q.symlink_to(Path(review['model']))
    (CANDIDATE/'opening/home.physics').write_bytes(Path(review['physics']).read_bytes())
    enclosure=json.loads((SOURCE/'opening/enclosure.json').read_text());enclosure['base_enclosure_model_sha256']=enclosure['sha256'];enclosure['sha256']=review['model_sha256'];enclosure['bytes']=Path(review['model']).stat().st_size;enclosure['continuous_partition_returns']='partition-returns.json'
    (CANDIDATE/'opening/enclosure.json').write_text(json.dumps(enclosure,indent=2)+'\n')
    layout=json.loads((SOURCE/'opening/layout.json').read_text());layout['collision_bodies']=15;layout['continuous_partition_returns']='opening/partition-returns.json';(CANDIDATE/'opening/layout.json').write_text(json.dumps(layout,indent=2)+'\n')
    (CANDIDATE/'opening/partition-returns.json').write_text(json.dumps({k:v for k,v in review.items()if k not in ('source_files_sha256','source_model_sha256','collision_vertices','collision_triangles','candidate_before_sha256')},indent=2)+'\n')
    assert all(sha(p)==s for p,s in review['source_files_sha256'].items())
    print(json.dumps(dict(applied=True,target=str(CANDIDATE),source_c13_unchanged=True,physics_sha256=sha(CANDIDATE/'opening/home.physics'),model_sha256=sha(CANDIDATE/'models/opening/home_enclosure.glb')),indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--apply',action='store_true');a=p.parse_args();apply()if a.apply else stage()
