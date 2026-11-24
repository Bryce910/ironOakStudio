#!/usr/bin/env python3
"""Fingerprint rendered source/assets and verify they stayed frozen for profiling."""
import argparse,datetime,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'docs/performance/starting-area/build-manifest.json'
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inputs():
    paths=[ROOT/'rpg',ROOT/'Makefile',ROOT.parent/'Launch_Valwood_World.sh',ROOT.parent/'Launch_Valwood_Starting_Area.sh',ROOT.parent/'Launch_Valwood_Rainforest.sh',ROOT.parent/'Launch_Valwood_Rainforest_Showcase.sh',ROOT.parent/'Launch_Valwood_Castle_Village.sh']
    for folder,patterns in [('engine',['*.c','*.cpp','*.h','*.inc']),('shaders',['world*glsl']),('tests',['world*test*','project_runtime_test.c']),('projects/valwood_world/models',['*.glb','*manifest*.json']),('projects/valwood_world/ground',['*.png']),('projects/valwood_world/scenery',['*.txt','*.json']),('tools',['*starting*.py','*rainforest*.py','*village*.py','benchmark_cinematic_scenery.py','benchmark_valwood_world.sh'])]:
        for pattern in patterns:paths.extend((ROOT/folder).glob(pattern))
    paths += [ROOT/'projects/valwood_world/project.valwood',ROOT/'projects/valwood_world/assets/map.jpg']
    return {str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p):digest(p) for p in sorted(set(paths)) if p.is_file()}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--verify',action='store_true')
    p.add_argument('--output',type=Path,default=OUT,metavar='MANIFEST_FILE',help='JSON manifest file to write or verify (default: %(default)s).')
    a=p.parse_args();output=a.output.expanduser();current=inputs()
    if a.verify:
        old=json.loads(output.read_text())['sha256'];different=[key for key in old.keys()|current.keys() if old.get(key)!=current.get(key)]
        assert not different,'Profile inputs changed: '+str(different)
        print(f'PASS: all {len(current)} source, binary and asset fingerprints still match the profiled build.')
    else:
        output.parent.mkdir(parents=True,exist_ok=True)
        output.write_text(json.dumps({'recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Executable, engine/shader sources, focused tests, runtime scene/model/ground assets and starting-area tools; captured before final native profiles.','sha256':current},indent=2)+'\n');print(f'Recorded {len(current)} rendered-build inputs.')
if __name__=='__main__':main()
