#!/usr/bin/env python3
"""Check additive castle streets, or explicitly collect sequential native evidence.

Default is static-only. Native profiles use saved Custom85% manual Balanced
settings and never build, cook, edit preferences, or omit measured slow frames.
Captures are separate untimed300-frame jobs. Full input hashes run only before
and after the batch, with the same idle-instance lock as the native launcher.
"""
import argparse
from collections import Counter
import fcntl
import json
import math
import os
from pathlib import Path
import subprocess
import sys

from PIL import Image
import benchmark_graphics_settings as common
import check_tanthoon_bandit_camp as camp

ROOT=Path(__file__).resolve().parents[1]
PROJECT=ROOT/'projects/valwood_world'
SCENERY=PROJECT/'scenery'
LAYOUT=SCENERY/'castle_streets_layout.json'
MANIFEST=SCENERY/'castle_streets_manifest.json'
INSTANCES=SCENERY/'castle_streets_instances.txt'
VIEWS=('castle-west-street','castle-tavern','castle-tavern-interior','castle-shelter','castle-alleys')
RUNS=('day','dusk','route','market')
CASES={'day':('castle-west-street','day'),'dusk':('castle-tavern-interior','dusk'),
       'route':('castle-west-street','day'),'market':('castle-market','day')}
WIDTH,HEIGHT,WARMUP=1600,900,120
require=common.require
EXTRA_INPUTS=(Path(__file__).resolve(),ROOT/'tools/check_tanthoon_bandit_camp.py',ROOT/'tools/build_castle_streets.py')


def static_check():
    layout,manifest=json.loads(LAYOUT.read_text()),json.loads(MANIFEST.read_text())
    require(layout.get('schema_version')==manifest.get('schema_version')==1,'Unsupported castle streets schema')
    require(manifest.get('layout_source')==LAYOUT.name and manifest.get('instances_source')==INSTANCES.name,'Manifest points at different scenery')
    require(common.digest(LAYOUT)==manifest['layout_sha256'] and common.digest(INSTANCES)==manifest['instances_sha256'],'Stale layout/instance hashes')
    records=camp.read_records(INSTANCES);counts=Counter(row[0]for row in records)
    require(len(records)==manifest.get('record_count')==274 and dict(counts)==manifest['counts'],'Expected complete274-record street district')
    require(len(layout['records'])==len(records),'Layout pose inventory differs')
    for pose,row in zip(layout['records'],records):
        require(pose['alias']==row[0],'Layout alias order differs from actual source')
        expected=[pose['world_xz'][0]/100800+.5,pose['world_xz'][1]/(100800*2818/5009)+.5,
                  pose['yaw_degrees'],pose['scale'],pose['burial_m'],pose['elevation_m']]
        require(all(abs(a-b)<.0000011 for a,b in zip(row[1:],expected)),'Layout pose differs from actual six/seven-column source')
    protected=manifest['protected_sources']
    names={'authored_instances.txt','starting_area_instances.txt','rainforest_instances.txt','castle_village_instances.txt','tanthoon_bandit_camp_instances.txt'}
    require(len(protected)==5 and {Path(p['file']).name for p in protected}==names,'Original five source proofs are incomplete')
    combined=counts.copy();source_counts={INSTANCES.name:len(records)}
    for source in protected:
        path=SCENERY/Path(source['file']).name
        require(common.digest(path)==source['sha256'],f'Protected source changed: {path.name}')
        previous=camp.read_records(path);require(len(previous)==source['records'],f'Protected inventory changed: {path.name}')
        combined.update(p[0]for p in previous);source_counts[path.name]=len(previous)
    require(sum(combined.values())==manifest.get('combined_record_count')==5859,'Expected six scenery sources/5859records')
    require(max(combined.values())==manifest['largest_combined_alias_batch'] and max(combined.values())<=4096,'Combined alias capacity differs')
    require({c['id']for c in layout['inspection_cameras']}==set(VIEWS),'Expected all five authored inspection views')
    require(layout['ground']['height_edits']is False,'Street ground overlay unexpectedly edits terrain height')
    require(counts['village_tavern']==counts['castle_residents_shelter']==1 and counts['castle_residents_bedroll']==4,'Tavern/shelter/resting props missing')
    require(sum(n for alias,n in counts.items()if alias.startswith('purchased_house_'))==26,'Expected26 additional house frontages')
    points=manifest['route']['points_local_xz'];length=sum(math.dist(p,q)for p,q in zip(points,points[1:]))
    require(2<=len(points)<=64 and points[0]==points[-1],'Route must be a bounded closed return walk')
    require(all(len(p)==2 and all(common.finite_number(x)for x in p)for p in points),'Invalid route coordinates')
    require(abs(length-manifest['route']['length_m'])<1.e-7 and abs(length-585.072135955)<.0001,'Frozen full route length differs')
    require(manifest['route']['sampled_step_m']<=.25 and manifest['route']['clear_capsule_radius_m']>=.4 and manifest['route']['max_absolute_grade']<.05,'Authoring route proof weakened')
    require(manifest['route']['source_obb_extra_clearance_m']>0 and manifest['route']['street_full_width_extra_clearance_m']>0,'Negative authored road clearance')
    require(len(manifest['lights'])==4,'Expected four bounded practical lights')
    # This intentionally verifies small authored files, not every embedded
    # model byte. Full model/shader/executable fingerprints belong to the
    # locked native pre/post freeze, after any furniture edits are finished.
    return {'record_count':274,'combined_records':5859,'sources':source_counts,'largest_alias_batch':max(combined.values()),
            'route_length_m':length,'route_frames':route_frames(manifest),'route_waypoints':len(points),'views':list(VIEWS),
            'scope':'Source hashes, actual instance inventories, layout and authoring metadata. Production CPU collision and native route evidence are separate.'},manifest


def route_frames(manifest):
    distance=manifest['route']['length_m']
    require(common.finite_number(distance)and distance>0,'Invalid route length')
    return math.ceil((math.ceil(distance/3.5*60)+WARMUP+120)/120)*120


def input_metadata():
    result=common.metadata()
    for path in EXTRA_INPUTS:
        stat=path.stat();result[common.label(path)]=[stat.st_dev,stat.st_ino,stat.st_size,stat.st_mtime_ns,stat.st_ctime_ns]
    return result


def fingerprints_while_idle():
    lock=ROOT/'projects/seldonne/.valwood-instance.lock'
    require(lock.parent.is_dir(),'Prepare the shared game-instance directory first')
    with lock.open('a')as stream:
        try:fcntl.flock(stream.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError as error:raise ValueError('Close Valwood before full input fingerprinting')from error
        try:
            result=common.fingerprints()
            result.update({common.label(path):common.digest(path)for path in EXTRA_INPUTS})
            return result
        finally:fcntl.flock(stream.fileno(),fcntl.LOCK_UN)


def check_saved_preferences():
    state=camp.check_saved_preferences()
    keys={line.split('=',1)[0].strip()for line in(PROJECT/'graphics.settings').read_text().splitlines()if line.strip()and not line.lstrip().startswith('#')}
    require(keys=={'version','preset','auto_quality','target_fps','render_scale','min_render_scale','shadow_side','reflections','contact_shadows','volumetric_fog','clouds','reflection_scale'},'Saved graphics file has unknown/missing fields')
    return state


def early_opaque_check(world,frames,run,view,expected_enabled=True):
    state=world.get('early_opaque')
    require(isinstance(state,dict),'Missing early opaque telemetry')
    require(state.get('alias')=='village_tavern'and state.get('enabled')is expected_enabled,'Unexpected early opaque alias/mode')
    early,late=state.get('frames'),state.get('late_skipped_frames')
    require(all(isinstance(v,int)and not isinstance(v,bool)and 0<=v<=frames for v in(early,late)),'Invalid early/late opaque frame counters')
    require(early==late,'Early tavern draw and late exclusion counts differ')
    if not expected_enabled:require(early==0,'Disabled reference still drew early opaque geometry')
    elif run=='route':require(early>0,'Full walk never exercised the early interior pass')
    elif view=='castle-tavern-interior':require(early==frames,'Interior missed an early tavern/late exclusion frame')
    else:require(early==0,'Outside case incorrectly enabled the interior pass')
    return state


def native_world_check(stem,frames,timed,run,view,time_of_day,static,save_png,validate_early=False,expected_hour_override=None,route_contract=None):
    capture=stem.with_suffix('.bmp');metadata=json.loads(Path(str(capture)+'.json').read_text())
    renderer=str(metadata.get('renderer',''))
    require(('AMD'in renderer or'Radeon'in renderer)and not any(x in renderer.lower()for x in('llvmpipe','softpipe','swrast','software')),'Expected native AMD renderer')
    require(metadata.get('capture_ok')is True and metadata.get('offscreen')is False,'Capture failed or was offscreen')
    require((metadata.get('width'),metadata.get('height'),metadata.get('frames'))==(WIDTH,HEIGHT,frames),'Native drawable/frame count differs')
    with Image.open(capture)as image:
        require(image.size==(WIDTH,HEIGHT),'Capture is not full1600x900 drawable size')
        if save_png:image.save(stem.with_suffix('.png'))
    report=Path(str(stem.with_suffix('.json')if timed else capture)+'.world.json');world=json.loads(report.read_text())
    require(world.get('scenic')==view and world.get('benchmark')is timed,'Actual scenic/benchmark mode differs')
    for field in('gl_error','failed_tiles','remaining_texture_entries','remaining_texture_references','occlusion_failures','weather_failures'):
        require(world.get(field)==0,f'{field}: {world.get(field)!r}')
    g=world['graphics']
    require(g.get('preset')=='Custom'and g.get('automatic')is False and g.get('target_fps')==60,'Saved Custom/manual60FPS mode did not apply')
    require(g.get('apply_failures')==g.get('scale_changes')==0,'Fixed-quality allocation/scale regression')
    require((g.get('display_width'),g.get('display_height'),g.get('render_width'),g.get('render_height'))==(1600,900,1360,765),'Actual85% render/drawable size differs')
    require(all(common.finite_number(g.get(k))and abs(g[k]-.85)<=.001 for k in('effective_scale','minimum_observed_scale','maximum_observed_scale')),'Fixed85% scale drifted')
    require(g.get('reflections')is True and g.get('reflection_scale')==.5 and g.get('contact_shadows')is True and g.get('volumetric_fog')is True and g.get('volumetric_clouds')is True,'Balanced effects changed')
    require(world.get('shadow_map_side')==2048 and world.get('soft_shadow_taps')==9 and world.get('contact_lighting')is True and world.get('volumetric_clouds')is True,'Actual Balanced lighting passes differ')
    require(world.get('occlusion_frames')==frames,'Contact-shadow resolve missed frames')
    # The original static check still proves its frozen six-source inventory.
    # Additive district checks can supply their independently verified source
    # counts without weakening that historical baseline.
    require(world.get('authored_source_files')==len(static['sources']) and world.get('authored_instances')==static['combined_records'],'Rendered authored source/record counts differ from the verified inventory')
    sources=world['authored_sources'];actual={Path(s['path']).name:s['instances']for s in sources}
    require(len(sources)==len(static['sources']) and actual==static['sources'],'Actual source inventories differ')
    population,animation,streets=world['village_population'],world['village_animation'],world['castle_streets']
    require(population.get('residents')==140 and population.get('pose_assets')==80,'Full140-person/80-pose bank missing')
    require(animation.get('gpu_pose_blending')is True and animation.get('pose_batch_capacity')==95,'Expected95 bounded adjacent-pose batches')
    require(population.get('updates',0)>=frames and population.get('peak_visible',0)>0,'Population never updated/appeared')
    require(all(streets.get(k)==v for k,v in {'sheltered_residents':4,'tavern_residents':6,'street_residents':2,'seated':4,'practical_emitters':4,'global_flame_capacity':40}.items()),'New residents/seating/practical-light telemetry differs')
    frustum=world.get('population_frustum',{})
    if validate_early:
        require(frustum.get('enabled')is True and frustum.get('shadow_casters_unchanged')is True,'Population frustum optimization or original shadow path missing')
        require(all(isinstance(frustum.get(k),int)and frustum[k]>0 for k in('draw_calls','culled_batches','submitted_batches')),'Expected bounded population frustum submissions and conservative rejections')
    for family in('wildlife','bandit_camp'):
        state=world.get(family,{})
        require(state.get('load_failures')==state.get('unsafe_roots')==0,f'{family} load/support regression')
    travel=world.get('travel_loading_frames');require(isinstance(travel,int)and travel>=0,'Missing travel-hold counter')
    if run!='route'and route_contract is None:require(travel==0,'Stationary scene held loading images')
    route=world['benchmark_route']
    if run=='route'or route_contract is not None:
        expected=route_contract or{'id':'valdoa_west_streets_tavern_return','length_m':static['route_length_m'],'waypoints':static['route_waypoints']}
        require(route.get('active')is True and route.get('id')==expected['id'],'Expected complete authored walking route')
        require(common.finite_number(route.get('length_m'))and abs(route['length_m']-expected['length_m'])<.0001 and abs(route.get('speed_mps',0)-3.5)<.0001,'Actual route length/walking speed differs')
        for field in('collision_blocked_steps','slope_blocked_steps','sliding_steps'):require(route.get(field)==0,f'Full streets walk failed: {field}')
        require(route.get('complete')is True and common.finite_number(route.get('return_error_m'))and route['return_error_m']<.001,'Full walk did not finish and return')
        require(route.get('waypoints_reached')==expected['waypoints']-1,'Full route did not reach every authored waypoint')
    else:require(route.get('active')is False,'Stationary profile unexpectedly walked a route')
    expected_hour=17.8 if time_of_day=='dusk' else 16.05
    if expected_hour_override is not None:expected_hour=expected_hour_override
    require(abs(world['lighting']['hour']-expected_hour)<.01,'Actual day/dusk lighting differs')
    # Historical --verify-existing evidence predates this optimization. Require
    # the new contract for normal newly-launched cases, never retroactively
    # claim that an older report measured a pass which it did not contain.
    preparation=world.get('creek_water_preparation')
    if validate_early:
        early_opaque_check(world,frames,run,view)
        require(isinstance(preparation,dict),'Missing nearby water startup evidence')
        if run=='route':
            require(preparation.get('startup_ready')is True,'Nearby creek was not prepared before walking')
            require(preparation.get('first_reflection_frame')==392,'First creek visibility changed; preparation must preserve the route/frustum')
        if preparation.get('startup_ready')is True:
            require(preparation.get('startup_render_passes')==4,'Nearby water did not exercise all four startup material directions')
            require(all(preparation.get(k)==0 for k in('reflection_play_allocations','sky_reflection_play_allocations','water_scene_play_allocations')),'Prewarmed water allocated resources during play')
            require(common.finite_number(preparation.get('startup_ms'))and preparation['startup_ms']>=0,'Invalid startup preparation timing')
    return {'renderer':renderer,'graphics':g,'village_population':population,'village_animation':animation,'castle_streets':streets,
            'early_opaque':world.get('early_opaque'),'population_frustum':frustum,'creek_water_preparation':preparation,
            'lighting':world['lighting'],'travel_loading_frames':travel,'benchmark_route':route,'world_report':str(report),'capture':str(capture)}


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);mode=p.add_mutually_exclusive_group()
    mode.add_argument('--native',action='store_true',help='Explicitly launch sequential native jobs')
    mode.add_argument('--verify-existing',action='store_true',help='Read existing reports only; no launch/full runtime hashing/writes')
    p.add_argument('--captures',action='store_true',help='Separate untimed300-frame screenshots, no FPS claim')
    p.add_argument('--runs',nargs='+',choices=RUNS,help='Timed cases; default day,dusk,fullroute,market regression')
    p.add_argument('--views',nargs='+',choices=VIEWS,help='Capture views; default all five')
    p.add_argument('--times',nargs='+',choices=('day','dusk'),help='Capture times; default day,dusk')
    p.add_argument('--frames',type=int,help='Optional duration override; full route must still finish')
    p.add_argument('--output',type=Path,default=ROOT/'docs/performance/castle-streets/native')
    args=p.parse_args(argv)
    require(not args.captures or args.native or args.verify_existing,'--captures requires --native or --verify-existing')
    require(args.captures or not(args.views or args.times),'--views/--times are capture-only')
    require(not args.captures or not args.runs,'--runs selects timed cases; captures use --views/--times')
    require(args.frames is None or args.frames>(0 if args.captures else WARMUP),'Frames must be positive; profiles must exceed120warmup frames')
    static,manifest=static_check()
    if not args.native and not args.verify_existing:
        print(json.dumps({'static_pass':True,**static},indent=2));return 0
    preferences=check_saved_preferences()
    runs=args.runs or list(RUNS);views=args.views or list(VIEWS);times=args.times or['day','dusk']
    require(all(len(v)==len(set(v))for v in(runs,views,times)),'Duplicate runs/views/times')
    cases=[(time,view,time)for time in times for view in views]if args.captures else[(run,*CASES[run])for run in runs]
    require(args.captures or'route'not in runs or args.frames is None or args.frames>=route_frames(manifest),'Explicit route duration is too short')
    output=args.output.expanduser().resolve()
    for directory in(ROOT/'engine',ROOT/'shaders',PROJECT/'models',PROJECT/'scenery',PROJECT/'ground'):
        require(output!=directory and not output.is_relative_to(directory),'Evidence cannot be inside frozen runtime inputs')
    if args.verify_existing:require(output.is_dir(),'Existing evidence directory missing')
    else:
        require(not output.exists()or not any(output.iterdir()),'Evidence directory is not empty; choose a fresh directory')
        require((ROOT/'rpg').is_file()and os.access(ROOT/'rpg',os.X_OK)and common.LAUNCHER.is_file(),'Prepare executable/project first; helper never builds/cooks')
        output.mkdir(parents=True,exist_ok=True)
    environment,removed=common.native_environment();before=input_metadata()if args.native else None
    frozen=fingerprints_while_idle()if args.native else None
    if args.native:
        require(input_metadata()==before,'Inputs changed while preparing fingerprints')
        for model in manifest['models'].values():
            require(frozen.get(model['file'])==model['sha256'],f'Stale authored model bounds/hash: {model["file"]}; regenerate layout after the final asset cook')
        (output/'source-freeze.json').write_text(json.dumps({'sha256':frozen,'preferences_before':preferences,'removed_environment_keys':removed,'saved_profile':'Custom85% manual Balanced effects/60FPS; no graphics CLI overrides'},indent=2)+'\n')
    rows=[];failures=[];monitor=common.MemoryMonitor()
    if args.native:monitor.start()
    try:
        for run,view,time_of_day in cases:
            frames=args.frames or(300 if args.captures else route_frames(manifest)if run=='route' else 1800)
            timed=not args.captures;suffix='view'if args.captures else'final';stem=output/f'{run}-{view}-{suffix}'
            command=[str(common.LAUNCHER),'--scenic',view,'--width',str(WIDTH),'--height',str(HEIGHT),'--frames',str(frames),'--no-hud','--capture',str(stem.with_suffix('.bmp'))]
            if timed:command+=['--benchmark','--view','river'if run=='route'else'sweep','--warmup',str(WARMUP),'--profile',str(stem.with_suffix('.json'))]
            run_environment=environment.copy();run_environment['VALWOOD_VILLAGE_LIGHTING']='dusk'if time_of_day=='dusk'else'afternoon'
            row={'run':run,'view':view,'time':time_of_day,'frames':frames,'timed':timed,'command':command}
            try:
                if args.native:
                    print(f'Native castle streets {run}: {view} ({frames}frames)',flush=True)
                    with stem.with_suffix('.log').open('x')as stream:
                        process=subprocess.run(command,cwd=ROOT,env=run_environment,stdout=stream,stderr=subprocess.STDOUT,check=False)
                    require(process.returncode==0,f'Native job exited{process.returncode}; preserved log contains details')
                row.update(native_world_check(stem,frames,timed,run,view,time_of_day,static,save_png=args.native,validate_early=args.native))
                row['timing']=camp.load_timing(stem,frames)if timed else None
                row['strict_45fps_pass']=(row['timing']['mean_fps']>=45 and row['timing']['one_percent_low_fps']>=45)if timed else None
                row['all_frames_within_45fps_budget']=(row['timing']['max_ms']<=1000/45)if timed else None
                row['correctness_pass']=True
            except(OSError,ValueError,KeyError,TypeError)as error:row.update(correctness_pass=False,error=str(error))
            rows.append(row);print(json.dumps({k:v for k,v in row.items()if k not in('command','timing')}),flush=True)
            if args.native and(input_metadata()!=before or common.preference_state()!=preferences):
                failures.append('Inputs/preferences changed; remaining native jobs cancelled');break
    finally:
        if args.native:
            monitor.stop();(output/'gpu-memory.json').write_text(json.dumps(monitor.samples,indent=2)+'\n')
            try:
                require(fingerprints_while_idle()==frozen,'Frozen runtime files changed during batch')
                require(common.preference_state()==preferences,'Saved preferences changed; helper did not restore/overwrite user edits')
            except(OSError,ValueError)as error:failures.append(str(error))
        timed_rows=[r for r in rows if r.get('timing')]
        correct=len(rows)==len(cases)and all(r.get('correctness_pass')for r in rows)and not failures
        gate=(len(timed_rows)==len(cases)and all(r['strict_45fps_pass']for r in timed_rows))if not args.captures else None
        summary={'static':static,'runs':rows,'failures':failures,'correctness_pass':correct,'preferences_preserved':common.preference_state()==preferences,
                 'strict_45fps_gate':{'definition':'Mean FPS and1% low both>=45; all measured frame intervals retained','all_pass':gate,'failed_cases':[r['run']for r in timed_rows if not r['strict_45fps_pass']]},
                 'memory':monitor.summary()if args.native else None,'note':'Native profiles and untimed captures are separate. Complete route includes loading/wait intervals; no warmup/sample exclusions beyond the declared120frames. Existing verification reads reports and current small source proofs without rehashing frozen runtime files.'}
        if args.native:(output/'results.json').write_text(json.dumps(summary,indent=2)+'\n')
        else:print(json.dumps(summary,indent=2))
    return 0 if correct and(gate is True or gate is None) else 1


if __name__=='__main__':
    try:sys.exit(main())
    except(OSError,ValueError,KeyError,TypeError)as error:raise SystemExit(str(error))from error
