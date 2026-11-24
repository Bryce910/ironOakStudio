#!/usr/bin/env python3
"""Independently inspect the owned Orc bake; this never accepts game rendering."""
from pathlib import Path
import argparse
import hashlib
import io
import json
import math
import struct
import sys
import urllib.parse
import numpy as np
from PIL import Image

STAGE=Path(__file__).resolve().parent
REPORT=STAGE/'opponent-review/ORC_ASSET_CONTRACT_REVIEW.json'
DTYPES={5120:'i1',5121:'u1',5122:'<i2',5123:'<u2',5125:'<u4',5126:'<f4'}
DIM={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}

def require(condition,message):
    if not condition:raise RuntimeError(message)

def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def pin(path,inputs):
    path=Path(path).resolve();require(path.is_file(),'Missing required file: '+str(path))
    value=sha(path);inputs[str(path)]=value;return value

def local(path,root):
    p=Path(path);return (p if p.is_absolute() else root/p).resolve()

def hashes(value,root,label,inputs):
    require(isinstance(value,(dict,list)) and len(value)>0,'Empty manifest '+label)
    rows=value.items() if isinstance(value,dict) else ((row['path'],row['sha256']) for row in value)
    result={}
    for name,expected in rows:
        p=local(name,root);actual=pin(p,inputs)
        require(isinstance(expected,str) and len(expected)==64 and actual==expected,'Manifest '+label+' hash mismatch: '+str(p))
        result[str(p)]=actual
    return result

class Glb:
    def __init__(self,path,inputs):
        self.path=Path(path).resolve();self.inputs=inputs
        pin(self.path,inputs);self.data=self.path.read_bytes()
        require(20<=len(self.data)<=32*1024*1024,'GLB exceeds the production intake limit: '+str(path))
        magic,version,size=struct.unpack_from('<III',self.data)
        require((magic,version,size)==(0x46546C67,2,len(self.data)),'Invalid GLB header: '+str(path))
        offset=12;chunks=[]
        while offset<len(self.data):
            require(offset+8<=len(self.data),'Truncated GLB chunk')
            length,kind=struct.unpack_from('<II',self.data,offset);offset+=8
            require(length%4==0 and offset+length<=len(self.data),'Invalid GLB chunk size')
            chunks.append((kind,self.data[offset:offset+length]));offset+=length
        require(chunks and chunks[0][0]==0x4E4F534A,'GLB JSON must be first')
        self.doc=json.loads(chunks[0][1].decode().rstrip(' \t\r\n\x00'))
        self.buffers=[];binary=[v for k,v in chunks if k==0x004E4942]
        for index,buffer in enumerate(self.doc.get('buffers',[])):
            if 'uri' in buffer:self.buffers.append(self.uri(buffer['uri']))
            else:
                require(index==0 and len(binary)==1,'Unsupported GLB buffer layout')
                self.buffers.append(binary[0])
            require(buffer['byteLength']<=len(self.buffers[-1]),'Truncated GLB buffer')
        self.primitives=[]
        # The actual bake contract is one identity node containing every
        # material primitive, so GLB and native metadata traversal agree.
        mesh_nodes=[n for n in self.doc.get('nodes',[]) if 'mesh' in n]
        require(len(mesh_nodes)==1,'Expected one baked mesh node: '+str(path))
        node=mesh_nodes[0]
        require('skin' not in node and not self.doc.get('animations'),'Body/cap bank must contain baked static poses')
        require(node.get('translation',[0,0,0])==[0,0,0] and node.get('rotation',[0,0,0,1])==[0,0,0,1] and node.get('scale',[1,1,1])==[1,1,1],'Baked mesh node is not identity')
        if 'matrix' in node:require(np.max(np.abs(np.array(node['matrix']).reshape(4,4)-np.eye(4)))<1e-7,'Baked node matrix is not identity')
        self.primitives=self.doc['meshes'][node['mesh']]['primitives']
        require(1<=len(self.primitives)<=32,'Invalid production mesh count')

    def uri(self,uri):
        require(not uri.startswith('data:'),'Unexpected data URI; bake should use embedded buffer views or pinned files')
        parsed=urllib.parse.urlsplit(uri)
        require(not parsed.scheme and not parsed.netloc and not parsed.query and not parsed.fragment,'Remote/unpinned asset URI')
        p=(self.path.parent/urllib.parse.unquote(parsed.path)).resolve()
        require(p.is_relative_to(self.path.parent.parent),'Asset URI escapes Orc output')
        pin(p,self.inputs);return p.read_bytes()

    def accessor(self,index):
        a=self.doc['accessors'][index]
        require('sparse' not in a and a['componentType'] in DTYPES and a['type'] in DIM,'Unsupported accessor')
        view=self.doc['bufferViews'][a['bufferView']]
        dtype=np.dtype(DTYPES[a['componentType']]);columns=DIM[a['type']];count=a['count']
        require(0<count<=10000000,'Invalid accessor count')
        stride=view.get('byteStride',dtype.itemsize*columns)
        require(stride>=dtype.itemsize*columns,'Invalid accessor stride')
        within=a.get('byteOffset',0);end=within+(count-1)*stride+dtype.itemsize*columns
        require(end<=view['byteLength'],'Accessor exceeds its view')
        start=view.get('byteOffset',0)+within;buffer=self.buffers[view.get('buffer',0)]
        require(start+(count-1)*stride+dtype.itemsize*columns<=len(buffer),'Accessor exceeds buffer')
        result=np.ndarray((count,columns),dtype=dtype,buffer=buffer,offset=start,strides=(stride,dtype.itemsize))
        require(np.isfinite(result).all(),'Nonfinite vertex/index data')
        return result

    def geometry(self,reference=None):
        result=[];bounds=[]
        for primitive in self.primitives:
            require(primitive.get('mode',4)==4 and 'indices' in primitive,'Expected indexed triangles')
            attributes=primitive['attributes'];require({'POSITION','NORMAL','TEXCOORD_0'}<=attributes.keys(),'Missing position/normal/UV')
            position=self.accessor(attributes['POSITION']).astype('<f4',copy=False)
            normal=self.accessor(attributes['NORMAL']);uv=self.accessor(attributes['TEXCOORD_0']).astype('<f4',copy=False)
            index=self.accessor(primitive['indices']).reshape(-1).astype('<u4')
            require(position.shape[1]==normal.shape[1]==3 and uv.shape[1]==2 and len(position)==len(normal)==len(uv),'Attribute layout mismatch')
            require(len(index)%3==0 and int(index.max())<len(position),'Triangle index out of range')
            require(np.max(np.abs(position))<=32 and np.max(np.abs(uv))<1000,'Unbounded position/UV data')
            require(np.min(np.linalg.norm(normal,axis=1))>.1,'Missing/degenerate vertex normals')
            topology=uv.tobytes()+index.tobytes();digest=hashlib.sha256(topology).hexdigest()
            prior=reference[len(result)] if reference is not None and len(reference)>len(result) else None
            if prior and prior['topology_sha256']==digest:key=prior['layout_fnv64']
            else:
                key=14695981039346656037
                for value in topology:key=((key^value)*1099511628211)&0xffffffffffffffff
            result.append({'vertices':len(position),'triangles':len(index)//3,'layout_fnv64':key,'topology_sha256':digest,'material':primitive.get('material',0)})
            bounds.extend([position.min(axis=0),position.max(axis=0)])
        bounds=np.array(bounds)
        return result,[bounds.min(axis=0).tolist(),bounds.max(axis=0).tolist()]

    def image(self,index):
        image=self.doc['images'][index]
        if 'uri' in image:return self.uri(image['uri'])
        view=self.doc['bufferViews'][image['bufferView']];buffer=self.buffers[view.get('buffer',0)]
        start=view.get('byteOffset',0);require(start+view['byteLength']<=len(buffer),'Texture exceeds buffer')
        return buffer[start:start+view['byteLength']]

    def materials(self,cache):
        images={};roles={}
        for primitive in self.primitives:
            material=self.doc['materials'][primitive.get('material',0)]
            pbr=material.get('pbrMetallicRoughness',{})
            for role,texture in [('albedo',pbr.get('baseColorTexture')),('normal',material.get('normalTexture')),('metallic_roughness',pbr.get('metallicRoughnessTexture')),('occlusion',material.get('occlusionTexture'))]:
                if not texture:continue
                source=self.doc['textures'][texture['index']]['source'];data=self.image(source);digest=hashlib.sha256(data).hexdigest()
                if digest not in images:
                    if digest not in cache:
                        with Image.open(io.BytesIO(data)) as decoded:
                            require(min(decoded.size)>=64 and max(decoded.size)<=16384,'Invalid texture dimensions')
                            size=list(decoded.size);decoded.thumbnail((128,128));pixels=np.array(decoded.convert('RGB'),dtype=np.float32)
                            cache[digest]={'sha256':digest,'dimensions':size,'mean_rgb':pixels.mean(axis=(0,1)).tolist(),'pixel_std':float(pixels.std())}
                    images[digest]=cache[digest]
                roles.setdefault(role,set()).add(digest)
        return {'images':images,'roles':{k:sorted(v) for k,v in roles.items()}}

class Binary:
    def __init__(self,path,inputs):self.path=Path(path);pin(self.path,inputs);self.data=self.path.read_bytes();self.at=0
    def take(self,n):
        require(n>=0 and self.at+n<=len(self.data),'Truncated binary metadata: '+str(self.path))
        value=self.data[self.at:self.at+n];self.at+=n;return value
    def u32(self):return struct.unpack('<I',self.take(4))[0]
    def floats(self,n):
        v=np.frombuffer(self.take(n*4),dtype='<f4');require(np.isfinite(v).all() and np.max(np.abs(v),initial=0)<=32,'Invalid metadata floats');return v
    def end(self):require(self.at==len(self.data),'Unexpected trailing metadata')

def metadata(body,anatomy,layouts,inputs):
    socket=Binary(body/'combat_weapon_sockets.bin',inputs)
    require(socket.take(8)==b'VWMOT172' and socket.u32()==172,'Wrong socket header')
    edges=socket.floats(172*6).reshape(172,2,3);socket.end()
    lengths=np.linalg.norm(edges[:,1].astype(np.float64)-edges[:,0].astype(np.float64),axis=1)
    require(np.max(np.abs(edges))<=16 and lengths.min()>=.04 and lengths.max()<=4,'Invalid actual weapon edges')
    pivots=Binary(body/'combat_weapon_pivots.bin',inputs)
    require(pivots.take(8)==b'VWGRP172' and pivots.u32()==172,'Wrong measured pivot header')
    values=pivots.floats(172*3);pivots.end();require(np.max(np.abs(values))<=16,'Invalid measured weapon pivots')
    cfg=body/'combat_presentation.cfg';pin(cfg,inputs);tokens=cfg.read_text().split()
    require(len(tokens)>=11 and tokens[:5]==['VALWOOD_COMBAT_PRESENTATION','1','upper_body_aimed','1','contact_frames'] and tokens[-1]=='end','Invalid explicit species presentation')
    frames=list(map(float,tokens[5:10]));require(len(frames)==5 and all(math.isfinite(f) and .88<=f<=6.38 for f in frames),'Invalid presentation contact keys')
    optional=tokens[10:-1];require(len(optional)%2==0,'Malformed measured presentation fields')
    profile={}
    for key,value in zip(optional[::2],optional[1::2]):
        require(key in {'walk_stride_m','body_radius_m','body_height_m'} and key not in profile,'Unknown/duplicate measured presentation field')
        profile[key]=float(value)
    require(profile.keys()=={'walk_stride_m','body_radius_m','body_height_m'} and all(math.isfinite(v) for v in profile.values()),'Actual opponent stride/body dimensions are required')
    require(.25<=profile['walk_stride_m']<=4 and .15<=profile['body_radius_m']<=.75 and 1<=profile['body_height_m']<=3.5 and profile['body_height_m']>=2*profile['body_radius_m'],'Invalid measured opponent presentation profile')
    blob=Binary(anatomy/'anatomy.bin',inputs)
    require(blob.take(8)==b'VWANA002' and blob.u32()==172,'Wrong anatomy header')
    meshes=blob.u32();require(meshes==len(layouts) and blob.u32()==8,'Anatomy mesh/part count mismatch')
    coverage=np.zeros(8,dtype=np.int64);weight_sum_range=[float('inf'),0.]
    for mesh in layouts:
        count=blob.u32();low=blob.u32();high=blob.u32();key=low|(high<<32)
        require(count==mesh['vertices'] and key==mesh['layout_fnv64'],'Anatomy layout does not describe actual Orc body')
        labels=blob.floats(count*8).reshape(count,8)
        require(np.min(labels)>=0 and np.max(labels)<=1,'Invalid anatomical part weights')
        # Hierarchical arm/hand masks may legitimately overlap; the runtime
        # contract bounds each part independently rather than their sum.
        sums=labels.sum(axis=1);weight_sum_range[0]=min(weight_sum_range[0],float(sums.min()));weight_sum_range[1]=max(weight_sum_range[1],float(sums.max()))
        coverage+=(labels>.001).sum(axis=0)
    require((coverage>=4).all(),'One or more severable body parts has no actual mesh coverage')
    capsule_counts=[];hull_counts=[]
    for pose in range(172):
        wounds=blob.floats(8*6).reshape(8,2,3)
        require(np.min(np.linalg.norm(wounds[:,1],axis=1))>.01,'Invalid wound emission axes')
        count=blob.u32();require(1<=count<=16,'Invalid anatomy capsule count');capsule_counts.append(count)
        for _ in range(count):
            part=blob.u32();values=blob.floats(7)
            require(1<=part<=8 and np.float32(.015)<=values[6]<=np.float32(.65),'Invalid anatomy capsule')
        for _ in range(8):
            points=blob.u32();require(4<=points<=2048,'Invalid detached support hull');blob.floats(points*3);hull_counts.append(points)
    blob.end()
    return {'socket_lengths_m':[float(lengths.min()),float(lengths.max())],'measured_pivots':172,'anatomy_meshes':meshes,'part_vertex_coverage':coverage.tolist(),'part_weight_sum_range':weight_sum_range,'pose_capsules_range':[min(capsule_counts),max(capsule_counts)],'support_hull_points_range':[min(hull_counts),max(hull_counts)],'contact_frames':frames,'measured_profile':profile}

def inspect(manifest,geometry_only=False):
    inputs={};manifest=Path(manifest).resolve();pin(manifest,inputs);doc=json.loads(manifest.read_text());root=manifest.parent
    require(doc.get('schema')=='valwood.power-orc.arena-assets.v1' and doc.get('source_name')=='PowerOrc14','Actual PowerOrc14 manifest required')
    archives=hashes(doc['source_archives'],root,'source_archives',inputs)
    require(any('5973135' in p and 'Animations' in p for p in archives) and any('5973135' in p and 'Base' in p for p in archives),'Missing matching purchased PowerOrc14 source archives')
    sources=hashes(doc['source_files'],root,'source_files',inputs)
    require(any('PowerOrc14' in p and 'Attack' in p for p in sources),'Missing real purchased Orc animation source')
    hashes(doc['generator'],root,'generator',inputs)
    outputs=hashes(doc['outputs'],root,'outputs',inputs)
    body=local(doc['body_directory'],root);anatomy=local(doc['anatomy_directory'],root)
    require(body.is_relative_to(root) and anatomy.is_relative_to(root),'Orc output directories escape manifest root')
    material_cfg=root/'ORC_MATERIALS.cfg';pin(material_cfg,inputs);material_tokens=material_cfg.read_text().split()
    require(len(material_tokens)==5 and material_tokens[:3]==['VALWOOD_ORC_MATERIALS','1','original_textures'] and material_tokens[3] in {'0','1'} and material_tokens[4]=='end','Invalid exact Orc texture status config')
    require((material_tokens[3]=='1')==bool(doc.get('original_textures_present',False)),'Manifest and native Orc texture status disagree')
    first=None;body_bounds=[];caps=[];textures=None;texture_cache={};material_structure=None
    for pose in range(172):
        glb=Glb(body/f'combat_body_{pose:03}.glb',inputs);layout,bounds=glb.geometry(first)
        current_textures=glb.materials(texture_cache)
        current_structure={'materials':glb.doc.get('materials',[]),'textures':glb.doc.get('textures',[])}
        if first is None:first=layout;textures=current_textures;material_structure=current_structure
        require(layout==first,'Orc pose topology/material assignment changed: '+str(pose))
        require(current_textures==textures and current_structure==material_structure,'Orc material/texture content changes between poses: '+str(pose))
        body_bounds.append(bounds)
        cap=Glb(anatomy/'caps'/f'cap_{pose:03}.glb',inputs);cap_layout,_=cap.geometry(caps[0] if caps else None)
        require(len(cap_layout)==8,'Expected eight actual severed-part cut cap primitives')
        if caps:require(cap_layout==caps[0],'Cut cap topology changes between poses')
        caps.append(cap_layout)
    idle_height=body_bounds[0][1][1]-body_bounds[0][0][1]
    require(1<=idle_height<=4,'Orc scale is not plausible actor-local metres')
    albedo=textures['roles'].get('albedo',[])
    actual_albedo=bool(albedo) and any(max(textures['images'][key]['mean_rgb'])<250 and textures['images'][key]['pixel_std']>2 for key in albedo)
    if not geometry_only:
        require(doc.get('original_textures_present') is True and doc.get('materials_provisional') is False,'Original purchased texture provenance remains pending; authored placeholder colors cannot pass strict intake')
        require(any(Path(p).suffix.lower() in {'.png','.jpg','.jpeg','.tga','.tif','.tiff','.exr','.dds'} for p in sources),'Original texture sources are not pinned in source_files')
        require(actual_albedo,'Actual purchased Orc albedo textures are missing/blank; geometry-only bake cannot be released')
    details=metadata(body,anatomy,first,inputs)
    # Every generated dependency must be covered by the producer manifest.
    generated={p:h for p,h in inputs.items() if Path(p).is_relative_to(root) and p!=str(manifest)}
    for p,h in generated.items():require(outputs.get(p)==h,'Generated file is not pinned in ORC_ASSETS outputs: '+p)
    require(inputs=={p:sha(Path(p)) for p in inputs},'Asset dependencies changed during independent inspection')
    return {'schema':'valwood.power-orc.asset-contract-review.v1','passed':True,'accepted':not geometry_only,'geometry_only':geometry_only,
        'actual_orc_render_acceptance':False,'source_name':'PowerOrc14','materials_provisional':doc.get('materials_provisional',True),'original_textures_present':doc.get('original_textures_present',False),'body_poses':172,'cap_poses':172,'body_layouts':first,
        'idle_bounds_m':body_bounds[0],'idle_height_m':idle_height,'purchased_albedo_present':actual_albedo,'texture_review':textures,'metadata':details,
        'inputs':inputs,'manifest':str(manifest),'manifest_sha256':inputs[str(manifest)],
        'limits':['Independent file/geometry/material/metadata contract only; native loading, actual contacts, appearance and animation remain separate requirements.',
                  'No physical-controller feel or hardware performance claim.']}

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--manifest',type=Path,default=STAGE/'arena-project/arena_orcs/ORC_ASSETS.json')
    parser.add_argument('--geometry-only',action='store_true');parser.add_argument('--check-only',action='store_true');args=parser.parse_args()
    if args.check_only:
        selected=REPORT.with_name('ORC_GEOMETRY_ONLY_REVIEW.json') if args.geometry_only else REPORT
        receipt=json.loads(selected.read_text())
        require(receipt.get('passed') is True and receipt.get('geometry_only')==args.geometry_only,'Requested actual Orc contract has not passed')
        if not args.geometry_only:require(receipt.get('accepted') is True and receipt.get('purchased_albedo_present') is True,'Actual textured Orc contract not accepted')
        require(receipt['manifest']==str(args.manifest.resolve()),'Different Orc manifest selected')
        require(receipt['inputs']=={p:sha(Path(p)) for p in receipt['inputs']},'Accepted Orc inputs changed')
        print('PASS current actual PowerOrc14 '+('geometry-only prototype contract; original textures not accepted' if args.geometry_only else 'textured asset contract'));return
    result=inspect(args.manifest,args.geometry_only);result['inputs'][str(Path(__file__).resolve())]=sha(Path(__file__).resolve())
    output=REPORT.with_name('ORC_GEOMETRY_ONLY_REVIEW.json') if args.geometry_only else REPORT
    output.parent.mkdir(exist_ok=True);output.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
    print(json.dumps({k:result[k] for k in ['passed','accepted','geometry_only','body_poses','cap_poses','idle_height_m','purchased_albedo_present']}))

if __name__=='__main__':
    try:main()
    except (RuntimeError,KeyError,ValueError,OSError,struct.error) as error:
        print('Orc asset contract rejected: '+str(error),file=sys.stderr);sys.exit(1)
