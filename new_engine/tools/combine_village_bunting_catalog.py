#!/usr/bin/env python3
"""Extend a frozen publisher catalog with explicitly new rope-bound pennants."""
from pathlib import Path
import argparse,copy,hashlib,json
ROOT=Path(__file__).resolve().parents[1]
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--base',type=Path,default=ROOT/'assets/purchased/medieval_village/publisher-source-datum/catalog.json');ap.add_argument('--bunting',type=Path,default=ROOT/'assets/purchased/medieval_village/publisher-bunting/catalog.json');ap.add_argument('--output',type=Path,default=ROOT/'assets/purchased/medieval_village/publisher-bunting/combined-catalog.json');a=ap.parse_args()
    base=json.loads(a.base.read_text());extra=json.loads(a.bunting.read_text());r=copy.deepcopy(base);oldnames={x['name']for x in r['assets']}
    if any(x['name']in oldnames for x in extra['assets']):raise ValueError('Bunting alias collision')
    r['assets'].extend(extra['assets']);r['placements'].extend(extra['placements'])
    r['new_dressing']=dict(type='Rope-anchored source modular pennants',recovered_original_map_placement=False,assets=[x['name']for x in extra['assets']],placements=len(extra['placements']),
        source_rope_curves=extra['source_rope_curves'],source_catalog=str(a.bunting.resolve()),source_catalog_sha256=hashlib.sha256(a.bunting.read_bytes()).hexdigest(),limitations=extra['limitations'])
    r['previous_catalog']=str(a.base.resolve());r['previous_catalog_sha256']=hashlib.sha256(a.base.read_bytes()).hexdigest()
    if r['assets'][:len(base['assets'])]!=base['assets']or r['placements'][:len(base['placements'])]!=base['placements']:raise ValueError('Frozen source catalog records changed')
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(r,indent=2,allow_nan=False)+'\n')
    print('READY',a.output,len(r['assets']),'assets',len(r['placements']),'placements; all baseline records identical',flush=True)
if __name__=='__main__':main()
