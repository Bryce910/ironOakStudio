#!/usr/bin/env python3
"""Explicit sequential native particles acceptance, or source-only validation.

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
       'alleys':('castle-alleys','day'),'exterior':('castle-tavern','day'),
       'rainforest':('rainforest-pool','day'),'campfire':('bandit-fire','dusk')}
require=common.require

def metadata():
    result=streets.input_metadata();p=Path(__file__).resolve();s=p.stat()
    result[common.label(p)]=[s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
    for p in (common.PROJECT/'shaders').rglob('*.glsl'):
        s=p.stat();result[common.label(p)]=[s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
    for p in (common.PROJECT/'particles').glob('*'):
        if p.is_file():
            st=p.stat();result[common.label(p)]=[st.st_dev,st.st_ino,st.st_size,st.st_mtime_ns,st.st_ctime_ns]
    return result

def fingerprints():
    result=streets.fingerprints_while_idle();p=Path(__file__).resolve()
    result[common.label(p)]=common.digest(p)
    for p in (common.PROJECT/'shaders').rglob('*.glsl'):result[common.label(p)]=common.digest(p)
    for p in (common.PROJECT/'particles').glob('*'):
        if p.is_file():result[common.label(p)]=common.digest(p)
    return result

def check_lighting(world,frames,fixed):
    b=world.get('hdr_bloom',{});t=world.get('tavern_light_return',{})
    require(b.get('enabled')is True and b.get('startup_ready')is True,'Bloom was not prepared before play')
    require(b.get('frames')==frames and b.get('resolves')==frames+1,'Bloom missed a normal frame or unexpected history advance')
    require((b.get('width'),b.get('height'),b.get('bytes'),b.get('allocations'),b.get('failures'))==(340,192,1044480,1,0),'Quarter-resolution bloom allocation/ownership/failure contract differs')
    require(abs(b.get('strength',0)-.035)<1e-6 and b.get('scene_linear')is True and b.get('post_timing_includes_bloom')is True,'Bloom strength/linear timing contract differs')
    require(t.get('enabled')is True and all(t.get(k)is True for k in ('steady_mean_sources','exterior_roof_excluded','composed_as_indirect')),'Tavern return disabled or light-domain contract missing')
    expected=[4*82/1044*r*c for r,c in zip((.38,.32,.25),(1,.47,.12))]
    require(len(t.get('irradiance',[]))==3 and all(common.finite_number(x)and abs(x-y)<1e-7 for x,y in zip(t['irradiance'],expected)),'Tavern return differs from four authored practicals')
    if fixed:require(abs(world['lighting']['visual_time']-22)<1e-5,'Capture animation time is not fixed22')
    return {'hdr_bloom':b,'tavern_light_return':t}

def check_particles(world,frames,expected_authored_first=True):
    a=world.get('scene_particles',{});art=world.get('purchased_particle_art',{})
    order=world.get('authored_depth_first',{})
    require(order.get('enabled')is expected_authored_first and all(order.get(k)is True for k in ('same_geometry','same_materials','single_submission')),'Authored depth ordering contract differs')
    if expected_authored_first and world.get('scenic','').startswith('castle-'):require(order.get('frames')==frames,'Village missed an authored-first frame')
    require(a.get('enabled')is True and a.get('startup_ready')is True,'Particles were not warmed before play')
    require((a.get('authored_emitters'),a.get('smoke_sources'),a.get('dust_sources'))==(14,8,6),'Authored particle sources differ')
    require((a.get('width'),a.get('height'),a.get('bytes'),a.get('gpu_buffer_bytes'),a.get('allocations'),a.get('failures'))==(1360,765,8323200,24576,1,0),'Particle target/allocation/failure contract differs')
    require(a.get('draws')==a.get('resolves')==a.get('instance_uploads') and a['draws']>0,'Particles missed normal/warm draws')
    require(all(a.get(k)is True for k in ('post_contact_pre_temporal','weather_timing_includes_particles')),'Particle ordering/timing contract differs')
    require(art.get('fire')is True and art.get('smoke')is True and art.get('fire_frames')==144,'Purchased particle art missing')
    require(world.get('weather_cover_queries',0)>0,'Roof-cache callback was never used')
    return {'scene_particles':a,'purchased_particle_art':art}

def other_world_check(stem,frames,timed,view):
    from PIL import Image
    capture=stem.with_suffix('.bmp');m=json.loads(Path(str(capture)+'.json').read_text())
    renderer=m.get('renderer','')
    require(('AMD'in renderer or'Radeon'in renderer)and 'llvmpipe'not in renderer,'Native AMD renderer missing')
    require(m.get('capture_ok')is True and m.get('offscreen')is False,'Native capture failed')
    require((m.get('width'),m.get('height'),m.get('frames'))==(1600,900,frames),'Native drawable differs')
    with Image.open(capture)as image:image.save(stem.with_suffix('.png'))
    report=Path(str(stem.with_suffix('.json')if timed else capture)+'.world.json');w=json.loads(report.read_text())
    require(w.get('scenic')==view and w.get('benchmark')is timed,'Scene/mode differs')
    for key in ('gl_error','failed_tiles','remaining_texture_entries','remaining_texture_references','occlusion_failures','weather_failures'):
        require(w.get(key)==0,f'{key} failure')
    g=w['graphics'];require(g.get('automatic')is False and g.get('target_fps')==60 and g.get('effective_scale')==.85,'Saved manual85%/60FPS settings differ')
    require((g.get('render_width'),g.get('render_height'),w.get('shadow_map_side'))==(1360,765,2048),'Resolution/shadow settings differ')
    require(all(g.get(k)is True for k in ('reflections','contact_shadows','volumetric_fog','volumetric_clouds')),'Saved effects disabled')
    if view.startswith('rainforest'):
        require(w.get('rain_instances')==4096 and w.get('splash_instances')==768,'Full rain/splash budget missing')
    return {'renderer':renderer,'graphics':g,'world_report':str(report),'capture':str(capture)}

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--native',action='store_true');p.add_argument('--captures',action='store_true')
    p.add_argument('--runs',nargs='+',choices=tuple(CASES))
    p.add_argument('--output',type=Path,default=ROOT/'docs/performance/cinematic-particles/native')
    args=p.parse_args(argv);static,manifest=streets.static_check()
    for authored in (ROOT/'shaders').glob('world_*.glsl'):
        runtime=common.PROJECT/'shaders'/authored.name
        require(runtime.is_file(),f'Missing project runtime shader: {runtime}')
        require(common.digest(runtime)==common.digest(authored),f'Project shader differs from authored source: {authored.name}')
    if not args.native:
        print(json.dumps({'static_pass':True,**static},indent=2));return 0
    runs=args.runs or(['hero','tavern','exterior']if args.captures else['route','hero','tavern','market','campfire','rainforest'])
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
            print('Native particles',name,frames,'frames',flush=True)
            try:
                with stem.with_suffix('.log').open('x')as log:process=subprocess.run(command,env=runenv,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
                require(process.returncode==0,f'Engine exit{process.returncode}; log retained')
                if name in ('rainforest','campfire'):row.update(other_world_check(stem,frames,not args.captures,view))
                else:row.update(streets.native_world_check(stem,frames,not args.captures,'route'if name=='route'else name,view,time,static,True,validate_early=True,expected_hour_override=17.8 if time=='dusk' else 16.65))
                world=json.loads(Path(row['world_report']).read_text())
                row.update(check_lighting(world,frames,args.captures));row.update(check_particles(world,frames))
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
        summary={'correctness_pass':correct,'strict_45fps_pass':gate,'untimed':args.captures,'preferences_preserved':common.preference_state()==preferences,'runs':rows,'failures':failures,'memory':monitor.summary(),'timing_scope':'All intervals after declared120warmup retained. Weather, rain/splashes, smoke/dust and bloom included in timed rendering. Untimed captures make no FPS claim.'}
        (output/'results.json').write_text(json.dumps(summary,indent=2)+'\n')
    return 0 if correct and gate is not False else 1

if __name__=='__main__':
    try:sys.exit(main())
    except(OSError,ValueError,KeyError,TypeError)as error:raise SystemExit(str(error))from error
