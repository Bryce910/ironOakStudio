#!/usr/bin/env python3
"""Recover purchased UE4 TextureSource BGRE8 lat-long HDRs without Unreal.

The archive is data only. Original RGBE mantissas/exponents are retained exactly;
there is no exposure, gamma, tone mapping, resampling, or orientation guess.
Runtime scene binding and texture adjustment metadata are reported separately.
"""
from pathlib import Path
import argparse,hashlib,json,math,struct,subprocess,sys,zlib
import numpy as np

sys.path.insert(0,str(Path(__file__).resolve().parent))
from audit_purchased_village_source import Package

ROOT=Path(__file__).resolve().parents[1]
ARCHIVE=Path('/home/computer/Downloads/uploads_files_3007638_Medieval_Village.rar')
MEMBER_ROOT='Medieval_Village/Content/Medieval_Village/HDR/'
NAMES=('HDRI_Sky_Dome','HDRI_Reflection_Cubemap')

def digest(data):return hashlib.sha256(data).hexdigest()

def unpack_source(path):
    p=Package(path);e=next(e for e in p.exports if e['name']==path.stem);properties=p.export_properties(e)
    source=properties['Source'];assert source['Format']=='TSF_BGRE8' and source['NumSlices']==source['NumMips']==1
    assert properties.get('SRGB')is False and properties.get('CompressionSettings')=='TC_HDR'
    assert not source.get('bPNGCompressed',False)
    pos=max(e['offset']+e['size']for e in p.exports)
    magic,chunk_size,compressed_size,raw_size=struct.unpack_from('<QQQQ',p.data,pos)
    assert magic==0x9e2a83c1 and 0<chunk_size<=1024*1024 and raw_size==source['SizeX']*source['SizeY']*4
    count=math.ceil(raw_size/chunk_size);sizes=[struct.unpack_from('<QQ',p.data,pos+32+i*16)for i in range(count)]
    pos+=32+16*count;parts=[]
    for comp,raw in sizes:
        block=zlib.decompress(p.data[pos:pos+comp]);assert len(block)==raw;parts.append(block);pos+=comp
    raw=b''.join(parts);assert len(raw)==raw_size
    bg=np.frombuffer(raw,np.uint8).reshape(source['SizeY'],source['SizeX'],4)
    rgbe=bg[:,:,[2,1,0,3]].copy()
    return rgbe,properties,raw

def radiance(rgbe):
    h,w=rgbe.shape[:2];assert 8<=w<32768
    data=bytearray(f'#?RADIANCE\n# Exact UE4 TSF_BGRE8 source; sRGB=false; source row order retained.\nFORMAT=32-bit_rle_rgbe\n\n-Y {h} +X {w}\n'.encode())
    for row in rgbe:
        data.extend(bytes((2,2,w>>8,w&255)))
        for k in range(4):
            for x in range(0,w,128):
                values=row[x:x+128,k];data.append(len(values));data.extend(values.tobytes())
    return bytes(data)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--archive',type=Path,default=ARCHIVE)
    ap.add_argument('--source-project',type=Path,default=ROOT.parent/'.local-tools/village-source/source-project/Medieval_Village')
    ap.add_argument('--output',type=Path,default=ROOT/'assets/purchased/medieval_village/source-hdri');args=ap.parse_args()
    source=args.source_project/'Content/Medieval_Village/HDR';source.mkdir(parents=True,exist_ok=True);args.output.mkdir(parents=True,exist_ok=True)
    rows=[]
    for name in NAMES:
        member=MEMBER_ROOT+name+'.uasset';raw=subprocess.run(['unrar','p','-inul',str(args.archive),member],check=True,capture_output=True).stdout
        target=source/(name+'.uasset')
        if target.exists():assert target.read_bytes()==raw,'Existing private source differs'
        else:target.write_bytes(raw)
        rgbe,properties,bgre=unpack_source(target);encoded=radiance(rgbe);out=args.output/(name+'.hdr');out.write_bytes(encoded)
        # Bounded rows avoid allocating a full float64 6K HDR image.
        total=np.zeros(3,np.float64);maximum=0.;minimum=float('inf')
        for row in rgbe:
            expo=row[:,3].astype(np.int32);scale=np.exp2(expo.astype(float)-136);rgb=row[:,:3]*scale[:,None];rgb[expo==0]=0
            assert np.isfinite(rgb).all() and np.min(rgb)>=0
            total+=rgb.sum(0);minimum=min(minimum,float(rgb.min()));maximum=max(maximum,float(rgb.max()))
        rows.append(dict(name=name,source_archive_member=member,source_uasset=str(target.resolve()),source_uasset_sha256=digest(raw),source_BGRe_payload_sha256=digest(bgre),
            output=str(out.resolve()),output_sha256=digest(encoded),size=[rgbe.shape[1],rgbe.shape[0]],source_format='TSF_BGRE8',source_sRGB=False,
            encoding='Radiance RGBE; original mantissas and exponents unchanged; B/R storage converted once',linear_range=[minimum,maximum],linear_mean_rgb=(total/(rgbe.shape[0]*rgbe.shape[1])).tolist(),
            source_texture_adjustments={k:v for k,v in properties.items()if k.startswith('Adjust')},texture_adjustments_applied=False,
            orientation='Original TextureSource row order unchanged; Radiance -Y height +X width; no U/V mirror or rotation applied',source_properties=properties,
            scene_binding='Not inferred from filename; use exact .umap SkyLight/sky material bindings; source SkyLight currently identified as HDRI_Epic_Courtyard_Daylight, a separate asset'))
        print('RECOVERED',name,rgbe.shape[1],rgbe.shape[0],f'linear max={maximum:.3f}',flush=True)
    report=dict(schema=1,archive=str(args.archive),archive_sha256=hashlib.file_digest(args.archive.open('rb'),'sha256').hexdigest(),purchased_original_archive_modified=False,
        source_project_executed=False,production_unchanged=True,gamma_or_exposure_changes=False,files=rows)
    (args.output/'audit.json').write_text(json.dumps(report,indent=2)+'\n')

if __name__=='__main__':main()
