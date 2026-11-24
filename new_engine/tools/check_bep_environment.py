#!/usr/bin/env python3
"""Validate additive purchased scenery and collect sequential native evidence.

No quality overrides, builds, cooks, input mutations or hash scans during
rendering. Untimed visual captures are separate from the frame-time gate.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import subprocess
import sys

import benchmark_graphics_settings as common
import check_castle_streets as streets
import check_cinematic_particles as particles
import check_tanthoon_bandit_camp as camp

ROOT=common.ROOT
EVIDENCE=ROOT/'docs/performance/bep-environment'
SCENERY=common.PROJECT/'scenery'
CASES={**particles.CASES,
       'garden':('castle-purchased-gardens','day'),
       'creek':('castle-creek-garden','day'),
       'undergrowth':('rainforest-purchased-understory','day')}
require=common.require

def static_check(authored_updates=None):
    old,route=streets.static_check()
    protected=json.loads((EVIDENCE/'before-inputs.json').read_text())
    updates=authored_updates or {}
    purposes={'projects/valwood_world/scenery/village_life_layout.json':'Tavern bartender route and host variety'}
    purposes.update({'projects/valwood_world/models/'+name:'Restore original Body UVmap on genuine seated poses'
                     for name in [*[f'village_character_seated_idle_{i:02}.glb'for i in range(4)],'village_seated_manifest.json']})
    require(set(updates)<=set(purposes),'Unexpected prior authored input update')
    for name,digest in protected.items():
        if name=='rpg':continue
        if name in updates:
            update=updates[name]
            require(update['before_sha256']==digest and update['purpose']==purposes[name],'Unproven authored input update')
            require(common.digest(ROOT/name)==update['after_sha256'],'Updated authored input changed: '+name)
        else:require(common.digest(ROOT/name)==digest,'Original runtime asset/preferences changed: '+name)
    if updates:subprocess.run([sys.executable,str(ROOT/'tools/build_village_life.py'),'--check'],check=True)
    path=SCENERY/'bep_environment_instances.txt'
    records=camp.read_records(path);counts=Counter(r[0]for r in records)
    expected={'bep_nature_'+name for name in ('oak','beech','bush_a','bush_b','grass_long','grass_broad','fern','reed','clover','palm')}
    expected|={'bep_prop_'+name for name in ('barrel','chest','bench','bucket','fence','fence_broken')}
    expected|={'bep_structure_well','bep_structure_storehouse'}
    require(set(counts)==expected,'Expected all18 purchased model families in the actual source')
    require(500<len(records)<=1300 and old['combined_records']+len(records)<=8192,'New scenery exceeds its bounded instance budget')
    for alias in counts:
        for lod in ('near','mid'):
            require((common.PROJECT/'models'/f'{alias}_{lod}.glb').is_file(),'Missing purchased mesh LOD: '+alias)
    for tool in ('import_bep_nature.py','import_bep_props.py'):
        subprocess.run([sys.executable,str(ROOT/'tools'/tool),'--validate'],check=True)
    subprocess.run([sys.executable,str(ROOT/'tools/build_bep_environment.py'),'--validate-only'],check=True)
    result={**old,'sources':{**old['sources'],path.name:len(records)},
            'combined_records':old['combined_records']+len(records),
            'added_records':len(records),'added_families':len(counts),'added_counts':dict(counts),
            'protected_files':len(protected)-1}
    return result,route

def metadata():
    result=particles.metadata()
    for p in (Path(__file__).resolve(),ROOT/'tools/import_bep_nature.py',ROOT/'tools/import_bep_props.py',ROOT/'tools/build_bep_environment.py'):
        st=p.stat();result[common.label(p)]=[st.st_dev,st.st_ino,st.st_size,st.st_mtime_ns,st.st_ctime_ns]
    return result

def fingerprints():
    result=particles.fingerprints()
    for p in (Path(__file__).resolve(),ROOT/'tools/import_bep_nature.py',ROOT/'tools/import_bep_props.py',ROOT/'tools/build_bep_environment.py'):
        result[common.label(p)]=common.digest(p)
    return result

def main():
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--native',action='store_true');cli.add_argument('--captures',action='store_true')
    cli.add_argument('--reference-order',action='store_true',help='Untimed rainforest-only A/B with authored-first disabled')
    cli.add_argument('--runs',nargs='+',choices=tuple(CASES))
    cli.add_argument('--frames',type=int,help='Stationary frame count; default1800 timed/180 capture')
    cli.add_argument('--output',type=Path,default=EVIDENCE/'native')
    args=cli.parse_args();static,route=static_check()
    require(not args.reference_order or(args.native and args.captures),'Reference order is only for untimed native captures')
    if not args.native:
        print(json.dumps({'static_pass':True,**static},indent=2));return 0
    runs=args.runs or(['garden','creek','undergrowth']if args.captures else['route','hero','market','garden','creek','rainforest','undergrowth'])
    require(len(set(runs))==len(runs),'Duplicate cases')
    require(not args.captures or 'route'not in runs,'Full walk belongs to timed acceptance')
    require(not args.reference_order or all(CASES[n][0].startswith('rainforest')for n in runs),'Reference order is limited to the rainforest A/B')
    require(args.frames is None or args.frames>(0 if args.captures else 120),'Insufficient frames')
    output=args.output.resolve();require(not output.exists()or not any(output.iterdir()),'Choose a fresh evidence directory')
    for p in (ROOT/'engine',ROOT/'shaders',common.PROJECT):require(not output.is_relative_to(p),'Evidence overlaps runtime inputs')
    output.mkdir(parents=True,exist_ok=True)
    preferences=streets.check_saved_preferences();before=metadata();frozen=fingerprints()
    require(metadata()==before,'Inputs changed during fingerprinting')
    env,removed=common.native_environment()
    (output/'source-freeze.json').write_text(json.dumps({'sha256':frozen,'preferences_before':preferences,'static':static,
        'removed_environment_keys':removed,'profile':'Saved Custom85% manual60FPS; all original effects retained.'},indent=2)+'\n')
    rows=[];failures=[];monitor=common.MemoryMonitor();monitor.start()
    try:
        for name in runs:
            view,tod=CASES[name]
            frames=streets.route_frames(route)if name=='route'else args.frames or(180 if args.captures else 1800)
            stem=output/(name+'-'+view);runenv=env.copy()
            runenv['VALWOOD_VILLAGE_LIGHTING']='dusk'if tod=='dusk'else'afternoon'
            if args.captures:runenv['VALWOOD_QA_FIXED_TIME']='1'
            if args.reference_order:runenv['VALWOOD_QA_DISABLE_AUTHORED_FIRST']='1'
            command=[str(common.LAUNCHER),'--scenic',view,'--width','1600','--height','900','--frames',str(frames),'--no-hud','--capture',str(stem.with_suffix('.bmp'))]
            if not args.captures:command+=['--benchmark','--view','river'if name=='route'else'sweep','--warmup','120','--profile',str(stem.with_suffix('.json'))]
            row={'run':name,'view':view,'frames':frames,'timed':not args.captures,'reference_order':args.reference_order,'command':command}
            print('Native purchased environment',name,frames,'frames',flush=True)
            try:
                with stem.with_suffix('.log').open('x')as log:
                    process=subprocess.run(command,cwd=ROOT,env=runenv,stdout=log,stderr=subprocess.STDOUT)
                require(process.returncode==0,f'Engine exit{process.returncode}; log retained')
                if view.startswith('castle-'):
                    row.update(streets.native_world_check(stem,frames,not args.captures,name,view,tod,static,True,validate_early=True,expected_hour_override=17.8 if tod=='dusk'else 16.65))
                else:row.update(particles.other_world_check(stem,frames,not args.captures,view))
                world=json.loads(Path(row['world_report']).read_text())
                actual={Path(s['path']).name:s['instances']for s in world['authored_sources']}
                require(actual==static['sources']and world['authored_instances']==static['combined_records'],'Purchased layer absent from renderer')
                require(world['texture_cache_unmanaged_uploads']==0,'Shared texture cache fell back to duplicate uploads')
                submit=world.get('early_gpu_submission',{})
                require(submit.get('enabled')is True and submit.get('frames')==frames and submit.get('waits_for_completion')is False,'Early nonblocking scene submission differs')
                row.update(particles.check_lighting(world,frames,args.captures));row.update(particles.check_particles(world,frames,expected_authored_first=not args.reference_order))
                if view.startswith('rainforest'):
                    require(world['authored_depth_first']['frames']==(0 if args.reference_order else frames),'Rainforest depth ordering count differs')
                row['texture_cache_entries']=world['texture_cache_entries_at_load']
                if not args.captures:
                    row['timing']=camp.load_timing(stem,frames)
                    row['strict_45fps_pass']=row['timing']['mean_fps']>=45 and row['timing']['one_percent_low_fps']>=45
                    row['all_frames_within_45fps_budget']=row['timing']['max_ms']<=1000/45
                row['correctness_pass']=True
            except(OSError,ValueError,KeyError,TypeError)as error:row.update(correctness_pass=False,error=str(error))
            rows.append(row)
            print(json.dumps({k:row.get(k)for k in('run','correctness_pass','strict_45fps_pass','error')},separators=(',',':')),flush=True)
            if metadata()!=before or common.preference_state()!=preferences:
                failures.append('Runtime/settings changed; subsequent cases cancelled');break
    finally:
        monitor.stop();(output/'gpu-memory.json').write_text(json.dumps(monitor.samples,indent=2)+'\n')
        if fingerprints()!=frozen:failures.append('Final runtime fingerprints changed')
        if common.preference_state()!=preferences:failures.append('Saved preferences changed')
        correct=len(rows)==len(runs)and all(r.get('correctness_pass')for r in rows)and not failures
        gate=None if args.captures else correct and all(r.get('strict_45fps_pass')for r in rows)
        (output/'results.json').write_text(json.dumps({'correctness_pass':correct,'strict_45fps_pass':gate,'untimed':args.captures,
            'preferences_preserved':common.preference_state()==preferences,'static':static,'runs':rows,'failures':failures,
            'memory':monitor.summary(),'timing_scope':'Every interval after120warmup retained; no cook/build/hash work during native rendering.'},indent=2)+'\n')
    return 0 if correct and gate is not False else 1

if __name__=='__main__':
    try:sys.exit(main())
    except(OSError,ValueError,KeyError,TypeError)as error:raise SystemExit(str(error))from error
