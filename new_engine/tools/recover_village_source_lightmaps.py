#!/usr/bin/env python3
"""Recover exact UE4.26 GI bindings and sample original HQ lightmaps offline.

No engine/project or frozen cooked asset writes. The original map/build archive
remains unchanged. Texture source mip bytes are decompressed only on demand.
"""
from pathlib import Path
import argparse,collections,hashlib,json,math,mmap,struct,zlib
import numpy as np
from audit_purchased_village_scene import ScenePackage,PRIVATE

BUILT=PRIVATE/'source-project/Medieval_Village/Content/Medieval_Village/Maps/Medieval_Village_BuiltData.uasset'
OUT=PRIVATE/'source-lightmaps'
FORMULA_SOURCE='https://github.com/folgerwang/UnrealEngine/blob/release/Engine/Shaders/Private/LightmapCommon.ush'

def component_guids(raw,start,size):
    end=start+size;cursor=start
    def number():
        nonlocal cursor
        if cursor+4>end:raise ValueError('LOD record exceeds component')
        value=struct.unpack_from('<I',raw,cursor)[0];cursor+=4;return value
    if number()!=0:raise ValueError('Unsupported component serialization prefix')
    count=number()
    if count>8:raise ValueError('Unbounded LOD count')
    result=[]
    for lod in range(count):
        if cursor+19>end:raise ValueError('LOD header exceeds component')
        if raw[cursor:cursor+2]!=b'\0\0':raise ValueError('Unsupported stripped LOD')
        guid=raw[cursor+2:cursor+18].hex();override=raw[cursor+18];cursor+=19
        if override:
            if raw[cursor:cursor+2]!=b'\0\0':raise ValueError('Unsupported stripped color data')
            cursor+=2;stride,vertices,item_size,n=(number()for _ in range(4))
            if stride!=4 or item_size!=4 or vertices!=n:raise ValueError('Color shape mismatch')
            cursor+=4*n
        n=number();cursor+=32*n
        if cursor>end:raise ValueError('Paint stream exceeds component')
        result.append(dict(lod=lod,guid=guid,override=bool(override)))
    return result,cursor

def registry(package):
    export=next(e for e in package.exports if package.object(e['cls'])=='MapBuildDataRegistry')
    props=package.props(export['index']);start=package.property_cache[export['index']]['native_tail_offset'];b=package.data
    if b[start:start+6]!=b'\0'*6:raise ValueError('Unsupported build registry prefix')
    count=struct.unpack_from('<I',b,start+6)[0];cursor=start+10;result={};counts=collections.Counter()
    for index in range(count):
        begin=cursor;guid=b[cursor:cursor+16].hex();typ=struct.unpack_from('<I',b,cursor+16)[0];cursor+=20
        if guid in result or typ not in(0,2):raise ValueError(('Invalid lightmap record',index,begin,typ))
        row=dict(source_offset=begin,lightmap_type=typ,record=index)
        if typ==2:
            lights=struct.unpack_from('<I',b,cursor)[0]
            if lights>1000:raise ValueError('Invalid light GUID count')
            extra=16*lights;row['light_guids']=[b[cursor+4+i*16:cursor+20+i*16].hex()for i in range(lights)]
            objects=struct.unpack_from('<4i',b,begin+24+extra)
            row['textures']={name:package.ref(obj)for name,obj in zip(['HQ','LQ','sky_occlusion','AO_material_mask'],objects)}
            coeff=np.frombuffer(b,dtype='<f4',count=32,offset=begin+40+extra).reshape(4,2,4)
            if not np.isfinite(coeff).all():raise ValueError('Nonfinite lightmap coefficients')
            row['scale']=coeff[:,0].tolist();row['add']=coeff[:,1].tolist()
            uv=np.frombuffer(b,dtype='<f4',count=4,offset=begin+168+extra)
            row['uv_scale']=uv[:2].tolist();row['uv_bias']=uv[2:].tolist()
            # Preserve the remaining native LightMap2D body byte-for-byte;
            # source sampling below uses only fields proven against the shader.
            row['remaining_lightmap_native_hex']=b[begin+184+extra:begin+220+extra].hex()
            cursor+=200+extra
        shadow,irrelevant=struct.unpack_from('<II',b,cursor)
        if shadow!=0 or irrelevant>1000:raise ValueError(('Unsupported source shadow map',index,cursor,shadow,irrelevant))
        row['shadowmap_type']=shadow;row['irrelevant_light_guids']=[b[cursor+8+i*16:cursor+24+i*16].hex()for i in range(irrelevant)];cursor+=8+16*irrelevant
        stride,n=struct.unpack_from('<II',b,cursor);cursor+=8
        if stride!=16 or n>100000:raise ValueError('Invalid per-instance lightmap bulk shape')
        values=np.frombuffer(b,dtype='<f4',count=n*4,offset=cursor).reshape(-1,4);cursor+=n*16
        if not np.isfinite(values).all():raise ValueError('Nonfinite instance lightmap coordinates')
        row.update(per_instance_uv_bias=values.tolist(),per_instance_count=n,serialized_bytes=cursor-begin)
        result[guid]=row;counts[typ]+=1
    return dict(source_properties=props,records=result,record_count=count,parsed_start=start,parsed_end=cursor,
        type_counts=dict(counts),per_instance_records=sum(r['per_instance_count']for r in result.values()))

def textures(package):
    base=max(e['offset']+e['size']for e in package.exports);result={}
    for e in package.exports:
        if package.object(e['cls'])!='LightMapTexture2D':continue
        props=package.props(e['index']);source=props['Source'];tail=package.property_cache[e['index']];off=tail['native_tail_offset'];b=package.data
        if b[off:off+6]!=b'\0'*6:raise ValueError('Unsupported lightmap native prefix')
        flags,raw_count,compressed,relative=struct.unpack_from('<IIIQ',b,off+6)
        if flags!=3 or source['Format']!='TSF_BGRA8' or source.get('NumSlices')!=1:raise ValueError(('Unsupported HQ source pixels',e['name'],flags,source))
        w,h=source['SizeX'],source['SizeY'];expected=sum(max(1,w>>i)*max(1,h>>i)*4 for i in range(source['NumMips']))
        if raw_count!=expected:raise ValueError(('Lightmap source mip byte mismatch',e['name'],raw_count,expected))
        result[str(e['index'])]=dict(name=e['name'],export_index=e['index'],width=w,height=h,mips=source['NumMips'],raw_bytes=raw_count,
            source_format=source['Format'],srgb=props.get('SRGB',True),bulk_offset=base+relative,compressed_bytes=compressed,raw_base_bytes=w*h*4,source_properties=props)
    return result

def recover(output=OUT):
    scene=json.loads((PRIVATE/'publisher-scene.json').read_text());p=ScenePackage(BUILT);reg=registry(p);tx=textures(p);components=[];missing=[];duplicate=[];usage=collections.Counter()
    with Path(scene['source']).open('rb')as stream,mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ)as raw:
        for c in scene['placements']+scene['native_instance_components']:
            lods,parsed=component_guids(raw,c['native_tail_offset'],c['native_tail_bytes'])
            for lod in lods:
                if lod['guid']=='0'*32:continue
                record=reg['records'].get(lod['guid']);row=dict(component=c['export_index'],actor=c.get('actor_label'),mesh=c.get('mesh'),class_name=c['class_name'],lod=lod['lod'],guid=lod['guid'])
                if record:
                    row.update(lightmap_type=record['lightmap_type'],HQ_texture_index=record.get('textures',{}).get('HQ',{}).get('index')if record.get('textures',{}).get('HQ')else None,
                        per_instance_count=record['per_instance_count']);usage[lod['guid']]+=1
                else:missing.append(row)
                components.append(row)
    duplicate=[dict(guid=k,bindings=n)for k,n in usage.items()if n>1]
    result=dict(schema=1,source=str(BUILT.resolve()),source_sha256=hashlib.sha256(p.data).hexdigest(),map_source_sha256=scene['source_sha256'],
        formula_source=FORMULA_SOURCE,coordinates='Original UE local UV1, source-world normals in Unreal XYZ; no UV1 V flip',
        source_unmodified=True,source_lightmaps_indirect_only=True,indirect_only_evidence='Map DirectionalLight and SkyLight are explicitly Movable; preserve runtime direct sun in addition to source GI.',
        registry=reg,textures=tx,component_bindings=components,missing_bindings=missing,duplicate_guid_bindings=duplicate,
        unused_registry_guids=[k for k in reg['records']if k not in usage],
        limitations=['Per-instance foliage bias counts join exactly; texture-space instance order must retain original PerInstanceSMData order, not reordered render-cluster order.',
        'Explicit component LOD GUIDs only; no unproven cross-LOD lighting reuse is invented.',
        'Original ALL FBX UV1 must be validated against UE generated UV1 before calling sampled GI exact.'])
    output.mkdir(parents=True,exist_ok=True);(output/'manifest.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    print('READY',output/'manifest.json','registry',reg['record_count'],'textures',len(tx),'component LOD joins',len(components),'missing',len(missing),'foliage UVbias',reg['per_instance_records'],flush=True)
    return result

class LightmapSampler:
    def __init__(self,manifest=OUT/'manifest.json',cache=OUT/'mip-cache',lod=2):
        self.manifest=json.loads(Path(manifest).read_text());self.source=Path(self.manifest['source']);self.cache=Path(cache);self.cache.mkdir(parents=True,exist_ok=True);self.lod=lod;self.loaded=collections.OrderedDict()
    def _image(self,index):
        key=(str(index),self.lod)
        if key in self.loaded:self.loaded.move_to_end(key);return self.loaded[key]
        info=self.manifest['textures'][str(index)];mip=min(self.lod,info['mips']-1);target=self.cache/(info['name']+'_mip'+str(mip)+'.npy')
        if not target.exists():
            with self.source.open('rb')as f:
                f.seek(info['bulk_offset']);head=f.read(32);magic,chunk,total_compressed,total_raw=struct.unpack('<QQQQ',head)
                if magic!=0x9e2a83c1 or total_raw!=info['raw_bytes']or chunk!=131072:raise ValueError('Unexpected lightmap compressed source header')
                count=math.ceil(total_raw/chunk);sizes=[struct.unpack('<QQ',f.read(16))for _ in range(count)]
                start=sum(max(1,info['width']>>i)*max(1,info['height']>>i)*4 for i in range(mip));w=max(1,info['width']>>mip);h=max(1,info['height']>>mip);end=start+w*h*4
                output=bytearray();position=0;consumed=0
                for compressed,raw in sizes:
                    block_start=position;position+=raw;consumed+=compressed
                    if position>start and block_start<end:
                        encoded=f.read(compressed);block=zlib.decompress(encoded)
                        if len(block)!=raw:raise ValueError('Corrupt source lightmap chunk')
                        output.extend(block[max(0,start-block_start):min(raw,end-block_start)])
                    else:f.seek(compressed,1)
                if consumed!=total_compressed or len(output)!=w*h*4:raise ValueError('Lightmap source byte boundary mismatch')
                # Source rawFColor is BGRA; transfer is linear (SRGB false).
                rgba=np.frombuffer(output,dtype=np.uint8).reshape(h,w,4)[:,:,[2,1,0,3]].copy();np.save(target,rgba)
        image=np.load(target,mmap_mode='r');self.loaded[key]=image
        while len(self.loaded)>2:self.loaded.popitem(last=False)
        return image
    @staticmethod
    def bilinear(image,uv):
        h,w=image.shape[:2];p=np.asarray(uv,dtype=np.float64)*[w,h]-.5;p=np.clip(p,[0,0],[w-1,h-1]);lo=np.floor(p).astype(np.int64);hi=np.minimum(lo+1,[w-1,h-1]);t=p-lo
        return ((image[lo[:,1],lo[:,0]]/255.)*(1-t[:,0,None])*(1-t[:,1,None])+
            (image[lo[:,1],hi[:,0]]/255.)*t[:,0,None]*(1-t[:,1,None])+
            (image[hi[:,1],lo[:,0]]/255.)*(1-t[:,0,None])*t[:,1,None]+
            (image[hi[:,1],hi[:,0]]/255.)*t[:,0,None]*t[:,1,None])
    def sample(self,guid,uv1,normals_source_world,instance_index=None):
        uv=np.asarray(uv1,dtype=np.float64);normal=np.asarray(normals_source_world,dtype=np.float64);row=self.manifest['registry']['records'].get(guid)
        out=np.zeros((len(uv),4),np.float32)
        if row is None or row['lightmap_type']!=2:return out
        tex=row['textures']['HQ'];bias=np.asarray(row['uv_bias'])
        if instance_index is not None:
            if not 0<=instance_index<row['per_instance_count']:raise ValueError('Foliage GI instance index outside authored data')
            bias=np.asarray(row['per_instance_uv_bias'][instance_index][:2])
        coord=uv*np.asarray(row['uv_scale'])+bias;valid=np.isfinite(coord).all(1)&np.isfinite(normal).all(1)&(coord>=0).all(1)&(coord<=1).all(1)
        if not valid.any():return out
        image=self._image(tex['index']);tc=coord[valid].copy();tc[:,1]*=.5;q0=self.bilinear(image,tc);tc[:,1]+=.5;q1=self.bilinear(image,tc)
        scale=np.asarray(row['scale']);add=np.asarray(row['add']);logl=(q0[:,3]+(q1[:,3]-.5)/255.)*scale[0,3]+add[0,3]
        chroma=q0[:,:3]**2*scale[0,:3]+add[0,:3];l=np.maximum(np.exp2(logl)-.01858136,0)
        direction=q1*scale[1]+add[1];normal4=np.column_stack((normal[valid][:,[1,2,0]],np.ones(np.count_nonzero(valid))))
        d=np.maximum(np.sum(direction*normal4,axis=1),0);rgb=chroma*l[:,None]*d[:,None]
        if not np.isfinite(rgb).all():raise ValueError('Nonfinite decoded source GI')
        out[valid,:3]=rgb;out[valid,3]=1;return out

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output',type=Path,default=OUT);ap.add_argument('--smoke',action='store_true');args=ap.parse_args();r=recover(args.output)
    if args.smoke:
        sample=LightmapSampler(args.output/'manifest.json',args.output/'mip-cache',lod=2);fixtures=[]
        for binding in r['component_bindings'][:12]:
            if binding['lightmap_type']!=2:continue
            a=sample.sample(binding['guid'],np.array([[.25,.25],[.5,.5],[.75,.75]]),np.tile([0,0,1],(3,1)))
            fixtures.append(dict(binding=binding,rgba=a.tolist()))
        (args.output/'sampler-smoke.json').write_text(json.dumps(fixtures,indent=2)+'\n');print('SAMPLER SMOKE PASS',len(fixtures),flush=True)
if __name__=='__main__':main()
