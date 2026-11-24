#!/usr/bin/env python3
"""Stage exact source-UV window PBR on the six purchased near houses.

Only our existing decimated source metadata and prepared purchased images are
read. The current project, original downloads and all middle LODs are read-only.
The source window material is ordinary opaque PBR, with no invented interior,
emission, refraction or transparency. Other material/image bytes stay exact.
"""
from pathlib import Path
import argparse,copy,hashlib,json,sys
import numpy as np

sys.path.insert(0,str(Path(__file__).resolve().parent))
from build_purchased_village import ROOT,SOURCE,glb,encode_glb,inspect_glb,material_source_settings
from build_purchased_village_facades import accessor
from validate_purchased_village_fidelity import key

FAMILIES=('house_a','house_b','house_c','house_d','house_e','house_f')
SOURCE_NAME='MI_Tileable_Glass_Window'
SOURCE_STEM='T_Tileable_Glass_Window'

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def append(doc,binary,values,kind):
    values=np.ascontiguousarray(values)
    binary.extend(b'\0'*(-len(binary)%4));view=len(doc['bufferViews'])
    doc['bufferViews'].append(dict(buffer=0,byteOffset=len(binary),byteLength=values.nbytes,target=34963 if kind=='SCALAR' else 34962))
    binary.extend(values.tobytes());a=dict(bufferView=view,componentType=5125 if values.dtype==np.dtype('<u4') else 5126,count=len(values),type=kind)
    if kind=='VEC3':a.update(min=values.min(0).tolist(),max=values.max(0).tolist())
    doc['accessors'].append(a);return len(doc['accessors'])-1

def image(doc,binary,path):
    data=path.read_bytes();binary.extend(b'\0'*(-len(binary)%4));view=len(doc['bufferViews'])
    doc['bufferViews'].append(dict(buffer=0,byteOffset=len(binary),byteLength=len(data)));binary.extend(data)
    i=len(doc.setdefault('images',[]));doc['images'].append(dict(name=path.stem,mimeType='image/png',bufferView=view))
    if not doc.get('samplers'):doc['samplers']=[dict(magFilter=9729,minFilter=9987,wrapS=10497,wrapT=10497)]
    sampler=len(doc['samplers']);doc['samplers'].append(dict(magFilter=9729,minFilter=9987,wrapS=10497,wrapT=10497))
    t=len(doc.setdefault('textures',[]));doc['textures'].append(dict(source=i,sampler=sampler));return dict(index=t)

def source_windows(family):
    path=SOURCE/'source-facades'/(family+'_source.glb');doc,binary=glb(path);result={};count=0
    for mesh in doc['meshes']:
        for p in mesh['primitives']:
            if doc['materials'][p['material']]['name'].split('.')[0]!=SOURCE_NAME:continue
            a=p['attributes'];xyz=accessor(doc,binary,a['POSITION'],3);normal=accessor(doc,binary,a['NORMAL'],3)
            uv=accessor(doc,binary,a['TEXCOORD_0'],2);tangent=accessor(doc,binary,a['TANGENT'],4)
            for ix in accessor(doc,binary,p['indices'],1).reshape(-1,3):
                r=dict(position=xyz[ix],normal=normal[ix],uv=uv[ix],tangent=tangent[ix],id=count)
                result.setdefault(key(r['position']),[]).append(r);count+=1
    if not count:raise ValueError('No source glass: '+family)
    return path,result,count

def geometry_digest(doc,binary):
    rows=[]
    for mesh in doc['meshes']:
        for p in mesh['primitives']:
            a=p['attributes'];xyz=accessor(doc,binary,a['POSITION'],3);n=accessor(doc,binary,a['NORMAL'],3)
            for ix in accessor(doc,binary,p['indices'],1).reshape(-1,3):
                # Preserve winding as well as every position/normal corner.
                rows.append(np.c_[xyz[ix],n[ix]].astype('<f4').tobytes())
    return hashlib.sha256(b''.join(sorted(rows))).hexdigest(),len(rows)

def stage(family,models,out):
    source=models/('purchased_village_'+family+'_near.glb');before,old=glb(source);doc=copy.deepcopy(before);binary=bytearray(old)
    metadata,windows,count=source_windows(family);seen=set();new_vertices=[];nonwindow=0;retained=[];basis_min=1.;coverage=[]
    original_materials=copy.deepcopy(doc['materials']);original_images=copy.deepcopy(doc['images']);original_texture_count=len(doc['textures'])
    for mesh in doc['meshes']:
        keep=[]
        for p in mesh['primitives']:
            a=p['attributes'];xyz=accessor(doc,old,a['POSITION'],3);normal=accessor(doc,old,a['NORMAL'],3);uv=accessor(doc,old,a['TEXCOORD_0'],2)
            remaining=[]
            for ix in accessor(doc,old,p['indices'],1).reshape(-1,3):
                choices=windows.get(key(xyz[ix]),[]);matches=[]
                for candidate in choices:
                    order=[int(np.argmin(np.sum((candidate['position']-v)**2,axis=1)))for v in xyz[ix]]
                    if len(set(order))!=3:continue
                    if np.max(np.abs(candidate['position'][order]-xyz[ix]))>2e-5:continue
                    if np.max(np.abs(candidate['normal'][order]-normal[ix]))>.002:continue
                    matches.append((candidate,order))
                if not matches:
                    remaining.extend(map(int,ix));nonwindow+=1;continue
                if len(matches)!=1:raise ValueError('Ambiguous source window: '+family)
                src,order=matches[0]
                if src['id']in seen:raise ValueError('Source window matched twice: '+family)
                seen.add(src['id']);tangent=src['tangent'][order];n=normal[ix]
                orth=np.abs(np.sum(tangent[:,:3]*n,axis=1));assert orth.max()<.002
                basis_min=min(basis_min,float(np.sum(src['normal'][order]*n,axis=1).min()))
                desired=np.c_[xyz[ix],n,src['uv'][order],tangent].astype('<f4')
                assert desired[:,:3].tobytes()==np.asarray(xyz[ix],'<f4').tobytes()
                assert desired[:,3:6].tobytes()==np.asarray(n,'<f4').tobytes()
                new_vertices.extend(desired)
                area=lambda q:abs(float(np.linalg.det(np.stack((q[1]-q[0],q[2]-q[0])))))*.5
                coverage.append(dict(source_uv_area=area(src['uv']),old_atlas_uv_area=area(uv[ix])))
            if len(remaining)==accessor(doc,old,p['indices'],1).size:keep.append(p)
            elif remaining:
                kept=copy.deepcopy(p);kept['indices']=append(doc,binary,np.asarray(remaining,'<u4'),'SCALAR');keep.append(kept)
        mesh['primitives']=keep
    if len(seen)!=count:raise ValueError(f'Unmatched source windows {family}: {len(seen)}/{count}')
    mat,tiling=material_source_settings(SOURCE_NAME);assert mat['parent']=='MM_Simple'and tiling==1.
    assert not mat.get('base_property_overrides',{}).get('BlendMode'),'Source window blend mode changed'
    bc,n,arm=(image(doc,binary,SOURCE/'prepared'/(SOURCE_STEM+'_'+role+'.png'))for role in('BC','N','ARM'))
    material=len(doc['materials']);doc['materials'].append(dict(name='Purchased_Source_Glass_Window',alphaMode='OPAQUE',doubleSided=False,
        pbrMetallicRoughness=dict(baseColorTexture=bc,metallicRoughnessTexture=arm,metallicFactor=1.,roughnessFactor=1.),normalTexture=n,occlusionTexture=copy.deepcopy(arm)))
    packed=np.asarray(new_vertices,'<f4');unique,first,inverse=np.unique(packed.view('V48').reshape(-1),return_index=True,return_inverse=True)
    packed=packed[first];attrs={}
    for name,start,end,kind in [('POSITION',0,3,'VEC3'),('NORMAL',3,6,'VEC3'),('TEXCOORD_0',6,8,'VEC2'),('TANGENT',8,12,'VEC4')]:attrs[name]=append(doc,binary,packed[:,start:end].copy(),kind)
    doc['meshes'][0]['primitives'].append(dict(attributes=attrs,indices=append(doc,binary,inverse.astype('<u4'),'SCALAR'),material=material,mode=4))
    assert binary[:len(old)]==old
    assert doc['materials'][:len(original_materials)]==original_materials
    assert doc['images'][:len(original_images)]==original_images
    assert doc['textures'][:original_texture_count]==before['textures']
    doc['buffers'][0]['byteLength']=len(binary);doc.setdefault('extras',{})['source_window_fidelity']={'schema':1,'input_sha256':sha(source),'metadata_sha256':sha(metadata),'windows':count,'source_uv_tangent':True,'opaque_as_authored':True}
    output=out/source.name;output.write_bytes(encode_glb(doc,binary));after,b=glb(output)
    gd,triangles=geometry_digest(before,old);assert geometry_digest(after,b)==(gd,triangles)
    assert len(after['materials'])==len(before['materials'])+1
    stats=inspect_glb(output);assert stats['triangles']==triangles and output.stat().st_size<128*1024**2
    return dict(family=family,source=str(source.resolve()),source_sha256=sha(source),candidate=str(output.resolve()),candidate_sha256=sha(output),bytes=output.stat().st_size,
        source_metadata=str(metadata.resolve()),source_metadata_sha256=sha(metadata),window_triangles=count,total_triangles=triangles,nonwindow_triangles=nonwindow,
        position_normal_corner_digest=gd,source_geometry_and_normals_exact=True,source_window_uv_tangent_exact=True,source_normal_alignment_min=basis_min,
        original_materials_and_images_exact=True,original_binary_payload_exact_prefix=True,primitive_count=stats['primitive_count'],UV_coverage=coverage)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--models',type=Path,default=ROOT/'projects/valwood_world/models');ap.add_argument('--output',type=Path,default=SOURCE/'source-windows');args=ap.parse_args()
    args.output.mkdir(parents=True,exist_ok=True);models=args.output/'models';models.mkdir(exist_ok=True)
    originals={str(p.resolve()):sha(p)for f in FAMILIES for p in[args.models/('purchased_village_'+f+'_'+lod+'.glb')for lod in('near','mid')]}
    records=[stage(f,args.models,models)for f in FAMILIES]
    for p,h in originals.items():assert sha(p)==h
    report=dict(schema=1,production_unchanged=True,all_originals_and_mid_LODs_unchanged=True,shared_source_images=[dict(path=str((SOURCE/'prepared'/(SOURCE_STEM+'_'+role+'.png')).resolve()),sha256=sha(SOURCE/'prepared'/(SOURCE_STEM+'_'+role+'.png')))for role in('BC','N','ARM')],added_shared_RGBA8_mip_MiB=28.,new_runtime_vertex_attributes=0,files=records)
    (args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('STAGED',len(records),'near houses,',sum(r['window_triangles']for r in records),'source window triangles; production unchanged',flush=True)

if __name__=='__main__':main()
