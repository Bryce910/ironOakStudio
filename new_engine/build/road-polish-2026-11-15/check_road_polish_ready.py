#!/usr/bin/env python3
"""Publish only the reviewed Linux road composition and roadside detail pass."""
from pathlib import Path
import argparse, contextlib, hashlib, importlib.util, io, json, sys

N=Path(__file__).resolve().parent
OLD=N.parent/'road-life-2026-11-15'
ROOT=N.parents[2]
READY=N/'ROAD_POLISH_READY.json'
OLD_READY_SHA='1129a1666f7844522d3493b249fad097d56244b324ec5ccfd6d9620ab1d5c610'
OLD_CHECK_SHA='e9643e203bbfc8c2537d762771f5be465fc78763bf336c20979f52a7dac96b1f'
REVIEWS=[
    'review/traffic/ROAD_POLISH_TRAFFIC_REVIEW.json',
    'review/scenery/ROADSIDE_SCENERY_REVIEW.json',
    'review/minimap/ROAD_MINIMAP_REVIEW.json',
    'review/routes/ROAD_TRAFFIC_ROUTE_REVIEW.json',
    'review/native-road/ROAD_NATIVE_REVIEW.json',
    'review/ROAD_POLISH_VISUAL_REVIEW.json',
]

def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def load(p):return json.loads(Path(p).read_text())
def require(v,why):
    if not v:raise RuntimeError(why)

def baseline():
    require(sha(OLD/'ROAD_LIFE_READY.json')==OLD_READY_SHA,'Accepted road baseline changed')
    require(sha(OLD/'check_road_life_ready.py')==OLD_CHECK_SHA,'Accepted road verifier changed')
    spec=importlib.util.spec_from_file_location('road_accepted',OLD/'check_road_life_ready.py')
    mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
    with contextlib.redirect_stdout(io.StringIO()):mod.check()
    return mod

def private_fixture(p):
    return p.is_relative_to(N/'review/native-road/private-project')

def publish():
    require(not READY.exists(),'Published readiness is immutable')
    old=baseline();b=old.B;accepted=load(OLD/'ROAD_LIFE_READY.json')
    prepared={str(Path(p).resolve(strict=True)):(h,accepted['frozen_stats'][p]) for p,h in accepted['inputs'].items()}
    pins=b.Pins(prepared)
    for p,h in [(OLD/'ROAD_LIFE_READY.json',OLD_READY_SHA),(OLD/'check_road_life_ready.py',OLD_CHECK_SHA)]:pins.add(p,h)
    native=load(N/'runtime-native-v1/READY.json');plan=load(N/'runtime-native-v1/BUILD_PLAN.json')
    require(native['ready'] and native['native_only'] and native['Windows_work_performed'] is False,'Native build not ready')
    require(native['application_units']==104 and native['full_application_dependency_closure_verified'],'Incomplete native closure')
    require(native['source']==str(N/'source-v1') and native['binary']==str(N/'runtime-native-v1/native-bin/rpg'),'Native build escaped stage')
    require(plan['source_sha256']==native['source_sha256'] and len(plan['units'])==104 and not plan['skip_player'],'Source/link plan differs')
    oldplan=load(OLD/'runtime-native-v1/BUILD_PLAN.json')
    require({r['logical_source'] for r in plan['units']}=={r['logical_source'] for r in oldplan['units']},'Native application units changed')
    origin=load(N/'SOURCE_ORIGIN.json')['baseline_sha256']
    changed={p for p,h in native['source_sha256'].items() if origin.get(p)!=h}
    allowed={'engine/world_viewer.cpp','engine/world_minimap.h','engine/world_corner_minimap.inc','engine/world_road_traffic_core.h'}
    require(changed==allowed and set(origin)<=set(native['source_sha256']),'Unexpected source delta: '+repr(changed))
    for rel,h in native['source_sha256'].items():pins.add(N/'source-v1'/rel,h)
    pins.add(native['binary'],native['sha256'])
    for name in ['READY.json','BUILD_PLAN.json']:pins.add(N/'runtime-native-v1'/name)
    for row in plan['units']:
        receipt=Path(row['target']).with_suffix('.json');data=load(receipt)
        require(data['command']==row['command'],'Object compile flags differ')
        pins.add(receipt);pins.add(row['target']);pins.add(row['dependency_file'])
        for p,h in data['actual_dependency_sha256'].items():pins.add(p,h)
    def authored(project):
        return {str(p.relative_to(project)):sha(p) for p in project.rglob('*')
                if p.is_file() and not p.is_symlink() and not b.mutable_relative(p.relative_to(project))}
    before,after=authored(OLD/'demo-project'),authored(N/'demo-project')
    require(set(before)==set(after),'Unexpected authored metadata added/removed')
    project_changes={p for p in after if before[p]!=after[p]}
    require(project_changes=={'scenery/authored_instances.txt'},'Unexpected project changes: '+repr(project_changes))
    for rel,h in after.items():pins.add(N/'demo-project'/rel,h)
    for name in REVIEWS:
        p=N/name;data=load(p);require(data.get('passed') is True,'Review not passed: '+name);pins.add(p)
        for path,h in b.hash_pairs(data):
            path=Path(path).absolute()
            if private_fixture(path):continue
            if path.is_relative_to(N/'demo-project') and b.mutable_relative(path.relative_to(N/'demo-project')):continue
            pins.add(path,h)
    scene=load(N/'review/native-road/ROAD_NATIVE_REVIEW.json')
    require(scene['native_sha256']==native['sha256'] and scene['actual_host_collision_route_validated'] and scene['native_gl_errors']==0,'Actual native scene not validated')
    private=N/'review/native-road/private-project'
    for rel,h in after.items():require(scene['inputs'].get(str(private/rel))==h,'Native/public scene mismatch: '+rel)
    routes={}
    for p in (N/'demo-project').iterdir():
        if not p.is_symlink():continue
        target=str(p.resolve(strict=True));routes[str(p)]=target
        require(scene['private_asset_root_bindings'].get(str(private/p.name))==target,'Actual native asset binding differs')
        oldpath=OLD/'demo-project'/p.name
        require(oldpath.is_symlink() and str(oldpath.resolve(strict=True))==target,'Unreviewed shared asset bank')
    for p in [Path(__file__),N/'build_native_candidate.py',N/'seed_native_cache.py',N/'SOURCE_ORIGIN.json',N/'launch_demo.sh',ROOT/'Launch_Valwood_Polished_Road_Demo.sh']:pins.add(p)
    pins.unchanged()
    result=dict(schema='valwood.road-polish.ready.v1',ready=True,native_only=True,Windows_work_performed=False,
                application_units=104,native_sha256=native['sha256'],inputs=pins.hashes,frozen_stats=pins.stats,
                asset_routes=routes,required_reviews=REVIEWS,source_changes=sorted(changed),project_changes=sorted(project_changes),
                baseline_sha256=OLD_READY_SHA,user_saves_untouched=True,hardware_fps_verified=False,
                limits=['Finite initial road composition and roadside art pass; not an AAA quality certification.',
                        'Traffic can queue at the uncleared first quest encounter; it resumes after combat is cleared.',
                        'Native offscreen software checks verify movement, collisions, pause/resume and composition, not hardware frame rate.',
                        '31 existing actors and existing asset families retained; no Windows build.'])
    temp=READY.with_suffix('.tmp');temp.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n');temp.replace(READY)
    print(json.dumps(dict(ready=True,native_sha256=native['sha256'],pins=len(pins.hashes))))

def check():
    # The full accepted asset bank remains independently checked, rather than
    # blessing a changed inode merely because a new derivative was published.
    old=baseline();b=old.B;r=load(READY)
    require(r.get('ready') and r.get('schema')=='valwood.road-polish.ready.v1' and r.get('baseline_sha256')==OLD_READY_SHA,'Road polish is not reviewed')
    require(set(r['inputs'])==set(r['frozen_stats']),'Incomplete dependency catalog')
    for p,h in r['inputs'].items():
        require(b.HEX.fullmatch(h),'Invalid expected digest');before=b.snapshot(p)
        if before[2]<b.SMALL_FILE or before!=r['frozen_stats'][p]:require(sha(p)==h and b.snapshot(p)==before,'Reviewed dependency changed: '+p)
    for p,target in r['asset_routes'].items():require(Path(p).is_symlink() and str(Path(p).resolve(strict=True))==target,'Shared asset route changed')
    print('Valwood polished road ready: mixed traffic, detailed verges and clearer minimap.')

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--write',action='store_true');ap.add_argument('--check-only',action='store_true');args=ap.parse_args()
    if args.write:publish()
    check()
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,TypeError,RuntimeError) as e:print(str(e),file=sys.stderr);sys.exit(1)
