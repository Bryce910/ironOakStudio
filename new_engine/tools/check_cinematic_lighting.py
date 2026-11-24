#!/usr/bin/env python3
"""Explicit sequential native lighting acceptance, or source-only validation.

Preserves saved settings. Timed jobs and fixed-time untimed captures are
separate; all declared post-warmup intervals remain in the performance gate.
No build, cook or full hash runs while the GPU renderer is active.
"""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys
import benchmark_graphics_settings as common
import check_castle_streets as streets
import check_tanthoon_bandit_camp as camp
ROOT=common.ROOT
CASES={'route':('castle-west-street','day'),'hero':('castle-hero-entry','day'),
       'tavern':('castle-tavern-interior','dusk'),'market':('castle-market','day'),
       'alleys':('castle-alleys','day'),'exterior':('castle-tavern','day')}
require=common.require

def metadata():
    result=streets.input_metadata();p=Path(__file__).resolve();s=p.stat()
    result[common.label(p)]=[s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
    for p in (common.PROJECT/'shaders').rglob('*.glsl'):
        s=p.stat();result[common.label(p)]=[s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
    return result

def fingerprints():
    result=streets.fingerprints_while_idle();p=Path(__file__).resolve()
    result[common.label(p)]=common.digest(p)
    for p in (common.PROJECT/'shaders').rglob('*.glsl'):result[common.label(p)]=common.digest(p)
    return result

def check_lighting(world,frames,fixed):
    b=world.get('hdr_bloom',{});t=world.get('tavern_light_return',{})
    require(b.get('enabled')is True and b.get('startup_ready')is True,'Bloom was not prepared before play')
    require(b.get('frames')==frames and b.get('resolves')==frames+1,'Bloom missed a normal frame or unexpected history advance')
    require((b.get('width'),b.get('height'),b.get('bytes'),b.get('allocations'),b.get('failures'))==(340,192,1044480,1,0),'Quarter-resolution bloom allocation/ownership/failure contract differs')
    require(abs(b.get('strength',0)-.035)<1e-6 and b.get('scene_linear')is True and b.get('post_timing_includes_bloom')is True,'Bloom strength/linear timing contract differs')
    require(t.get('enabled')is True and all(t.get(k)is True for k in ('steady_mean_sources','exterior_roof_excluded','composed_as_indirect')),'Tavern return disabled or light-domain contract missing')
    require(len(t.get('irradiance',[]))==3 and all(common.finite_number(x)and 0<x<.1 for x in t['irradiance']),'Tavern return energy is not finite/bounded')
    if fixed:require(abs(world['lighting']['visual_time']-22)<1e-5,'Capture animation time is not fixed22')
    return {'hdr_bloom':b,'tavern_light_return':t}

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--native',action='store_true');p.add_argument('--captures',action='store_true')
    p.add_argument('--runs',nargs='+',choices=tuple(CASES))
    p.add_argument('--output',type=Path,default=ROOT/'docs/performance/cinematic-lighting/native')
    args=p.parse_args(argv);static,manifest=streets.static_check()
    for authored in (ROOT/'shaders').glob('world_*.glsl'):
        runtime=common.PROJECT/'shaders'/authored.name
        require(runtime.is_file(),f'Missing project runtime shader: {runtime}')
        require(common.digest(runtime)==common.digest(authored),f'Project shader differs from authored source: {authored.name}')
    if not args.native:
        print(json.dumps({'static_pass':True,**static},indent=2));return 0
    runs=args.runs or(['hero','tavern','exterior']if args.captures else['route','hero','tavern','market'])
    require(len(set(runs))==len(runs),'Duplicate cases')
    require(not args.captures or'route'not in runs,'Route belongs to the timed acceptance')
    output=args.output.resolve();require(not output.exists()or not any(output.iterdir()),'Choose a fresh evidence directory')
    for directory in (ROOT/'engine',ROOT/'shaders',common.PROJECT/'models',common.PROJECT/'ground',common.PROJECT/'scenery'):
        require(output!=directory and not output.is_relative_to(directory),'Evidence is inside runtime inputs')
    output.mkdir(parents=True,exist_ok=True)
    preferences=streets.check_saved_preferences();before=metadata();frozen=fingerprints()
    require(metadata()==before,'Runtime changed during initial fingerprint')
    env,removed=common.native_environment()
    (output/'source-freeze.json').write_text(json.dumps({'sha256':frozen,'preferences_before':preferences,'removed_environment_keys':removed,'saved_profile':'Custom85% manual60FPS,2Kshadow,halfreflection,contact/fog/clouds; no graphics CLI override'},indent=2)+'\n')
    rows=[];failures=[];monitor=common.MemoryMonitor();monitor.start()
    try:
        for name in runs:
            view,time=CASES[name];frames=180 if args.captures else streets.route_frames(manifest)if name=='route' else 1800
            stem=output/(name+'-'+view);runenv=env.copy();runenv['VALWOOD_VILLAGE_LIGHTING']='dusk'if time=='dusk'else'afternoon'
            if args.captures:runenv['VALWOOD_QA_FIXED_TIME']='1'
            command=[str(common.LAUNCHER),'--scenic',view,'--width','1600','--height','900','--frames',str(frames),'--no-hud','--capture',str(stem.with_suffix('.bmp'))]
            if not args.captures:command+=['--benchmark','--view','river'if name=='route'else'sweep','--warmup','120','--profile',str(stem.with_suffix('.json'))]
            row={'run':name,'view':view,'time':time,'frames':frames,'timed':not args.captures,'command':command}
            print('Native lighting',name,frames,'frames',flush=True)
            try:
                with stem.with_suffix('.log').open('x')as log:process=subprocess.run(command,env=runenv,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
                require(process.returncode==0,f'Engine exit{process.returncode}; log retained')
                row.update(streets.native_world_check(stem,frames,not args.captures,'route'if name=='route'else name,view,time,static,True,validate_early=True,expected_hour_override=17.8 if time=='dusk' else 16.65))
                row.update(check_lighting(json.loads(Path(row['world_report']).read_text()),frames,args.captures))
                if not args.captures:
                    row['timing']=camp.load_timing(stem,frames)
                    row['strict_45fps_pass']=row['timing']['mean_fps']>=45 and row['timing']['one_percent_low_fps']>=45
                    row['all_frames_within_45fps_budget']=row['timing']['max_ms']<=1000/45
                row['correctness_pass']=True
            except(OSError,ValueError,KeyError,TypeError)as error:row.update(correctness_pass=False,error=str(error))
            rows.append(row)
            print(json.dumps({k:row.get(k)for k in ('run','correctness_pass','error','strict_45fps_pass')},separators=(',',':')),flush=True)
            if metadata()!=before or common.preference_state()!=preferences:
                failures.append('Runtime/preferences changed; subsequent jobs cancelled');break
    finally:
        monitor.stop();(output/'gpu-memory.json').write_text(json.dumps(monitor.samples,indent=2)+'\n')
        try:
            require(fingerprints()==frozen,'Final runtime fingerprints changed')
            require(common.preference_state()==preferences,'Saved preferences changed; no overwrite/restore attempted')
        except(OSError,ValueError)as error:failures.append(str(error))
        correct=len(rows)==len(runs)and all(r.get('correctness_pass')for r in rows)and not failures
        gate=None if args.captures else correct and all(r.get('strict_45fps_pass')for r in rows)
        summary={'correctness_pass':correct,'strict_45fps_pass':gate,'untimed':args.captures,'preferences_preserved':common.preference_state()==preferences,'runs':rows,'failures':failures,'memory':monitor.summary(),'timing_scope':'All intervals after declared120warmup retained. Bloom extraction/blur/composite included in timed rendering. Untimed captures make no FPS claim.'}
        (output/'results.json').write_text(json.dumps(summary,indent=2)+'\n')
    return 0 if correct and gate is not False else 1

if __name__=='__main__':
    try:sys.exit(main())
    except(OSError,ValueError,KeyError,TypeError)as error:raise SystemExit(str(error))from error
