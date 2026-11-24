#!/usr/bin/env python3
"""Create reversible visual-only ground overlays; never change an input project.

The purchased terrain's existing UVs are metre-aligned. Retain all original
geometry, UVs, indices, datums and embedded source-image bytes, and append
softened normals plus one continuous road-paint accessor. Existing bounded
facade mode 2 carries the terrain layers; scale.z=7.875 tags this opt-in use.
The companion patch_shader() changes only those explicitly tagged materials.
"""
from __future__ import annotations
import argparse, copy, hashlib, io, json, struct
from pathlib import Path
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT.parent/'.local-tools/bep-source'
CATALOG=ROOT/'assets/purchased/bep_reforged/city-ground-final/catalog.json'
MARKER=7.875
TARGET_NORMAL={'earthpath':.55,'grassground':.62,'rocky':.72}


def sha(data):
    return hashlib.sha256(data if isinstance(data,bytes) else Path(data).read_bytes()).hexdigest()


class GLB:
    def __init__(self,path):
        self.path=Path(path);self.raw=self.path.read_bytes()
        if self.raw[:4]!=b'glTF' or struct.unpack_from('<I',self.raw,4)[0]!=2:
            raise ValueError('Expected glTF2 GLB')
        n,kind=struct.unpack_from('<II',self.raw,12)
        if kind!=0x4e4f534a:raise ValueError('Missing GLB JSON')
        self.doc=json.loads(self.raw[20:20+n]);size,kind=struct.unpack_from('<II',self.raw,20+n)
        if kind!=0x004e4942:raise ValueError('Missing embedded GLB BIN')
        self.binary=self.raw[28+n:28+n+size]
        if len(self.doc['buffers'])!=1 or self.doc['buffers'][0].get('uri'):
            raise ValueError('Expected one embedded buffer')

    def read(self,index):
        a=self.doc['accessors'][index];v=self.doc['bufferViews'][a['bufferView']]
        components={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
        dtype=np.dtype({5126:'<f4',5125:'<u4',5123:'<u2'}[a['componentType']])
        return np.ndarray((a['count'],components),dtype=dtype,buffer=self.binary,
            offset=v.get('byteOffset',0)+a.get('byteOffset',0),
            strides=(v.get('byteStride',components*dtype.itemsize),dtype.itemsize)).copy()


def patch_shader(text):
    """Return a marker-gated patch of world_foliage.fs.glsl, or its no-op repeat."""
    if '// VISUAL_GROUND_FINISH_V1' in text:return text
    marker='vec4 facade_base(vec2 uv,vec2 dx,vec2 dy) {'
    if text.count(marker)!=1:raise ValueError('Expected one facade_base definition')
    text=text.replace(marker,'''// VISUAL_GROUND_FINISH_V1: only authored countryside overlays use this tag.
bool visual_ground_blend() {return uFacadeMode==2&&abs(uFacadeUVScale.z-7.875)<.0001;}
vec4 facade_base(vec2 uv,vec2 dx,vec2 dy) {
    if(visual_ground_blend()) {
        vec3 grass=textureGrad(uBase,uv,dx,dy).rgb;
        vec3 soil=textureGrad(uFacadeAux2,uv,dx,dy).rgb;
        return vec4(mix(grass,soil,clamp(vFacadePaint,0.,1.)),1.);
    }''',1)
    marker='vec3 facade_normal(vec2 uv,vec2 dx,vec2 dy) {'
    if text.count(marker)!=1:raise ValueError('Expected one facade_normal definition')
    text=text.replace(marker,'''vec3 facade_normal(vec2 uv,vec2 dx,vec2 dy) {
    if(visual_ground_blend()) {
        vec3 grass=textureGrad(uNormal,uv,dx,dy).xyz*2.-1.;
        vec3 soil=textureGrad(uFacadeAux0,uv,dx,dy).xyz*2.-1.;
        return normalize(mix(grass,soil,clamp(vFacadePaint,0.,1.)))*.5+.5;
    }''',1)
    marker='vec3 facade_arm(vec2 uv,vec2 dx,vec2 dy) {'
    if text.count(marker)!=1:raise ValueError('Expected one facade_arm definition')
    return text.replace(marker,'''vec3 facade_arm(vec2 uv,vec2 dx,vec2 dy) {
    if(visual_ground_blend())return vec3(1.,1.,0.);''',1)


def prepare_normals(output):
    prepared=json.loads((ROOT/'assets/purchased/bep_reforged/countryside-materials/ground-materials.json').read_text())
    layers={r['guid']:r['data']for r in json.loads((SOURCE/'terrain-layers.json').read_text())}
    result={};audit=[]
    for name,strength in TARGET_NORMAL.items():
        layer=layers[prepared[name]['source_layer_guid']]
        source=SOURCE/'BEP_Built-in_02/guid'/layer['m_NormalMapTexture']['guid']/'asset'
        im=Image.open(source).convert('RGB');a=np.asarray(im,np.float32)/255.
        xy=(a[:,:,:2]*2.-1.)*strength
        n=np.dstack((xy,np.sqrt(np.maximum(1.-np.sum(xy*xy,axis=2),0.))))
        target=(min(1024,im.width),min(1024,im.height))
        n=np.stack([np.asarray(Image.fromarray(n[:,:,c].astype(np.float32),'F').resize(target,Image.Resampling.LANCZOS))for c in range(3)],axis=2)
        n/=np.maximum(np.linalg.norm(n,axis=2,keepdims=True),1e-8)
        n[:,:,1]*=-1 # Original Unity-to-native V correction, exactly once.
        b=np.uint8(np.clip(np.rint((n*.5+.5)*255),0,255));buf=io.BytesIO()
        Image.fromarray(b).save(buf,format='PNG',compress_level=3);raw=buf.getvalue()
        dest=output/'textures'/(name+'_normal.png');dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(raw)
        result[name]=raw
        audit.append(dict(material=name,original_strength=layer['m_NormalScale'],new_strength=strength,
            original_normal=str(source),original_sha256=sha(source),output=str(dest),sha256=sha(raw),
            output_size=list(target),green_inverted_once=True))
    return prepared,result,audit


def segment_distance(points,segments):
    """Small bounded batches keep the source road distance query inexpensive."""
    points=np.asarray(points);out=np.empty(len(points))
    a=segments[:,0];d=segments[:,1]-a;length=(d*d).sum(1)
    if np.any(length<=0):raise ValueError('Degenerate source road segment')
    for start in range(0,len(points),1024):
        p=points[start:start+1024]
        t=np.clip(((p[:,None]-a)*d).sum(2)/length,0,1)
        out[start:start+len(p)]=np.linalg.norm(p[:,None]-(a+d*t[:,:,None]),axis=2).min(1)
    return out


def road_weight(points,segments,city_rect):
    distance=segment_distance(points,segments)
    x,z=points.T
    # Stable metre-scale irregularity, independent of camera and triangle IDs.
    edge=.24*np.sin(x*1.17+z*.81)+.13*np.sin(x*.49-z*1.73)
    t=np.clip((distance+edge-1.6)/(4.3-1.6),0,1)
    weight=1.-t*t*(3.-2.*t)
    lo,hi=city_rect
    exterior=np.maximum(np.maximum(lo-points,points-hi),0)
    d=np.linalg.norm(exterior,axis=1);t=np.clip(d/3.5,0,1)
    # The old rectangular town infill was explicitly earth, including gaps in
    # the source ground footprint. Retain earth there and fade into meadow.
    return np.maximum(weight,1.-t*t*(3.-2.*t)).astype('<f4')


def append_view(doc,binary,data,**extra):
    binary+=b'\0'*((-len(binary))%4);offset=len(binary);binary+=data
    index=len(doc.setdefault('bufferViews',[]))
    doc['bufferViews'].append(dict(buffer=0,byteOffset=offset,byteLength=len(data),**extra))
    return binary,index


def append_texture(doc,binary,data):
    # Keep duplicate source textures shared within each material/model.
    for index,texture in enumerate(doc.get('textures',[])):
        image=doc['images'][texture['source']]
        if 'bufferView'not in image:continue
        view=doc['bufferViews'][image['bufferView']];start=view.get('byteOffset',0)
        if binary[start:start+view['byteLength']]==data:return binary,index
    binary,view=append_view(doc,binary,data)
    image=len(doc.setdefault('images',[]));doc['images'].append(dict(mimeType='image/png',bufferView=view))
    texture=len(doc.setdefault('textures',[]));doc['textures'].append(dict(source=image,sampler=0))
    return binary,texture


def write_glb(path,doc,binary):
    doc['buffers'][0]['byteLength']=len(binary);binary+=b'\0'*((-len(binary))%4)
    metadata=json.dumps(doc,separators=(',',':'),ensure_ascii=True).encode();metadata+=b' '*((-len(metadata))%4)
    raw=struct.pack('<III',0x46546c67,2,28+len(metadata)+len(binary))+struct.pack('<II',len(metadata),0x4e4f534a)+metadata+struct.pack('<II',len(binary),0x004e4942)+binary
    path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(raw)


def ground_model(source,dest,prepared,normals,segments,city_rect,town=False):
    original=GLB(source);doc=copy.deepcopy(original.doc);binary=original.binary
    original_materials=copy.deepcopy(doc['materials']);material_changes=[];weights=[]
    if town:
        for i,mat in enumerate(doc['materials']):
            if mat['name']!='MI_POM_Ground':continue
            mat['normalTexture']['scale']=.65;mat['pbrMetallicRoughness']['metallicFactor']=0.
            material_changes.append(dict(index=i,name=mat['name'],normal_scale_before=original_materials[i]['normalTexture'].get('scale',1.),normal_scale_after=.65,metallic_after=0.,source_albedo_and_layer_paint_preserved=True))
    else:
        textures={}
        for name in ['grassground','earthpath']:
            binary,textures[name+'_base']=append_texture(doc,binary,Path(prepared[name]['base']).read_bytes())
            binary,textures[name+'_normal']=append_texture(doc,binary,normals[name])
        unit=Path(prepared['earthpath']['arm']).read_bytes();binary,arm=append_texture(doc,binary,unit)
        for i,mat in enumerate(doc['materials']):
            name=mat['name']
            if name not in TARGET_NORMAL:continue
            mat['pbrMetallicRoughness']['metallicFactor']=0.
            mat['pbrMetallicRoughness']['roughnessFactor']=1.
            if name=='rocky':
                binary,normal=append_texture(doc,binary,normals['rocky']);mat['normalTexture']=dict(index=normal)
                material_changes.append(dict(index=i,name=name,metallic_after=0.,roughness_after=1.,normal_strength=TARGET_NORMAL[name]));continue
            mat['pbrMetallicRoughness']['baseColorTexture']=dict(index=textures['grassground_base'])
            mat['pbrMetallicRoughness']['metallicRoughnessTexture']=dict(index=arm)
            mat['normalTexture']=dict(index=textures['grassground_normal']);mat['occlusionTexture']=dict(index=arm)
            mat['extensions']={'VALWOOD_materials_facade':dict(parameters=[1,2,textures['earthpath_normal'],arm,textures['earthpath_base'],1,1,MARKER])}
            material_changes.append(dict(index=i,name=name,metallic_before=original_materials[i]['pbrMetallicRoughness']['metallicFactor'],metallic_after=0.,roughness_after=1.,continuous_two_layer_ground=True))
        doc.setdefault('extensionsUsed',[])
        if 'VALWOOD_materials_facade'not in doc['extensionsUsed']:doc['extensionsUsed'].append('VALWOOD_materials_facade')
        for mesh in doc['meshes']:
            for prim in mesh['primitives']:
                name=doc['materials'][prim['material']]['name']
                if name not in ['grassground','earthpath']:continue
                uv=original.read(prim['attributes']['TEXCOORD_0'])
                # Exact original four-metre planar world UVs encode native XZ.
                w=road_weight(uv.astype(np.float64)*4.,segments,city_rect)
                binary,view=append_view(doc,binary,w.tobytes(),target=34962)
                accessor=len(doc['accessors']);doc['accessors'].append(dict(bufferView=view,componentType=5126,count=len(w),type='SCALAR',min=[float(w.min())],max=[float(w.max())]))
                prim['attributes']['_VALWOOD_PAINT']=accessor;weights.append(dict(material=name,vertices=len(w),mixed_vertices=int(np.sum((w>0)&(w<1))),minimum=float(w.min()),maximum=float(w.max())))
    if not material_changes:raise ValueError('No eligible ground materials')
    write_glb(dest,doc,binary);after=GLB(dest)
    checks=dict(original_bin_prefix_byte_exact=after.binary[:len(original.binary)]==original.binary,
        original_accessors_exact=after.doc['accessors'][:len(original.doc['accessors'])]==original.doc['accessors'],
        original_views_exact=after.doc['bufferViews'][:len(original.doc['bufferViews'])]==original.doc['bufferViews'],
        nodes_and_scenes_exact=all(after.doc[k]==original.doc[k]for k in ['nodes','scenes']),
        source_images_exact=after.doc['images'][:len(original.doc['images'])]==original.doc['images'])
    positions=[];indices=0;weight_sites={};weight_delta=0.
    for oldmesh,newmesh in zip(original.doc['meshes'],after.doc['meshes']):
        for old,new in zip(oldmesh['primitives'],newmesh['primitives']):
            if old['indices']!=new['indices']:raise ValueError('Changed triangle index accessor')
            indices+=original.doc['accessors'][old['indices']]['count']
            for name,index in old['attributes'].items():
                if new['attributes'][name]!=index or not np.array_equal(original.read(index),after.read(index)):
                    raise ValueError('Changed original attribute '+name)
            positions.append(original.read(old['attributes']['POSITION']))
            if '_VALWOOD_PAINT'in new['attributes']:
                uv=after.read(new['attributes']['TEXCOORD_0']);paint=after.read(new['attributes']['_VALWOOD_PAINT']).ravel()
                for site,w in zip(uv,paint):
                    key=tuple(site.tobytes());previous=weight_sites.setdefault(key,float(w));weight_delta=max(weight_delta,abs(previous-float(w)))
    p=np.concatenate(positions);checks.update(all_original_attributes_byte_exact=True,triangle_indices_exact=True,
        duplicate_uv_weight_max_difference=weight_delta,bounds_exact=True,min_y_exact=True)
    if not all(v for k,v in checks.items()if isinstance(v,bool)) or weight_delta!=0:raise ValueError(('Visual overlay geometry audit failed',checks))
    return dict(source=str(Path(source).resolve()),source_sha256=sha(source),output=str(dest.resolve()),sha256=sha(dest),
        bytes=dest.stat().st_size,original_bytes=original.path.stat().st_size,triangles=indices//3,
        bounds=[p.min(0).tolist(),p.max(0).tolist()],checks=checks,materials=material_changes,paint=weights)


def build(project,output,catalog=CATALOG):
    project=Path(project).resolve();output=Path(output).resolve()
    if output.exists():raise ValueError('Use a new immutable output directory')
    output.mkdir(parents=True);data=json.loads(Path(catalog).read_text())
    prepared,normals,normal_audit=prepare_normals(output)
    road=np.asarray(data['countryside']['road_native']);segments=np.stack((road[:-1],road[1:]),axis=1)
    residential=np.asarray(data['residential_repair']['street_segments_native']);segments=np.concatenate((segments,residential))
    city=json.loads((project/'city_showcase.json').read_text());origin=np.asarray(city['origin'])[[0,2]]
    bounds=np.asarray(city['ground']['bounds']);city_rect=np.stack((bounds[:2]-origin,bounds[2:]-origin))
    rows=[];replacement=[];aliases=[];seam_sites={};maximum_seam_delta=0.
    for line in (project/'world.studio').read_text().splitlines():
        s=line.split()
        if len(s)<4 or s[0]!='asset':continue
        alias,prefix=s[1:3];town=alias=='nm_city_11_sm_ground_mesh'
        if not(alias.startswith('bep_country_terrain_')or town):continue
        aliases.append(alias)
        for suffix in ['.glb','_mid.glb']:
            relative=Path('models')/(prefix+suffix);source=project/relative
            label='near'if suffix=='.glb'else'mid';dest=output/'models'/alias/('asset_'+label+'.glb')
            row=ground_model(source,dest,prepared,normals,segments,city_rect,town);row.update(alias=alias,lod=label);rows.append(row)
            replacement.append(dict(relative_path=relative.as_posix(),candidate=str(dest),sha256=row['sha256']))
            if label=='near':replacement.append(dict(relative_path=(Path('models')/(prefix+'_near.glb')).as_posix(),candidate=str(dest),sha256=row['sha256']))
            if town:continue
            model=GLB(dest)
            for mesh in model.doc['meshes']:
                for prim in mesh['primitives']:
                    if '_VALWOOD_PAINT'not in prim['attributes']:continue
                    uv=model.read(prim['attributes']['TEXCOORD_0']);paint=model.read(prim['attributes']['_VALWOOD_PAINT']).ravel()
                    for site,w in zip(uv,paint):
                        key=tuple(site.tobytes());previous=seam_sites.setdefault(key,float(w));maximum_seam_delta=max(maximum_seam_delta,abs(previous-float(w)))
    if len(aliases)!=17 or len(rows)!=34:raise ValueError(('Expected sixteen terrain and one town ground alias',aliases))
    if maximum_seam_delta!=0:raise ValueError('Material weight differs across tile/LOD seam')
    report=dict(schema=1,status='CPU_VERIFIED_VISUAL_ONLY_OVERLAY',source_project=str(project),catalog=str(Path(catalog).resolve()),catalog_sha256=sha(catalog),
        aliases=aliases,model_count=len(rows),replacements=replacement,models=rows,normals=normal_audit,
        blend=dict(existing_bounded_facade_version=1,mode=2,marker_z=MARKER,uv_period_m=4.,road_inside_m=1.6,road_outside_m=4.3,
            camera_independent_edge_irregularity_max_m=.37,city_infill_feather_m=3.5,maximum_tile_and_lod_seam_weight_difference=maximum_seam_delta,
            shader_patch_required=True),
        source_projects_modified=False,source_images_modified=False,physics_and_height_fields_modified=False,
        limitation='Terrain material appearance and original town normal strength only. Source UVs, surface positions and topology remain exact. Requires visual GPU review with corrected lighting.')
    (output/'GROUND_FINISH_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n')
    shader=project/'shaders/world_foliage.fs.glsl';patched=patch_shader(shader.read_text())
    (output/'world_foliage.ground-patch.fs.glsl').write_text(patched)
    (output/'shader-patch-review.json').write_text(json.dumps(dict(source=str(shader),source_sha256=sha(shader),candidate_sha256=sha(patched.encode()),
        bounded_material_marker=MARKER,existing_material_modes_untouched=True,normal_aux_linear=True,albedo_aux_srgb=True,
        runtime_rebuild_required=False,source_shader_modified=False),indent=2)+'\n')
    return report


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--project',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--catalog',type=Path,default=CATALOG);args=ap.parse_args()
    result=build(args.project,args.output,args.catalog)
    print(json.dumps(dict(status=result['status'],model_count=result['model_count'],review=str(args.output/'GROUND_FINISH_REVIEW.json'),exact_geometry=True,seam_weight_maximum_difference=0),indent=2))


if __name__=='__main__':main()
