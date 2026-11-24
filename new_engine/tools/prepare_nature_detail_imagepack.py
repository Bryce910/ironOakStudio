#!/usr/bin/env python3
"""Prepare bounded opaque 2K albedo encoding; keep all PBR data lossless."""
from pathlib import Path
import argparse,copy,hashlib,json
import numpy as np
from PIL import Image
from bake_nature_material_fidelity import ROOT

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--materials',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-closeup-materials/materials.json')
    p.add_argument('--output',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-closeup-imagepack')
    p.add_argument('--quality',type=int,default=96)
    args=p.parse_args()
    if not 95<=args.quality<=100:raise ValueError('Only high-quality opaque albedo encoding is allowed')
    args.output.mkdir(parents=True,exist_ok=True);materials=copy.deepcopy(json.loads(args.materials.read_text()));report=[]
    for path,info in materials.items():
        source=Path(info['paths']['base']);rgba=Image.open(source).convert('RGBA');a=np.asarray(rgba)
        if np.any(a[:,:,3]!=255)or info.get('alphaMode'):raise ValueError('JPEG cannot retain source alpha: '+path)
        output=args.output/(info['name']+'-base.jpg');rgba.convert('RGB').save(output,quality=args.quality,subsampling=0,optimize=True)
        decoded=np.asarray(Image.open(output).convert('RGB'));error=np.abs(a[:,:,:3].astype(float)-decoded);rmse=float(np.sqrt(np.mean(error**2)))
        if error.mean()>1.5 or np.percentile(error,99)>8:raise ValueError('Opaque encoding exceeds color quality budget: '+path)
        info['paths']['base']=str(output.resolve());info['opaque_base_encoding']={'format':'JPEG','quality':args.quality,'chroma_subsampling':0,'original_png':str(source),'mean_absolute_error_u8':float(error.mean()),'p99_absolute_error_u8':float(np.percentile(error,99)),'psnr_db':float(20*np.log10(255/max(rmse,1e-20)))}
        info['fingerprint']=hashlib.sha256((info['fingerprint']+hashlib.sha256(output.read_bytes()).hexdigest()).encode()).hexdigest()
        report.append({'name':info['name'],'source_bytes':source.stat().st_size,'encoded_bytes':output.stat().st_size,**info['opaque_base_encoding']})
    (args.output/'materials.json').write_text(json.dumps(materials,indent=2)+'\n');(args.output/'encoding-quality.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2),flush=True)

if __name__=='__main__':main()
