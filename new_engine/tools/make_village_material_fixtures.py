#!/usr/bin/env python3
"""Create tiny CPU/native-certification fixtures from frozen city materials."""
from pathlib import Path
import copy,hashlib,json,sys
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent))
from restore_village_source_windows import append,image
from build_purchased_village import ROOT,encode_glb

def main():
    root=ROOT/'assets/purchased/medieval_village/city-materials';rows=json.loads((root/'materials-frozen.json').read_text());out=root/'fixtures';out.mkdir(exist_ok=True);report=[]
    for name in ['MI_Wood_Trims_01','MI_WallPlaster','MI_POM_Ground','MI_Tent_1','MI_GlassWindow']:
        row=rows[name];doc={'asset':{'version':'2.0'},'buffers':[{'byteLength':0}],'bufferViews':[],'accessors':[],'images':[],'textures':[],'materials':[],'meshes':[],'nodes':[{'mesh':0}],'scenes':[{'nodes':[0]}],'scene':0};raw=bytearray();cache={}
        def tex(path):
            if path not in cache:cache[path]=image(doc,raw,Path(path))['index']
            return dict(index=cache[path])
        pbr=dict(baseColorFactor=row['factor'],roughnessFactor=row['roughness'],metallicFactor=row['metallic'])
        if row.get('base'):pbr['baseColorTexture']=tex(row['base'])
        if row.get('arm'):pbr['metallicRoughnessTexture']=tex(row['arm'])
        mat=dict(name=name,pbrMetallicRoughness=pbr,alphaMode=row['alphaMode'],alphaCutoff=row['alphaCutoff'],doubleSided=row['doubleSided'])
        if row.get('normal'):mat['normalTexture']=tex(row['normal'])
        if row.get('arm'):mat['occlusionTexture']=tex(row['arm'])
        if row.get('emissive'):mat['emissiveTexture']=tex(row['emissive']);mat['emissiveFactor']=row['emissive_factor']
        if row['facade_mode']:
            aux=[tex(p)['index']if p else-1 for p in row['aux']];mat['extensions']={'VALWOOD_materials_facade':{'parameters':[row['facade_version'],row['facade_mode'],*aux,*row['facade_scale']]}};doc['extensionsUsed']=['VALWOOD_materials_facade']
        doc['materials']=[mat];attrs={}
        for key,values,kind in [('POSITION',[[-1,0,-1],[1,0,-1],[1,0,1],[-1,0,1]],'VEC3'),('NORMAL',[[0,1,0]]*4,'VEC3'),('TANGENT',[[1,0,0,1]]*4,'VEC4'),('TEXCOORD_0',[[0,1],[1,1],[1,0],[0,0]],'VEC2')]:attrs[key]=append(doc,raw,np.array(values,'<f4'),kind)
        if row.get('source_paint'):attrs['_VALWOOD_SOURCE_PAINT']=append(doc,raw,np.array([[0,0,0],[1,1,1],[128/255,128/255,179/255],[0,77/255,0]],'<f4'),'VEC3')
        ix=append(doc,raw,np.array([0,2,1,0,3,2],'<u4'),'SCALAR');doc['meshes']=[{'primitives':[dict(attributes=attrs,indices=ix,material=0,mode=4)]}];doc['buffers'][0]['byteLength']=len(raw);path=out/(name+'.glb');path.write_bytes(encode_glb(doc,raw));report.append(dict(name=name,path=str(path.resolve()),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),bytes=path.stat().st_size,source_material_mode=row['facade_mode'],source_schema=row.get('facade_version',1)))
    (out/'fixtures.json').write_text(json.dumps(report,indent=2)+'\n');print('READY',len(report),'small source-material fixtures',flush=True)

if __name__=='__main__':main()
