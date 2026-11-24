#!/usr/bin/env python3
"""Full native walks with asset/source freeze and explicit statistical/floor gates.

Run only after visual review and an explicit shared-engine GPU/build release.
No builds, imports, hashes or unrelated tests run during either walking interval.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from benchmark_graphics_settings import ROOT, PROJECT, LAUNCHER, native_environment


def sha(path):
    digest=hashlib.sha256()
    with path.open('rb') as handle:
        for block in iter(lambda:handle.read(1024*1024),b''):
            digest.update(block)
    return digest.hexdigest()


def inputs():
    paths={ROOT/'rpg',ROOT/'Makefile',PROJECT/'world.studio',PROJECT/'graphics.settings',
           Path(__file__).resolve(),ROOT/'engine/production.mk'}
    for folder in ('engine','shaders'):
        paths.update(p for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in ('.cpp','.c','.h','.inc','.glsl'))
    for folder in ('models','scenery','ground','environments','surfaces','ai'):
        base=PROJECT/folder
        if base.exists():
            paths.update(p for p in base.rglob('*') if p.is_file())
    return sorted(paths)


def meta(paths):
    return {str(p.relative_to(ROOT)):[p.stat().st_size,p.stat().st_mtime_ns,p.stat().st_ino] for p in paths}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--camera',choices=['first','third'],default='third')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    paths=inputs();metadata=meta(paths)
    frozen={str(p.relative_to(ROOT)):sha(p) for p in paths}
    (out/'source-freeze.json').write_text(json.dumps(dict(sha256=frozen,metadata=metadata,camera=args.camera),indent=2)+'\n')
    counters={}
    for device in Path('/sys/class/drm').glob('card[0-9]*/device'):
        row={}
        for name in ('gpu_busy_percent','mem_info_vram_total','mem_info_vram_used'):
            p=device/name
            if p.exists():
                row[name]=p.read_text().strip()
        if row:
            counters[str(device)]=row
    (out/'preflight.json').write_text(json.dumps(dict(gpu=counters,
        scope='Saved quality; current desktop application state retained; no unrelated apps closed by this script'),indent=2)+'\n')
    results=[]
    for label,scene,frames,distance in [('jungle','rainforest',7700,432.632946),
                                      ('castle-tavern','castle-west-street',10500,585.07)]:
        env,_=native_environment();env.update(VALWOOD_CAPTURE_VIEW=args.camera,VALWOOD_QA_FRAME_TIMES_ONLY='1')
        profile=out/(label+'.json')
        command=[str(LAUNCHER),'--scenic',scene,'--no-hud','--frames',str(frames),'--warmup','120',
            '--width','1600','--height','900','--auto-quality','off','--profile',str(profile),'--benchmark','--view','river']
        with (out/(label+'.log')).open('w') as log:
            try:
                completed=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=420)
                code=completed.returncode
            except subprocess.TimeoutExpired:
                code=-1
        wp=Path(str(profile)+'.world.json')
        world=json.loads(wp.read_text()) if wp.exists() else {}
        route=world.get('benchmark_route',{})
        audit=out/(label+'.gate.json')
        checked=subprocess.run([sys.executable,str(ROOT/'tools/check_world_performance.py'),str(profile),'--output',str(audit)],
            cwd=ROOT,capture_output=True,text=True)
        (out/(label+'.gate.log')).write_text(checked.stdout+checked.stderr)
        data=json.loads(audit.read_text()) if audit.exists() else {}
        stats=(data.get('profiles') or [{}])[0]
        correct=code==0 and world.get('gl_error')==0 and world.get('remaining_texture_references')==0 and route.get('complete') is True
        correct=correct and route.get('collision_blocked_steps')==0 and route.get('slope_blocked_steps')==0
        correct=correct and abs(route.get('actual_distance_m',0)-distance)<.02 and route.get('return_error_m',1)<.001
        graphics=world.get('graphics') or {}
        quality=all(graphics.get(k)==v for k,v in dict(display_width=1600,display_height=900,
            render_width=1120,render_height=630,automatic=False,scale_changes=0,
            reflections=True,contact_shadows=True,volumetric_fog=True,volumetric_clouds=True).items())
        correct=correct and quality and stats.get('native_renderer') is True and not stats.get('correctness_errors')
        row=dict(scene=scene,camera=args.camera,exit=code,correctness_pass=correct,route=route,
            gl_error=world.get('gl_error'),references=world.get('remaining_texture_references'),
            quality_unchanged=quality,
            graphics=world.get('graphics'),live=world.get('live_world'),audit=data,
            statistical_target_pass=correct and bool(stats.get('statistical_target_pass')),
            strict_floor_pass=correct and bool(stats.get('every_frame_within_budget')),
            metadata_unchanged=meta(paths)==metadata)
        results.append(row)
        (out/'results.json').write_text(json.dumps(dict(camera=args.camera,runs=results),indent=2)+'\n')
        print(json.dumps(dict(scene=scene,correctness=correct,average=stats.get('mean_fps'),
            one_percent_low=stats.get('one_percent_low_fps'),misses=stats.get('frames_over_budget'),
            statistical=row['statistical_target_pass'],strict_floor=row['strict_floor_pass'])),flush=True)
        if not row['metadata_unchanged']:
            print('Input metadata changed: remaining routes cancelled; this window cannot certify the frozen build.',flush=True)
            break
    # Keep exact changes so a concurrent writer/build cannot silently turn a
    # valid-looking route average into acceptance evidence for a different build.
    changed=[]
    for name,digest in frozen.items():
        path=ROOT/name
        actual=sha(path) if path.exists() else None
        if actual!=digest:
            changed.append(dict(path=name,sha256_before=digest,sha256_after=actual))
    paths_after=inputs()
    added=sorted(str(p.relative_to(ROOT)) for p in set(paths_after)-set(paths))
    unchanged=not changed and not added
    (out/'freeze-after.json').write_text(json.dumps(dict(inputs_unchanged=unchanged,
        changed_inputs=changed,added_inputs=added,
        accepted_for_final_performance=unchanged and len(results)==2 and all(r['metadata_unchanged'] for r in results)),indent=2)+'\n')
    raise SystemExit(0 if unchanged and len(results)==2 and all(r['statistical_target_pass'] and r['metadata_unchanged'] for r in results) else 1)


if __name__=='__main__':
    main()
