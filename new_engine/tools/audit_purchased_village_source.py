#!/usr/bin/env python3
"""Read the purchased UE4.26 package as data; recover material metadata/PNGs.

This intentionally does not import Unreal or run anything from the source
project. Only the small, version-checked package/property subset used by these
material instances is understood. TextureSource extraction uses the full PNG
bulk payload, never the 256px editor thumbnail.
"""
from pathlib import Path
import argparse
import hashlib
import io
import json
import math
import struct
import zlib

ROOT=Path(__file__).resolve().parents[1]
DEFAULT_SOURCE=Path('/home/computer/Downloads/Medieval_Village/Content/Medieval_Village')
DEST=ROOT/'assets/purchased/medieval_village'

def canonical_texture_source_png(encoded,source_format):
    """Decode TextureSource storage channels, not an exported image's labels.

    The supplied TSF_BGRA8 bulk PNG preserves BGRA byte order. A normal PNG
    decoder therefore exposes stored B,G,R,A as if it were R,G,B,A. This is
    proven against matching vendor-exported archive PNGs pixel-for-pixel.
    Normalize exactly once at extraction; do not reinterpret ARM in shaders.
    """
    from PIL import Image
    image=Image.open(io.BytesIO(encoded));image.load()
    if source_format=='TSF_BGRA8':
        r,g,b,a=image.convert('RGBA').split();image=Image.merge('RGBA',(b,g,r,a))
    elif source_format not in {'TSF_G8','TSF_RGBA8'}:
        raise ValueError('Unsupported TextureSource format: '+str(source_format))
    output=io.BytesIO();image.save(output,format='PNG',optimize=True)
    return output.getvalue()


class Package:
    def __init__(self,path):
        self.path=Path(path); self.data=self.path.read_bytes(); self.pos=0
        assert self.i32()==-1641380927, 'Not an Unreal package'
        assert self.i32()==-7, 'Only the supplied legacy package layout is supported'
        self.i32(); assert self.i32()==522, 'Only the supplied UE4.26 property format is supported'
        self.i32(); custom=self.i32(); assert 0<=custom<=100
        self.pos+=20*custom; self.header=self.i32(); self.string(); self.pos+=4
        nc=self.i32(); no=self.i32(); self.string(); self.pos+=8
        ec=self.i32(); eo=self.i32(); ic=self.i32(); io=self.i32()
        assert 0<nc<10000 and 0<ec<10000 and 0<ic<10000
        self.pos=no; self.names=[]
        for _ in range(nc): self.names.append(self.string()); self.pos+=4
        self.imports=[]; self.pos=io
        for _ in range(ic):
            self.name(); self.name(); self.i32(); name=self.name(); self.pos+=8
            self.imports.append(name)
        self.exports=[]
        assert eo+104*ec<=self.header
        for i in range(ec):
            self.pos=eo+104*i; cls=self.i32(); self.pos+=12; name=self.name(); self.pos+=4
            size=self.i64(); offset=self.i64()
            assert 0<=offset<=offset+size<=len(self.data)
            self.exports.append(dict(name=name,cls=cls,size=size,offset=offset))

    def i32(self):
        value=struct.unpack_from('<i',self.data,self.pos)[0]; self.pos+=4; return value

    def i64(self):
        value=struct.unpack_from('<q',self.data,self.pos)[0]; self.pos+=8; return value

    def string(self):
        count=self.i32(); size=abs(count)*(2 if count<0 else 1)
        assert size<100000 and self.pos+size<=len(self.data)
        value=self.data[self.pos:self.pos+size]; self.pos+=size
        return value[:-2 if count<0 else -1].decode('utf-16-le' if count<0 else 'utf8') if count else ''

    def name(self):
        index=self.i32(); number=self.i32(); assert 0<=index<len(self.names)
        return self.names[index]+('_'+str(number-1) if number else '')

    def object(self,index):
        return None if index==0 else self.imports[-index-1] if index<0 else self.exports[index-1]['name']

    def header_tag(self):
        name=self.name()
        if name=='None': return None
        kind=self.name(); size=self.i32(); index=self.i32(); meta=None
        if kind=='StructProperty': meta=self.name(); self.pos+=16
        elif kind=='BoolProperty': meta=bool(self.data[self.pos]); self.pos+=1
        elif kind in {'ByteProperty','EnumProperty','ArrayProperty','SetProperty'}: meta=self.name()
        elif kind=='MapProperty': meta=[self.name(),self.name()]
        guid=self.data[self.pos]; self.pos+=1
        if guid: self.pos+=16
        assert 0<=size and self.pos+size<=len(self.data)
        return dict(name=name,kind=kind,size=size,meta=meta,start=self.pos)

    def properties(self):
        result={}
        tagged_structs={'MaterialParameterInfo','TextureSource','MaterialInstanceBasePropertyOverrides',
                        'ScalarParameterAtlasInstanceData','ScalarParameterValue','TextureParameterValue',
                        'VectorParameterValue','FunctionExpressionInput'}
        for _ in range(10000):
            tag=self.header_tag()
            if tag is None: return result
            name,kind,size,meta,start=(tag[k] for k in ['name','kind','size','meta','start'])
            end=start+size
            if kind=='FloatProperty': value=struct.unpack_from('<f',self.data,start)[0]
            elif kind=='BoolProperty': value=meta
            elif kind=='IntProperty': value=self.i32()
            elif kind=='ObjectProperty': value=self.object(self.i32())
            elif kind in {'NameProperty','ByteProperty','EnumProperty'}: value=self.name()
            elif kind=='StrProperty': value=self.string()
            elif kind=='StructProperty' and meta in tagged_structs: value=self.properties()
            elif kind=='StructProperty' and meta in {'ExpressionInput','ColorMaterialInput','ScalarMaterialInput','VectorMaterialInput'}:
                obj,out=struct.unpack_from('<ii',self.data,start); value=dict(expression=self.object(obj),output=out)
            elif kind=='ArrayProperty':
                count=self.i32(); assert 0<=count<10000
                if meta=='StructProperty':
                    inner=self.header_tag()
                    if inner['meta'] in tagged_structs:
                        value=[self.properties() for _ in range(count)]
                    else: value=dict(count=count,unsupported_element_type=inner['meta'])
                elif meta=='ObjectProperty': value=[self.object(self.i32()) for _ in range(count)]
                else: value=dict(count=count,unsupported_element_type=meta)
            else: value=dict(unsupported_type=meta or kind,bytes=size)
            result[name]=value; self.pos=end
        raise ValueError('Property count limit exceeded')

    def export_properties(self,entry):
        self.pos=entry['offset']; return self.properties()

    def texture_source_png(self):
        entry=next(e for e in self.exports if e['name']==self.path.stem)
        properties=self.export_properties(entry); source=properties['Source']
        assert source['bPNGCompressed'] and source['NumMips']==source['NumSlices']==1
        pos=max(e['offset']+e['size'] for e in self.exports)
        magic,chunk_size,compressed_size,raw_size=struct.unpack_from('<QQQQ',self.data,pos)
        assert magic==0x9e2a83c1 and chunk_size==131072 and raw_size<100*1024*1024
        count=math.ceil(raw_size/chunk_size)
        sizes=[struct.unpack_from('<QQ',self.data,pos+32+i*16) for i in range(count)]
        pos+=32+16*count; output=bytearray()
        for compressed,raw in sizes:
            assert compressed<=chunk_size*2 and raw<=chunk_size
            block=zlib.decompress(self.data[pos:pos+compressed]); assert len(block)==raw
            output+=block; pos+=compressed
        assert len(output)==raw_size and sum(c for c,_ in sizes)==compressed_size
        from PIL import Image
        image=Image.open(io.BytesIO(output)); image.verify()
        image=Image.open(io.BytesIO(output))
        assert image.size==(source['SizeX'],source['SizeY'])
        raw=bytes(output);canonical=canonical_texture_source_png(raw,source['Format'])
        self.raw_texture_source_png=raw
        self.texture_source_provenance=dict(storage_format=source['Format'],
            storage_png_sha256=hashlib.sha256(raw).hexdigest(),
            canonical_png_sha256=hashlib.sha256(canonical).hexdigest(),
            channel_normalization='BGRA storage to canonical RGBA; R/B exchanged exactly once' if source['Format']=='TSF_BGRA8' else 'Canonical source channel order')
        return canonical,list(image.size)


def sha(data): return hashlib.sha256(data).hexdigest()


def audit(source=DEFAULT_SOURCE,extract=False,ground=False):
    materials={}; masters={}
    for path in sorted((source/'Materials').glob('*.uasset')):
        if not path.stem.startswith('MI_'): continue
        package=Package(path); entry=next(e for e in package.exports if e['name']==path.stem)
        props=package.export_properties(entry)
        record=dict(parent=props.get('Parent'),source_file=str(path),source_sha256=sha(package.data),
                    base_property_overrides=props.get('BasePropertyOverrides',{}))
        for key,label in [('ScalarParameterValues','scalars'),('TextureParameterValues','textures'),('VectorParameterValues','vectors')]:
            record[label]={p['ParameterInfo']['Name']:p['ParameterValue'] for p in props.get(key,[])}
        materials[path.stem]=record
    for name in ['MM_Master_2_Blend','MM_Master_3_Blend','MM_POM_Master_Simple','MM_POM_3_Blend_Puddle','MM_Simple']:
        package=Package(source/'Materials'/(name+'.uasset')); nodes=[]
        for entry in package.exports:
            if not entry['name'].startswith('MaterialExpression'): continue
            props=package.export_properties(entry)
            for key in ['Material','MaterialExpressionGuid','MaterialExpressionEditorX','MaterialExpressionEditorY','ExpressionGUID','Outputs','FunctionOutputs']:
                props.pop(key,None)
            nodes.append(dict(name=entry['name'],type=package.object(entry['cls']),properties=props))
        material=package.export_properties(next(e for e in package.exports if e['name']==name))
        outputs={key:material[key] for key in ['BaseColor','Normal','Metallic','Roughness','AmbientOcclusion'] if key in material}
        masters[name]=dict(source_sha256=sha(package.data),material_outputs=outputs,nodes=nodes)
    extracted=[]
    requests=[('T_RGB_Mask','source_textures/T_RGB_Mask.PNG'),
              ('T_Wall_Plaster_01_RGB_Mask','source_textures/T_Tileable_Wall_Plaster_01_RGB_Mask.PNG'),
              ('T_Wall_Plaster_01_01_BC','source_textures/T_Tileable_Wall_Plaster_01_01_BC.PNG')]
    if ground:
        requests.extend((stem+'_'+kind,'source_ground/'+stem+'_'+kind+'.png')
                        for stem in ['T_SoilStoneMix','T_Soil'] for kind in ['BC','N','ARM'])
    for name,output in requests:
        path=source/'Textures'/(name+'.uasset'); package=Package(path)
        data,dimensions=package.texture_source_png()
        texture_props=package.export_properties(next(e for e in package.exports if e['name']==name))
        record=dict(source_file=str(path),source_sha256=sha(package.data),png_sha256=sha(data),
                    dimensions=dimensions,output=output,
                    source_srgb=texture_props.get('SRGB',True),
                    source_srgb_basis='Explicit serialized property' if 'SRGB' in texture_props else 'Texture2D default; no override serialized',
                    extraction='Full TextureSource chunked-zlib PNG; source storage channels canonicalized; not editor thumbnail',
                    **package.texture_source_provenance)
        record['storage_png']='source_storage_png/'+name+'.png'
        if extract:
            destination=DEST/record['output']; destination.parent.mkdir(exist_ok=True); destination.write_bytes(data)
            storage=DEST/record['storage_png'];storage.parent.mkdir(exist_ok=True);storage.write_bytes(package.raw_texture_source_png)
        extracted.append(record)
    report=dict(schema=1,source_project=str(source.parent.parent),source_executed=False,
                source_modified=False,material_instances=materials,master_materials=masters,
                recovered_textures=extracted)
    (DEST/'unreal-material-source-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Audited',len(materials),'material instances and',len(masters),'master graphs; recovered full-resolution textures:',extract,flush=True)
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=DEFAULT_SOURCE)
    parser.add_argument('--extract',action='store_true')
    parser.add_argument('--ground',action='store_true',help='Also recover source soil and stone-mix PBR images')
    args=parser.parse_args(); audit(args.source,args.extract,args.ground)
