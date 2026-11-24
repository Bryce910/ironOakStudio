#!/usr/bin/env python3
"""Read the owned UE4.26 material graphs and exact StaticMaterials slot tables."""
from pathlib import Path
import argparse,hashlib,json,struct,sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from audit_purchased_village_source import Package

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT.parent/'.local-tools/village-source/source-project/Medieval_Village/Content/Medieval_Village'

class SourcePackage(Package):
    def properties(self):
        result={};tagged={'MaterialParameterInfo','TextureSource','MaterialInstanceBasePropertyOverrides','ScalarParameterAtlasInstanceData','ScalarParameterValue','TextureParameterValue','VectorParameterValue','FunctionExpressionInput','StaticMaterial','MeshUVChannelInfo'}
        for _ in range(10000):
            tag=self.header_tag()
            if tag is None:return result
            name,kind,size,meta,start=(tag[k]for k in('name','kind','size','meta','start'));end=start+size
            if kind=='FloatProperty':value=struct.unpack_from('<f',self.data,start)[0]
            elif kind=='BoolProperty':value=meta
            elif kind=='IntProperty':value=self.i32()
            elif kind=='ObjectProperty':
                index=self.i32();value=self.object(index)
            elif kind in{'NameProperty','ByteProperty','EnumProperty'}:value=self.name()
            elif kind=='StrProperty':value=self.string()
            elif kind=='StructProperty'and meta in tagged:value=self.properties()
            elif kind=='StructProperty'and meta=='LinearColor':value=dict(zip('rgba',struct.unpack_from('<4f',self.data,start)))
            elif kind=='StructProperty'and meta in{'ExpressionInput','ColorMaterialInput','ScalarMaterialInput','VectorMaterialInput'}:
                obj,out=struct.unpack_from('<ii',self.data,start);value=dict(expression=self.object(obj),output=out)
            elif kind=='ArrayProperty':
                count=self.i32();assert 0<=count<10000
                if meta=='StructProperty':
                    inner=self.header_tag()
                    if inner['meta']in tagged:value=[self.properties()for _ in range(count)]
                    else:value=dict(count=count,unsupported_element_type=inner['meta'])
                elif meta=='ObjectProperty':value=[self.object(self.i32())for _ in range(count)]
                elif meta=='FloatProperty':value=list(struct.unpack_from('<'+str(count)+'f',self.data,self.pos))
                else:value=dict(count=count,unsupported_element_type=meta)
            else:value=dict(unsupported_type=meta or kind,bytes=size)
            result[name]=value;self.pos=end
        raise ValueError('Source property count exceeded')

def registry(source=SOURCE):
    meshes={};materials={};masters={}
    for path in sorted((source/'Meshes').glob('*.uasset')):
        p=SourcePackage(path)
        e=next((e for e in p.exports if e['name']==path.stem),None)
        if not e:continue
        props=p.export_properties(e);slots=props.get('StaticMaterials')
        if slots is not None:
            assert isinstance(slots,list),path
            meshes[path.stem]=dict(source=str(path.resolve()),sha256=hashlib.sha256(p.data).hexdigest(),slots=slots,
                lightmap_coordinate_index=props.get('LightMapCoordinateIndex'),lightmap_resolution=props.get('LightMapResolution'))
    for path in sorted((source/'Materials').glob('*.uasset')):
        if not path.stem.startswith(('MI_','MM_')):continue
        p=SourcePackage(path);e=next(e for e in p.exports if e['name']==path.stem);props=p.export_properties(e)
        if path.stem.startswith('MI_'):
            row=dict(parent=props.get('Parent'),source=str(path.resolve()),sha256=hashlib.sha256(p.data).hexdigest(),base_property_overrides=props.get('BasePropertyOverrides',{}))
            for key,label in [('ScalarParameterValues','scalars'),('TextureParameterValues','textures'),('VectorParameterValues','vectors')]:row[label]={v['ParameterInfo']['Name']:v['ParameterValue']for v in props.get(key,[])}
            materials[path.stem]=row
        else:
            nodes=[]
            for e in p.exports:
                if e['name'].startswith('MaterialExpression'):
                    v=p.export_properties(e)
                    for key in ['Material','MaterialExpressionGuid','MaterialExpressionEditorX','MaterialExpressionEditorY','ExpressionGUID','Outputs','FunctionOutputs']:v.pop(key,None)
                    nodes.append(dict(name=e['name'],type=p.object(e['cls']),properties=v))
            masters[path.stem]=dict(source=str(path.resolve()),sha256=hashlib.sha256(p.data).hexdigest(),material_outputs={k:props[k]for k in('BaseColor','Normal','Metallic','Roughness','AmbientOcclusion','EmissiveColor','Opacity','OpacityMask','SubsurfaceColor','WorldPositionOffset')if k in props},properties={k:props[k]for k in('BlendMode','ShadingModel','TwoSided')if k in props},nodes=nodes)
    return dict(schema=1,source_executed=False,source_project=str(source.parent.parent),meshes=meshes,material_instances=materials,master_materials=masters)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--source',type=Path,default=SOURCE);ap.add_argument('--output',type=Path,default=ROOT.parent/'.local-tools/village-source/material-registry.json');args=ap.parse_args()
    r=registry(args.source);args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(r,indent=2)+'\n')
    print('REGISTRY',len(r['meshes']),'meshes',len(r['material_instances']),'instances',len(r['master_materials']),'master graphs',flush=True)

if __name__=='__main__':main()
