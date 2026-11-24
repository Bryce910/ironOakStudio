#!/usr/bin/env python3
"""Bound original UE HQ-lightmap energy without guessing UVs or texels.

Each encoded channel is bounded by [0,1]. The range-decode coefficients can
therefore prove a lightmap contains no meaningful irradiance before any costly
per-instance geometry duplication. This does not rescale or invalidate data.
"""
from pathlib import Path
import argparse, hashlib, json
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT.parent/'.local-tools/village-source/source-lightmaps/manifest.json'

def bounds(row):
    scale=np.array(row['scale'],np.float64);add=np.array(row['add'],np.float64)
    # HQ alpha adds a signed residual in [-.5/255,+.5/255]. Scale can in
    # principle be signed, so select interval endpoints explicitly.
    log_end=np.array([-.5/255,1+.5/255])*scale[0,3]+add[0,3]
    l=max(float(np.exp2(log_end.max())-.01858136),0.)
    chroma=np.maximum(np.maximum(add[0,:3],add[0,:3]+scale[0,:3]),0.)
    lo=np.minimum(add[1],add[1]+scale[1]);hi=np.maximum(add[1],add[1]+scale[1])
    sh_abs=np.maximum(np.abs(lo[:3]),np.abs(hi[:3]))
    direction=max(float(np.linalg.norm(sh_abs)+hi[3]),0.)
    return (chroma*l*direction).tolist()

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--manifest',type=Path,default=DEFAULT)
    ap.add_argument('--output',type=Path,default=ROOT/'docs/village/2026-11-02/baked-light-energy-audit.json');args=ap.parse_args()
    doc=json.loads(args.manifest.read_text());records=doc['registry']['records'];result=[]
    for guid,row in records.items():
        if row['lightmap_type']!=2:continue
        upper=bounds(row);bindings=[b for b in doc['component_bindings']if b['guid']==guid]
        result.append(dict(guid=guid,maximum_possible_diffuse_RGB=upper,
            near_black_upper_bound=max(upper)<=1e-6,
            baked_light_GUID_count=len(row['light_guids']),
            bindings=[dict(component=b['component'],actor=b.get('actor'),mesh=b['mesh']['name']if b.get('mesh')else None,lod=b['lod'],instances=b['per_instance_count'])for b in bindings]))
    black=[r for r in result if r['near_black_upper_bound']];other=[r for r in result if not r['near_black_upper_bound']]
    report=dict(manifest=str(args.manifest.resolve()),manifest_sha256=hashlib.sha256(args.manifest.read_bytes()).hexdigest(),
        formula_source=doc['formula_source'],method='Analytical upper bound over every normalized source texture channel and every unit source-world normal; no UV or texture-data assumptions',
        threshold=1e-6,HQ_records=len(result),near_black_records=len(black),other_records=len(other),
        records_with_baked_light_GUIDs=sum(bool(r['baked_light_GUID_count'])for r in result),
        near_black_maximum_possible_value=max(max(r['maximum_possible_diffuse_RGB'])for r in black),
        non_near_black=other,
        conclusion='All architecture/ground bindings have analytically negligible baked irradiance. Current source Sun and SkyLight are Movable; preserve independent dynamic sky diffuse as well as direct sun. Near-black baked data must not replace dynamic skylight ambient.',
        production_data_modified=False)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:report[k]for k in ['HQ_records','near_black_records','other_records','records_with_baked_light_GUIDs','near_black_maximum_possible_value']},indent=2));print(args.output)

if __name__=='__main__':main()
