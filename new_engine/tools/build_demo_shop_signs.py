#!/usr/bin/env python3
"""Small authored trade signs using the resident purchased material library."""
import json
import math
from pathlib import Path
import numpy as np
import build_castle_tavern_assets as kit
from build_village_keep_hero import GLBWriter, image_policies, triangles
from build_purchased_village import encode_glb

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'projects/valwood_world/models'


def board(kind, lod):
    m = kit.Mesh(lod)
    n = 24 if lod == 'near' else 12
    # Broad sides face along the street; the bracket reaches out from the wall.
    oval = [(0,.40+.34*math.sin(i*math.tau/n),.53+.39*math.cos(i*math.tau/n)) for i in range(n)]
    for side in (-1,1):
        m.face('Wood',[(side*.045,y,z) for x,y,z in oval],[side,0,0])
    for i in range(n):
        j=(i+1)%n
        m.face('DarkWood',[(-.045,*oval[i][1:]),(.045,*oval[i][1:]),
                           (.045,*oval[j][1:]),(-.045,*oval[j][1:])],
               [0,math.sin((i+.5)*math.tau/n),math.cos((i+.5)*math.tau/n)])
    # Physical iron straps/bracket/rings have thickness and cast actual shadows.
    m.box('Metal',-.035,.035,0,.95,-.025,.025)
    m.beam('Metal',(0,.91,0),(0,.91,.81),.025)
    m.beam('Metal',(0,.66,0),(0,.91,.38),.022)
    for z in (.32,.75):
        m.beam('Metal',(0,.91,z),(0,.71,z),.018)
    for side in (-1,1):
        x = side*.061
        if kind == 'baker':
            m.sphere('Plaster',(x,.37,.53),(.018,.13,.25),12 if lod=='near' else 8,6)
            for z in (.40,.52,.64):
                m.beam('DarkWood',(x+side*.021,.41,z-.025),(x+side*.021,.48,z+.025),.012)
        elif kind == 'tavern':
            m.face('Plaster',[(x,.19,.33),(x,.19,.59),(x,.57,.62),(x,.57,.30)],[side,0,0])
            for i in range(12):
                a,b=i*math.tau/12,(i+1)*math.tau/12
                m.beam('Plaster',(x,.39+.105*math.sin(a),.66+.085*math.cos(a)),
                       (x,.39+.105*math.sin(b),.66+.085*math.cos(b)),.024)
            m.beam('DarkWood',(x+side*.006,.25,.32),(x+side*.006,.25,.59),.014)
            m.beam('DarkWood',(x+side*.006,.51,.31),(x+side*.006,.51,.61),.014)
        else:
            m.beam('Plaster',(x,.15,.42),(x,.64,.61),.018)
            for y,z,slope in ((.28,.47,-1),(.40,.52,1),(.53,.57,-1)):
                leaf=[(x,y,z),(x,y+.13,z+slope*.14),(x,y+.19,z+slope*.025)]
                m.face('Plaster',leaf,[side,0,0])
    return m


def main():
    rows=[]
    for kind in ('baker','tavern','herbalist'):
        for lod in ('near','mid'):
            m=board(kind,lod)
            writer=GLBWriter()
            writer.doc['asset']['generator']='Valwood authored physical shop signs 1'
            for name,(source,factor,rough) in kit.MATERIALS.items():
                p,n,uv,ix=m.parts[name]
                if not ix:
                    continue
                writer.material(kit.DONOR,source,name)
                material=writer.doc['materials'][-1]
                factor = [.28,.40,.14,1] if kind=='herbalist' and name=='Plaster' else factor
                material['pbrMetallicRoughness'].update(baseColorFactor=factor,metallicFactor=.65 if name=='Metal' else 0,roughnessFactor=rough)
                if name=='Metal':
                    material.pop('normalTexture',None)
                    material.pop('occlusionTexture',None)
                    material['pbrMetallicRoughness'].pop('baseColorTexture',None)
                    material['pbrMetallicRoughness'].pop('metallicRoughnessTexture',None)
                writer.doc['meshes'][0]['primitives'].append(dict(mode=4,material=len(writer.doc['materials'])-1,
                    attributes=dict(POSITION=writer.accessor(p,3),NORMAL=writer.accessor(n,3),TEXCOORD_0=writer.accessor(uv,2)),
                    indices=writer.accessor(ix,1,5125)))
            writer.doc['buffers'][0]['byteLength']=len(writer.binary)
            path=OUT/f'demo_sign_{kind}_{lod}.glb'
            kit.publish(path,encode_glb(writer.doc,bytes(writer.binary)))
            d,p,t=triangles(path)
            assert len(t)<1800 and np.isfinite(p).all()
            assert image_policies(path)<=image_policies(kit.DONOR)
            rows.append(dict(file=path.name,sha256=kit.sha(path),triangles=len(t),
                minimum=p.min(axis=0).tolist(),maximum=p.max(axis=0).tolist(),new_image_policies=0))
    report=dict(schema=1,assets=rows,source_sha256=kit.sha(kit.DONOR),new_texture_images=0,
                recipe_sha256=kit.sha(Path(__file__)),purpose='Distinct physical bread, tankard and herb signs; reused source wood/plaster maps')
    out=ROOT/'docs/demo-polish/2026-10-29/signs-manifest.json'
    out.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':
    main()
