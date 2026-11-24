#!/usr/bin/env python3
"""Bounded source-material cook for the purchased BEP countryside selection.

Only explicitly selected existing source payloads are read. Source archives,
city projects and original BEP imports are never modified. Ground rows follow
the included Built-in TerrainLayer albedo/normal/scalar material bindings;
auto-upgrade displacement references are not mislabelled as ARM maps.
"""
from pathlib import Path
import argparse,hashlib,json
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT.parent/'.local-tools/bep-source'
OUT=ROOT/'assets/purchased/bep_reforged/countryside-materials'
GROUND={
    'grassground':'44496a3f568b4f44f885bb81d9e17258',
    'earthpath':'47e4b4b2fa41b4943aaf4f663f9ea276',
    'rocky':'83e1f076682f6844e88f728ec10f0ce3',
}

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def linear(v):return np.where(v<=.04045,v/12.92,((v+.055)/1.055)**2.4)
def srgb(v):return np.where(v<=.0031308,v*12.92,1.055*np.maximum(v,0)**(1/2.4)-.055)
def resize(a,size):return np.stack([np.asarray(Image.fromarray(a[:,:,c].astype(np.float32),'F').resize(size,Image.Resampling.LANCZOS))for c in range(a.shape[2])],axis=2)
def image_bytes(array,path):
    Image.fromarray(np.uint8(np.clip(np.rint(array*255),0,255))).save(path,compress_level=3)
    return str(path.resolve())

def ground(output=OUT,size=1024):
    output=Path(output);(output/'textures').mkdir(parents=True,exist_ok=True)
    layers={r['guid']:r for r in json.loads((SOURCE/'terrain-layers.json').read_text())}
    inventory=json.loads((SOURCE/'BEP_Built-in_02/inventory.json').read_text());registry={r['guid']:r for r in inventory['assets']}
    rows={};audit=[];white=output/'textures/unoccluded-unit-ARM.png'
    image_bytes(np.ones((4,4,3),np.float32),white)
    for name,guid in GROUND.items():
        record=layers[guid];source=record['data'];paths={};files=[]
        for kind,key in [('base','m_DiffuseTexture'),('normal','m_NormalMapTexture')]:
            tex=source[key];path=SOURCE/'BEP_Built-in_02/guid'/tex['guid']/'asset'
            if not path.is_file():raise FileNotFoundError('Recover selected GUID '+tex['guid']+' before cooking')
            im=Image.open(path);a=np.asarray(im.convert('RGB'),np.float32)/255.;original=list(im.size)
            target=(min(size,im.width),min(size,im.height))
            if kind=='base':
                # Opaque terrain uses the source composite RGB; the included
                # scalar smoothness is zero, so diffuse alpha contributes no
                # smoothness. Filter color in linear light.
                a=srgb(resize(linear(a),target))
            else:
                # Unity's desktop unpack scales XY then reconstructs +Z.
                # Bake that authored layer strength once, before native mips.
                xy=a[:,:,:2]*2-1;xy*=source['m_NormalScale']
                n=np.dstack((xy,np.sqrt(np.maximum(1-np.sum(xy*xy,axis=2),0))))
                n=resize(n,target);n/=np.maximum(np.linalg.norm(n,axis=2,keepdims=True),1e-8)
                n[:,:,1]*=-1 # Native glTF V differs from original Unity V.
                a=n*.5+.5
            dest=output/'textures'/(name+'_'+kind+'.png');paths[kind]=image_bytes(a,dest)
            files.append(dict(role=kind,source_guid=tex['guid'],source_path=registry[tex['guid']]['path'],source_sha256=sha(path),
                source_size=original,prepared_size=list(target),prepared_sha256=sha(dest),
                linear_color_filter=kind=='base',source_normal_scale_baked=source['m_NormalScale']if kind=='normal'else None,
                normal_green_inverted_once=kind=='normal'))
        assert source['m_Smoothness']==0.,'Nonzero terrain smoothness requires explicit diffuse-alpha sampling'
        rows[name]=dict(name='BEP_Reforged_'+name,source_master='Built-in Terrain Standard',base=paths['base'],normal=paths['normal'],arm=str(white.resolve()),
            emissive=None,emissive_factor=[0,0,0],factor=[1,1,1,1],roughness=1.,metallic=source['m_Metallic'],alphaMode='OPAQUE',alphaCutoff=.5,doubleSided=False,
            uv_scale=[1,1],facade_mode=0,facade_version=1,aux=[None,None,None],facade_scale=[1,1,1],source_paint=False,
            source_layer_guid=guid,source_tile_metres=[source['m_TileSize'][k]for k in ['x','y']],normal_scale_baked=source['m_NormalScale'],
            limitations=['Original displacement reference retained as provenance, not treated as metallic/AO/roughness','No parallax displacement or terrain layer mixing is baked into this single material'])
        audit.append(dict(name=name,source_layer=record,prepared=files))
    (output/'ground-materials.json').write_text(json.dumps(rows,indent=2)+'\n')
    report=dict(source_package=inventory['archive'],selective_source_bytes=sum((SOURCE/'BEP_Built-in_02/guid'/f['source_guid']/'asset').stat().st_size for r in audit for f in r['prepared']),
        source_layers=str((SOURCE/'terrain-layers.json').resolve()),ground=audit,
        texture_GPU_RGBA8_mips_MiB=6*size*size*4*4/3/1024**2+4*4*4*4/3/1024**2,
        geometry_UV='For native ground +U=+X,+V=+Z use TANGENT.w=-1 and u=x/tileX,v=z/tileY; prepared normals include original Unity-to-native V green correction',
        native_projects_or_runtime_modified=False,source_archives_modified=False)
    (output/'ground-audit.json').write_text(json.dumps(report,indent=2)+'\n');print('GROUND READY',output/'ground-materials.json',report['texture_GPU_RGBA8_mips_MiB'],'MiB',flush=True)
    return rows

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output',type=Path,default=OUT);ap.add_argument('--size',type=int,choices=[512,1024],default=1024);args=ap.parse_args();ground(args.output,args.size)
if __name__=='__main__':main()
