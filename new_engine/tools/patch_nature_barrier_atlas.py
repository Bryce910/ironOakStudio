#!/usr/bin/env python3
"""Isolated HIGH-only barrier UV repair; retain exact source geometry and GI.

Reuses an authored two-UV atlas, preserves original generated tangents, and
copies every original position/normal/GI corner. MID continuity is untouched.
"""
from pathlib import Path
import argparse,collections,copy,hashlib,json
import numpy as np
from scipy.spatial import cKDTree
from cook_nature_showcase import Source,DEFAULT_SOURCE,ROOT
from bake_nature_material_fidelity import glb_document
from align_nature_showcase_lods import write_glb
from bake_nature_barrier_atlas import NAMES,MATERIALS

COMPONENTS={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}
DTYPES={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def array(doc,raw,ai):
    a=doc['accessors'][ai];v=doc['bufferViews'][a['bufferView']];dtype=np.dtype(DTYPES[a['componentType']]);count=COMPONENTS[a['type']]
    if a.get('sparse'):raise ValueError('Unexpected sparse accessor')
    return np.ndarray((a['count'],count),dtype,raw,v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',count*dtype.itemsize),dtype.itemsize)).copy()
def unit(a):return a/np.maximum(np.linalg.norm(a,axis=-1,keepdims=True),1e-20)

def original_tangents(p,n,uv,indices):
    """Same sum/project/parity algorithm as the native CPU model preparer."""
    p,n,uv=np.asarray(p,np.float32),unit(np.asarray(n,np.float32)),np.asarray(uv,np.float32)
    tri=indices.reshape(-1,3);a,b,c=p[tri[:,0]],p[tri[:,1]],p[tri[:,2]];e1,e2=b-a,c-a
    d1,d2=uv[tri[:,1]]-uv[tri[:,0]],uv[tri[:,2]]-uv[tri[:,0]];det=d1[:,0]*d2[:,1]-d1[:,1]*d2[:,0];okay=np.abs(det)>1e-12;safe=np.where(okay,det,1.)
    t=(e1*d2[:,1:2]-e2*d1[:,1:2])/safe[:,None];b=(e2*d1[:,0:1]-e1*d2[:,0:1])/safe[:,None];t[~okay]=0;b[~okay]=0
    sums=np.zeros_like(p);bits=np.zeros_like(p);np.add.at(sums,tri.reshape(-1),np.repeat(t,3,axis=0));np.add.at(bits,tri.reshape(-1),np.repeat(b,3,axis=0))
    sums-=n*np.sum(n*sums,axis=1,keepdims=True);bad=np.sum(sums*sums,axis=1)<1e-12;horizontal=bad&(np.abs(n[:,1])<.95);vertical=bad&~horizontal
    sums[horizontal]=np.stack((n[horizontal,2],np.zeros(horizontal.sum()),-n[horizontal,0]),axis=1);sums[vertical]=[1,0,0];sums=unit(sums)
    handed=np.where(np.sum(np.cross(n,sums)*bits,axis=1)<0,-1.,1.).astype(np.float32)
    return np.c_[sums,handed].astype('<f4')

def append(doc,raw,values,kind,index=False):
    values=np.ascontiguousarray(values,dtype='<u4'if index else'<f4');raw.extend(b'\0'*(-len(raw)%4));offset=len(raw);raw.extend(values.tobytes());raw.extend(b'\0'*(-len(raw)%4))
    vi=len(doc['bufferViews']);doc['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':values.nbytes,'target':34963 if index else 34962})
    a={'bufferView':vi,'componentType':5125 if index else 5126,'count':len(values),'type':kind}
    if kind=='VEC3':a.update(min=values.min(0).tolist(),max=values.max(0).tolist())
    ai=len(doc['accessors']);doc['accessors'].append(a);return ai

def referenced_textures(value):
    for k,v in value.items():
        if k.endswith('Texture')and isinstance(v,dict)and'index'in v:yield v
        elif isinstance(v,dict):yield from referenced_textures(v)

def compact(doc,raw):
    # Source chunk materials are unioned across both LOD files. Drop records
    # unused by this HIGH file instead of paying for their embedded payloads.
    used=sorted({p['material']for m in doc['meshes']for p in m['primitives']});matmap={old:new for new,old in enumerate(used)}
    materials=[doc['materials'][i]for i in used]
    for m in materials:
        if 'VALWOOD_materials_facade'in m.get('extensions',{}):raise ValueError('Unexpected facade material in source barrier chunk')
    for mesh in doc['meshes']:
        for p in mesh['primitives']:p['material']=matmap[p['material']]
    textures=sorted({t['index']for m in materials for t in referenced_textures(m)});texmap={old:new for new,old in enumerate(textures)}
    for m in materials:
        for t in referenced_textures(m):t['index']=texmap[t['index']]
    images=sorted({doc['textures'][i]['source']for i in textures});imagemap={old:new for new,old in enumerate(images)};newtextures=[copy.deepcopy(doc['textures'][i])for i in textures]
    for t in newtextures:t['source']=imagemap[t['source']]
    ais=sorted({ai for m in doc['meshes']for p in m['primitives']for ai in[*p['attributes'].values(),p['indices']]});aimap={old:new for new,old in enumerate(ais)}
    newaccessors=[copy.deepcopy(doc['accessors'][i])for i in ais]
    for mesh in doc['meshes']:
        for p in mesh['primitives']:
            p['attributes']={k:aimap[v]for k,v in p['attributes'].items()};p['indices']=aimap[p['indices']]
    out=bytearray();views=[];viewmap={}
    for a in newaccessors:
        i=a['bufferView']
        if i not in viewmap:
            v=doc['bufferViews'][i];offset=v.get('byteOffset',0);payload=raw[offset:offset+v['byteLength']];new=copy.deepcopy(v);new.update(buffer=0,byteOffset=len(out));viewmap[i]=len(views);views.append(new);out.extend(payload);out.extend(b'\0'*(-len(out)%4))
        a['bufferView']=viewmap[i]
    newimages=[];seen={}
    for i in images:
        image=copy.deepcopy(doc['images'][i])
        if 'uri'in image:payload=Path(image.pop('uri')).read_bytes()
        else:v=doc['bufferViews'][image['bufferView']];offset=v.get('byteOffset',0);payload=raw[offset:offset+v['byteLength']]
        key=hashlib.sha256(payload).digest()
        if key not in seen:seen[key]=len(views);views.append({'buffer':0,'byteOffset':len(out),'byteLength':len(payload)});out.extend(payload);out.extend(b'\0'*(-len(out)%4))
        image['bufferView']=seen[key];newimages.append(image)
    removed=len(doc['materials'])-len(materials);doc.update(materials=materials,textures=newtextures,images=newimages,accessors=newaccessors,bufferViews=views)
    return out,removed

def material_semantics(m):
    m=copy.deepcopy(m);m.pop('name',None)
    for ref in referenced_textures(m):ref['index']=0
    return m

def patch(record,source_path,output,references,info):
    positions,normals,uv0,newuv,materials,tree=references;origin=np.asarray(record['origin']);lo=np.asarray(record['world_bounds_min']);hi=np.asarray(record['world_bounds_max']);centers=positions.mean(1)
    if not np.any(np.all(centers>=lo-.001,axis=1)&np.all(centers<=hi+.001,axis=1)):return None
    doc,raw=glb_document(source_path)
    if any(any(k in node for k in('matrix','translation','rotation','scale'))for node in doc['nodes']):raise ValueError('Source chunk nodes unexpectedly transformed')
    original_primitive_count=sum(len(m['primitives'])for m in doc['meshes']);before_triangles=sum(doc['accessors'][p['indices']]['count']//3 for m in doc['meshes']for p in m['primitives']);atlas_index=None;changed=0;source_instances=set();verification=[];original_vertices=sum(doc['accessors'][p['attributes']['POSITION']]['count']for m in doc['meshes']for p in m['primitives'])
    for mesh in doc['meshes']:
        primitives=[]
        for p in mesh['primitives']:
            mat=doc['materials'][p['material']];name=mat.get('name','')
            if name not in MATERIALS:primitives.append(p);continue
            attrs={k:array(doc,raw,i)for k,i in p['attributes'].items()};ids=array(doc,raw,p['indices']).astype(np.uint32).reshape(-1,3);world=attrs['POSITION'].astype(float);world[:,2]*=-1;world+=origin;normal=attrs['NORMAL'].copy();normal[:,2]*=-1;old_uv=attrs['TEXCOORD_0'].copy();old_uv[:,1]=1-old_uv[:,1]
            distances,candidates=tree.query(world[ids].mean(1),k=min(4,len(centers)),workers=1)
            if distances.ndim==1:distances,candidates=distances[:,None],candidates[:,None]
            matched={}
            for row in np.flatnonzero((distances<.00035).any(1)):
                actual=ids[row];best=[]
                for c in candidates[row,distances[row]<.00035]:
                    if materials[c]!=name:continue
                    delta=np.linalg.norm(world[actual,None,:]-positions[c][None,:,:],axis=2);order=delta.argmin(1)
                    if len(set(order))!=3 or delta[np.arange(3),order].max()>.00035:continue
                    if np.max(np.abs(old_uv[actual]-uv0[c][order]))>.00003:continue
                    if np.min(np.sum(normal[actual]*normals[c][order],axis=1))<.9998:continue
                    best.append((int(c),order))
                if not best:continue
                c,order=best[0]
                for other,permutation in best[1:]:
                    if np.max(np.abs(newuv[other][permutation]-newuv[c][order]))>.00001:raise ValueError('Ambiguous coincident barrier source face')
                matched[int(row)]=(c,order)
            if not matched:primitives.append(p);continue
            if atlas_index is None:
                atlas=copy.deepcopy(mat);atlas['name']=info['name']
                for role in('base','normal','arm'):
                    image=len(doc['images']);file=Path(info['paths'][role]);doc['images'].append({'uri':str(file),'mimeType':'image/jpeg'if file.suffix=='.jpg'else'image/png'});texture=len(doc['textures']);doc['textures'].append({'source':image,'sampler':0});ref={'index':texture}
                    if role=='base':atlas['pbrMetallicRoughness']['baseColorTexture']=ref
                    elif role=='normal':atlas['normalTexture']=ref
                    else:atlas['pbrMetallicRoughness']['metallicRoughnessTexture']=ref;atlas['occlusionTexture']=ref
                atlas_index=len(doc['materials']);doc['materials'].append(atlas)
            if material_semantics(mat)!=material_semantics(doc['materials'][atlas_index]):raise ValueError('Source snow material factors differ; cannot share atlas')
            attrs.setdefault('TANGENT',original_tangents(attrs['POSITION'],attrs['NORMAL'],attrs['TEXCOORD_0'],ids))
            rows=np.asarray(sorted(matched),np.int64);mask=np.ones(len(ids),bool);mask[rows]=False;keep_ids=ids[mask].reshape(-1);target_ids=ids[rows].reshape(-1);changed+=len(rows)
            if len(keep_ids):
                used,remap=np.unique(keep_ids,return_inverse=True);keep=copy.deepcopy(p);keep['attributes']={}
                for key,a in attrs.items():keep['attributes'][key]=append(doc,raw,a[used],{'POSITION':'VEC3','NORMAL':'VEC3','TEXCOORD_0':'VEC2','TANGENT':'VEC4','_VALWOOD_BAKED_LIGHT':'VEC4'}[key])
                keep['indices']=append(doc,raw,remap.reshape(-1,1),'SCALAR',True);primitives.append(keep)
                verification.append((keep,{k:a[keep_ids]for k,a in attrs.items()}))
            target=copy.deepcopy(p);target['material']=atlas_index;target['attributes']={};desired={k:a[target_ids].copy()for k,a in attrs.items()};target_uv=[]
            for row in rows:
                c,order=matched[int(row)];target_uv.extend(newuv[c][order])
            desired['TEXCOORD_0']=np.asarray(target_uv,np.float32);desired['TEXCOORD_0'][:,1]=1-desired['TEXCOORD_0'][:,1]
            for key,a in desired.items():target['attributes'][key]=append(doc,raw,a,{'POSITION':'VEC3','NORMAL':'VEC3','TEXCOORD_0':'VEC2','TANGENT':'VEC4','_VALWOOD_BAKED_LIGHT':'VEC4'}[key])
            target['indices']=append(doc,raw,np.arange(len(target_ids),dtype=np.uint32).reshape(-1,1),'SCALAR',True);primitives.append(target);verification.append((target,desired))
            for key in('POSITION','NORMAL','_VALWOOD_BAKED_LIGHT'):
                if not np.array_equal(desired[key],attrs[key][target_ids]):raise ValueError('Source geometry/GI corner changed')
        mesh['primitives']=primitives
    if not changed:return None
    # Check every new accessor/index before compaction so count/type mistakes
    # cannot silently drop two corners or change a GI chart.
    for primitive,expected in verification:
        index=array(doc,raw,primitive['indices']).reshape(-1)
        for key,values in expected.items():
            if not np.array_equal(array(doc,raw,primitive['attributes'][key])[index],values):raise ValueError('Repacked corner accessor mismatch')
    raw,removed=compact(doc,raw);doc.setdefault('asset',{}).setdefault('extras',{})['valwood_targeted_barrier_dual_uv_atlas']=info['fingerprint']
    count=sum(doc['accessors'][p['indices']]['count']//3 for m in doc['meshes']for p in m['primitives']);vertices=sum(doc['accessors'][p['attributes']['POSITION']]['count']for m in doc['meshes']for p in m['primitives'])
    if count!=before_triangles:raise ValueError('Source triangle count changed')
    if vertices>1000000 or count>1000000 or sum(len(m['primitives'])for m in doc['meshes'])>128:raise ValueError('Targeted atlas exceeds native geometry budget')
    output.parent.mkdir(parents=True,exist_ok=True);write_glb(output,doc,raw)
    if output.stat().st_size>128*1024**2:
        size=output.stat().st_size;output.unlink();raise ValueError('Barrier atlas exceeds native file cap: '+source_path.name+' -> '+str(size/1024**2)+' MiB')
    return {'source':str(source_path.resolve()),'candidate':str(output.resolve()),'source_sha256':sha(source_path),'sha256':sha(output),'bytes':output.stat().st_size,'targeted_triangles':changed,'triangles_before_after':count,'vertices_before':original_vertices,'vertices_after':vertices,'source_position_normal_GI_corners_exact':True,'original_tangent_basis_preserved':True,'unused_materials_removed':removed,'original_primitives':original_primitive_count,'final_primitives':sum(len(m['primitives'])for m in doc['meshes'])}

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--catalog',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-bridge-mid-continuity/catalog.json');ap.add_argument('--atlas',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-barrier-atlas');ap.add_argument('--output',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-barrier-quality');args=ap.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    source=Source(DEFAULT_SOURCE);charts=json.loads((args.atlas/'charts.json').read_text());info=json.loads((args.atlas/'material.json').read_text());by_mesh=collections.defaultdict(list)
    def placement_key(p):return(p['mesh'],p['hierarchy'],tuple(round(x,6)for x in p['matrix']))
    allowed={placement_key(p)for p in charts['source_placements']}
    for c in charts['charts']:by_mesh[c['mesh']].append(c)
    pos=[];normal=[];uv=[];atlas=[];mats=[];placements=0
    for p in source.doc['placements']:
        if not p['lod_mask']&1 or placement_key(p)not in allowed or p['mesh']not in by_mesh:continue
        v,_=source.mesh(p['mesh']);matrix=np.asarray(p['matrix']).reshape(4,4,order='F');world=v[:,:3]@matrix[:3,:3].T+matrix[:3,3];n=unit(v[:,3:6]@np.linalg.inv(matrix[:3,:3]));added=False
        for c in by_mesh[p['mesh']]:
            if source.material(p,c['submesh'])!=c['material']:continue
            ids=c['vertex_ids'];pos.append(world[ids]);normal.append(n[ids]);uv.append(v[ids,6:8]);atlas.append(c['atlas_uv']);mats.append(Path(c['material']).stem);added=True
        placements+=added
    positions,normals,uv0,newuv=np.asarray(pos),np.asarray(normal),np.asarray(uv),np.asarray(atlas);tree=cKDTree(positions.mean(1));references=positions,normals,uv0,newuv,np.asarray(mats),tree
    catalog=copy.deepcopy(json.loads(args.catalog.read_text()));reports=[]
    for record in catalog:
        original=Path(record['files'][0]);result=patch(record,original,args.output/'chunks'/original.name,references,info)
        if not result:continue
        record['files'][0]=result['candidate'];record['file_sha256']=[sha(p)for p in record['files']];record['targeted_barrier_atlas']=str((args.atlas/'material.json').resolve());reports.append(result);print('BARRIER UV',record['name'],result['targeted_triangles'],'tris',round(result['bytes']/1024**2,2),'MiB',flush=True)
    if sum(r['targeted_triangles']for r in reports)!=len(positions):raise ValueError('Not all exact source target triangles were recovered: '+str(sum(r['targeted_triangles']for r in reports))+'/'+str(len(positions)))
    (args.output/'catalog.json').write_text(json.dumps(catalog,indent=2)+'\n');report={'source_catalog':str(args.catalog.resolve()),'atlas':str(args.atlas.resolve()),'files':reports,'source_placements':placements,'targeted_triangles':len(positions),'all_MID_files_exact':all(r['files'][1]==old['files'][1]for r,old in zip(catalog,json.loads(args.catalog.read_text()))),'GPU_vertex_ABI_unchanged':True,'source_position_normal_GI_corners_exact':True,'new_aliases':0,'production_untouched':True};(args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('READY',len(reports),'HIGH files;',len(positions),'source triangles;',placements,'instances',flush=True)

if __name__=='__main__':main()
