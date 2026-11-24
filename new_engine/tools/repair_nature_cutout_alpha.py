#!/usr/bin/env python3
"""Add source shape-atlas cutout alpha to existing isolated material bakes.

NM_Lit_Top_Cover_Shape.shader:646-647 clips ShapeCurvAOHLeaksMask.a-Cutoff.
Its alpha does not come from the base grain or shape B (height). This helper
does not rerun geometry/material baking and does not modify purchased inputs.
"""
from pathlib import Path
import argparse,json
import numpy as np
from PIL import Image
from cook_nature_citadel import ROOT,DATA,yaml_material


def repair_material_alpha(path,result):
    saved=yaml_material(DATA/'source'/path)
    f={k:v for item in saved['m_Floats']for k,v in item.items()}
    if f.get('_Cutoff',0)<=0 or not(f.get('_AlphaClip',0)or f.get('_AlphaCutoffEnable',0)):return result
    tex={k:v for item in saved['m_TexEnvs']for k,v in item.items()}
    c={k:v for item in saved['m_Colors']for k,v in item.items()}
    if f.get('_Shape_UV0_UV2',1)<.5:raise ValueError('UV3 cutout requires the source UV raster: '+path)
    guid=tex.get('_ShapeCurvAOHLeaksMask',{}).get('m_Texture',{}).get('guid')
    paths={e['guid']:e['path']for e in json.loads((DATA/'INVENTORY.json').read_text())['entries']}
    if guid not in paths:raise ValueError('Missing cutout shape source: '+path)
    source=DATA/'source'/paths[guid];base_path=Path(result['paths']['base'])
    with Image.open(base_path)as im:base=np.asarray(im.convert('RGB')).copy()
    with Image.open(source)as im:shape=np.asarray(im.convert('RGBA')).copy()
    h,w=base.shape[:2];sh,sw=shape.shape[:2]
    tr=c.get('_ShapeTilingOffset',{'r':1,'g':1,'b':0,'a':0})
    yy,xx=np.mgrid[:h,:w];u=((xx+.5)/w*tr['r']+tr['b'])%1;v=((1-(yy+.5)/h)*tr['g']+tr['a'])%1
    x=(u*sw-.5)%sw;y=((1-v)*sh-.5)%sh;xf=np.floor(x);yf=np.floor(y)
    x0=xf.astype(int)%sw;y0=yf.astype(int)%sh;fx=x-xf;fy=y-yf
    alpha=(shape[y0,x0,3]*(1-fx)+shape[y0,(x0+1)%sw,3]*fx)*(1-fy)+(shape[(y0+1)%sh,x0,3]*(1-fx)+shape[(y0+1)%sh,(x0+1)%sw,3]*fx)*fy
    rgba=np.dstack((base,np.uint8(np.clip(alpha,0,255)+.5)))
    target=base_path.with_name('base_cutout.png');Image.fromarray(rgba,'RGBA').save(target)
    result=dict(result);result['paths']=dict(result['paths']);result['paths']['base']=str(target)
    result.update(alphaMode='MASK',alphaCutoff=float(f['_Cutoff']))
    result['alpha_source']={'path':str(source.relative_to(ROOT)),'channel':'A','shader_expression':'clip(ShapeCurvAOHLeaksMask.a - _Cutoff)','coverage_fraction':float((alpha/255>=f['_Cutoff']).mean())}
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--report',type=Path,required=True);p.add_argument('--output-report',type=Path,required=True)
    args=p.parse_args();doc=json.loads(args.report.read_text());out={}
    for path,result in doc.items():
        out[path]=repair_material_alpha(path,result)
        if out[path].get('alphaMode'):print(Path(path).stem,'MASK',out[path]['alphaCutoff'],flush=True)
    args.output_report.write_text(json.dumps(out,indent=2)+'\n')


if __name__=='__main__':main()
