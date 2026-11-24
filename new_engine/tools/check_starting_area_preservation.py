#!/usr/bin/env python3
"""Read-only audit against the retained pre-starting-area references."""
import argparse,datetime,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'docs/performance/starting-area'
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=OUT/'preservation.json',metavar='REPORT_FILE',help='JSON report file; retained starting-area baselines are always read in place (default: %(default)s).')
    args=parser.parse_args();output=args.output.expanduser()
    prior=json.loads((OUT/'before/preservation.json').read_text());rows=[]
    for old in prior['seldonne_root_files']:
        p=ROOT/old['path'];current=digest(p) if p.is_file() else None
        rows.append({'path':old['path'],'category':old['category'],'baseline_sha256':old['baseline_sha256'],'current_sha256':current,'unchanged':current==old['baseline_sha256']})
    authored=[r for r in rows if r['category']=='authored_world_or_asset'];changed=[r['path'] for r in authored if not r['unchanged']]
    baseline=json.loads((OUT/'before-manifest.json').read_text())['sha256']
    original='projects/valwood_world/scenery/authored_instances.txt'
    map_source=Path('/home/computer/Desktop/Valwood-Map.jpg');map_retained=ROOT/'projects/valwood_world/assets/map.jpg'
    report={'checked_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Read-only Seldonne authored assets, original283 scenery placements, and reference-map audit.','authored_files_checked':len(authored),'authored_files_changed':changed,'runtime_or_layout_changes':[r['path'] for r in rows if not r['unchanged'] and r not in authored], 'source_scenery':{'path':original,'baseline_sha256':baseline[original],'current_sha256':digest(ROOT/original),'unchanged':digest(ROOT/original)==baseline[original]},'reference_map':{'source_sha256':digest(map_source),'retained_sha256':digest(map_retained),'unchanged':digest(map_source)==digest(map_retained)},'files':rows}
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,indent=2)+'\n')
    assert not changed,changed
    assert report['source_scenery']['unchanged'] and report['reference_map']['unchanged']
    print(f'PASS preservation: {len(authored)} authored Seldonne files, original283 source placements and supplied reference map unchanged.')
if __name__=='__main__':main()
