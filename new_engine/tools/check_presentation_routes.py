#!/usr/bin/env python3
"""Fixed-quality full walking routes for the packaged player-facing preview."""
import argparse,hashlib,json,math,os,subprocess,sys,tempfile
from pathlib import Path
from benchmark_graphics_settings import native_environment,MemoryMonitor

ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__);group=parser.add_mutually_exclusive_group(required=True);group.add_argument('--package',type=Path);group.add_argument('--project',type=Path);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--camera',choices=['first','third','both'],default='both');parser.add_argument('--render-scale',type=float,choices=[.7,.8,.85,.9,1.],default=.7);parser.add_argument('--milestone',action='store_true',help='Walk the focused courtyard and short jungle path');parser.add_argument('--route',choices=['both','jungle','castle'],default='both');parser.add_argument('--studio',type=Path,help='Optional staged Studio document for a controlled comparison');args=parser.parse_args()
    package=args.package.resolve()if args.package else None;project=package/'project'if package else args.project.resolve();binary=package/'bin/rpg'if package else ROOT/'rpg';out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    inputs=[binary,project/'world.studio',project/'graphics.settings',project/'scenery/demo_completion_instances.txt'];frozen={str(p):sha(p)for p in inputs}
    (out/'freeze-before.json').write_text(json.dumps(frozen,indent=2)+'\n');rows=[]
    with tempfile.TemporaryDirectory(prefix='valwood-preview-routes-')as temp:
        for camera in(['third','first']if args.camera=='both'else[args.camera]):
            routes=([('jungle','rainforest-milestone',2100,2*(math.hypot(10,.5032941)+math.hypot(17,35))),('castle','castle-milestone',1000,2*(10.4+10.58474745458))]if args.milestone else[('jungle','rainforest',7700,432.632946),('castle','castle-west-street',10500,585.070)])
            for label,scene,frames,distance in routes:
                if args.route!='both'and label!=args.route:continue
                name=camera+'-'+label;profile=out/(name+'.json');env,_=native_environment();env.update(VALWOOD_CAPTURE_VIEW=camera,VALWOOD_QA_FRAME_TIMES_ONLY='1',VALWOOD_QA_PROGRESS_FILE=str(Path(temp)/'preview.progress'))
                env['VALWOOD_PRESENTATION']='1'
                if args.studio:env['VALWOOD_QA_STUDIO_DOCUMENT']=str(args.studio.resolve())
                cmd=([str(package/'launch.sh')]if package else[str(binary),'--project',str(project),'--play','--no-hud','--width','1600','--height','900'])+['--scenic',scene,'--benchmark','--view','river','--frames',str(frames),'--warmup','120','--auto-quality','off','--render-scale',str(args.render_scale),'--profile',str(profile)]
                monitor=MemoryMonitor();monitor.start()
                with(out/(name+'.log')).open('w')as log:
                    try:code=subprocess.run(cmd,cwd='/tmp',env=env,stdout=log,stderr=subprocess.STDOUT,timeout=360).returncode
                    except subprocess.TimeoutExpired:code=-1
                monitor.stop()
                (out/(name+'.memory.json')).write_text(json.dumps(dict(summary=monitor.summary(),samples=monitor.samples),indent=2)+'\n')
                wp=Path(str(profile)+'.world.json');world=json.loads(wp.read_text())if wp.exists()else{};route=world.get('benchmark_route',{});graphics=world.get('graphics',{})
                audit=out/(name+'.audit.json');checked=subprocess.run([sys.executable,str(ROOT/'tools/check_world_performance.py'),str(profile),'--output',str(audit)],capture_output=True,text=True)
                (out/(name+'.audit.log')).write_text(checked.stdout+checked.stderr)
                data=json.loads(audit.read_text())if audit.exists()else{};stats=(data.get('profiles')or[{}])[0]
                quality=all(graphics.get(k)==v for k,v in dict(display_width=1600,display_height=900,render_width=int(1600*args.render_scale),render_height=int(900*args.render_scale),automatic=False,scale_changes=0,reflections=True,contact_shadows=True,volumetric_fog=True,volumetric_clouds=True).items())
                correct=code==0 and world.get('gl_error')==0 and world.get('remaining_texture_references')==0 and route.get('complete')is True and route.get('collision_blocked_steps')==0 and route.get('slope_blocked_steps')==0 and abs(route.get('actual_distance_m',0)-distance)<.02 and route.get('return_error_m',1)<.001 and quality and stats.get('native_renderer')is True and not stats.get('correctness_errors')
                row=dict(camera=camera,scene=scene,memory=monitor.summary(),correctness=correct,quality_unchanged=quality,average_fps=stats.get('mean_fps'),one_percent_low_fps=stats.get('one_percent_low_fps'),worst_ms=stats.get('worst_frame_ms'),misses=stats.get('frames_over_budget'),samples=stats.get('samples'),statistical_pass=correct and bool(stats.get('statistical_target_pass')),strict_floor_pass=correct and bool(stats.get('every_frame_within_budget')),preview_enabled=world.get('playable_preview',{}).get('enabled'),route=route)
                rows.append(row);(out/'results.json').write_text(json.dumps(dict(runs=rows),indent=2)+'\n');print(json.dumps(row),flush=True)
    after={name:sha(Path(name))for name in frozen};unchanged=after==frozen
    (out/'freeze-after.json').write_text(json.dumps(dict(unchanged=unchanged,sha256=after),indent=2)+'\n')
    result=dict(passed=unchanged and all(r['statistical_pass']and r['preview_enabled']for r in rows),strict_floor_pass=all(r['strict_floor_pass']for r in rows),inputs_unchanged=unchanged,runs=rows,note='Statistical walking target and strict every-frame floor reported separately. No capture/encoding, compilation or asset scans during walking intervals.')
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');return 0 if result['passed']else 1
if __name__=='__main__':raise SystemExit(main())
