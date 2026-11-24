#!/usr/bin/env python3
"""Run explicit native route QA: captures OR motion timing, with saved-state checks.

Default is a dry plan. --run owns the native GPU/instance lock for one session.
The same route is used in both modes; timing mode never reads display pixels.
"""
from pathlib import Path
import argparse,csv,fcntl,hashlib,json,os,re,subprocess,tempfile,time
import numpy as np
from PIL import Image,ImageDraw
from benchmark_graphics_settings import native_environment,MemoryMonitor

ROOT=Path(__file__).resolve().parents[1]
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def summarize(rows):
    times=np.array([float(r['frame_ms'])for r in rows],float)
    if not len(times):return {'frames':0}
    n=max(1,int(np.ceil(len(times)*.01)))
    return {'frames':len(times),'mean_fps':float(1000/times.mean()),'one_percent_low_fps':float(1000/np.sort(times)[-n:].mean()),'max_ms':float(times.max()),'p95_ms':float(np.percentile(times,95)),'frames_below45':int((times>1000/45).sum()),'frames_below30':int((times>1000/30).sum())}
def frame_metrics(rows):
    times=np.array([float(r['frame_ms'])for r in rows],float)
    return {'frames':len(times),'mean_ms':float(times.mean()),'p95_ms':float(np.percentile(times,95)),'max_ms':float(times.max())}if len(times)else{'frames':0}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--project',type=Path,default=ROOT/'projects/valwood_showcase');parser.add_argument('--route',type=Path);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--mode',choices=['timing','captures'],default='timing');parser.add_argument('--scale',type=float,default=.7);parser.add_argument('--warmup',type=int,default=120);parser.add_argument('--minimum-motion-frames',type=int,default=600);parser.add_argument('--run',action='store_true');parser.add_argument('--timeout',type=int,default=1800)
    parser.add_argument('--binary',type=Path,default=ROOT/'rpg_grandstone',help='Native review binary; defaults to the existing Grandstone build')
    parser.add_argument('--scenic',default='stronghold-town',help='Registered scenic view for the selected project')
    args=parser.parse_args();binary=args.binary.resolve()
    project=args.project.resolve();route=(args.route or project/'qa/source_review.route').resolve();out=args.output.resolve();metadata=json.loads(route.with_suffix('.json').read_text())
    assert metadata['cpu_geometry_audit'],'Native route must first pass source geometry audit'
    assert metadata['obstruction_cuts']==0,'Review obstructed spans before running native QA'
    out.mkdir(parents=True,exist_ok=True);maximum=metadata['requested_ticks']+args.warmup+3600
    command=[str(binary),'--project',str(project),'--play','--scenic',args.scenic,'--view','sweep','--no-hud','--benchmark','--frames',str(maximum),'--warmup',str(args.warmup),'--width','1600','--height','900','--auto-quality','off','--render-scale',str(args.scale),'--profile',str(out/'profile.json')]
    plan={'mode':args.mode,'route':str(route),'source_cpu_audit':str(route.with_suffix('.json')),'checkpoints':metadata['capture_checkpoints'],'maximum_frames':maximum,'command':command,'readbacks_enabled':args.mode=='captures','kind':metadata.get('kind','segmented visual fly-through; walking navigation unverified')}
    (out/'plan.json').write_text(json.dumps(plan,indent=2)+'\n')
    if not args.run:print(json.dumps(plan,indent=2));return
    lock=(ROOT/'projects/seldonne/.valwood-instance.lock').open('a')
    try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    except BlockingIOError:raise RuntimeError('Native GPU/instance is already in use; schedule an exclusive session')
    protected=[project/n for n in ['project.valwood','graphics.settings','world.studio','showcase.views','showcase.residents','showcase.flames','publisher_showcase.json','village_showcase.json','city_showcase.json','city.scene','city.sky','city.physics','ground/showcase.height','ground/countryside.height','ground/countryside.coverage']]
    protected.extend(p for p in(project/'saves').rglob('*')if p.is_file())
    for name in ['valwood_showcase','valwood_stronghold','valwood_visual','valwood_world']:
        protected.extend(p for p in(ROOT/'projects'/name/'saves').rglob('*')if p.is_file())
    before={str(p):digest(p)for p in protected if p.is_file()};shaders={str(p):digest(p)for p in(project/'shaders').glob('*.glsl')};route_hash=digest(route);binary_hash=digest(binary)
    (out/'protected-state-before.json').write_text(json.dumps({'files':before,'shaders':shaders,'route_sha256':route_hash,'binary_sha256':binary_hash},indent=2)+'\n')
    # Remove only this runner's generated outputs. Game/project data stay read-only.
    for pattern in ['checkpoint-*.bmp','checkpoint-*.png','profile.json*','route.json','route.frames.csv','result.json','contact-sheet.png']:
        for path in out.glob(pattern):path.unlink()
    with tempfile.TemporaryDirectory(prefix='valwood-source-review-')as temp:
        env,_=native_environment();env.update(DISPLAY=':0',XAUTHORITY='/home/computer/.Xauthority',VALWOOD_PRESENTATION='1',VALWOOD_QA_PROGRESS_FILE=str(Path(temp)/'journey.progress'),VALWOOD_QA_FRAME_TIMES_ONLY='1',VALWOOD_CAPTURE_VIEW='first',VALWOOD_QA_SOURCE_ROUTE=str(route),VALWOOD_QA_SOURCE_ROUTE_REPORT=str(out/'route'))
        if args.mode=='captures':env['VALWOOD_QA_SOURCE_ROUTE_CAPTURE']=str(out/'checkpoint')
        else:env.pop('VALWOOD_QA_SOURCE_ROUTE_CAPTURE',None)
        monitor=MemoryMonitor();monitor.start()
        process=None;renderer='';begin=time.monotonic()
        try:
            with(out/'run.log').open('w')as log:
                process=subprocess.Popen(command,cwd='/tmp',env=env,stdout=log,stderr=subprocess.STDOUT)
                begin=time.monotonic()
                while process.poll()is None:
                    elapsed=time.monotonic()-begin
                    text=(out/'run.log').read_text(errors='replace')
                    match=re.search(r'\[World\] Renderer ([^\n]+)',text)
                    if match:
                        renderer=match.group(1)
                        if 'AMD'not in renderer or any(word in renderer.lower()for word in['llvmpipe','softpipe','software']):
                            raise RuntimeError('Native GPU required; aborting before heavy draws: '+renderer)
                    (out/'process.json').write_text(json.dumps({'pid':process.pid,'elapsed_seconds':elapsed,'renderer':renderer,'running':True},indent=2)+'\n')
                    if(out/'STOP').exists():raise RuntimeError('QA session stopped by its explicit STOP file')
                    if elapsed>args.timeout:raise TimeoutError('Native route timed out')
                    time.sleep(.5)
                result=process
        finally:
            if process is not None and process.poll()is None:
                process.terminate()
                try:process.wait(timeout=10)
                except subprocess.TimeoutExpired:process.kill();process.wait()
            monitor.stop()
            if process is not None:
                (out/'process.json').write_text(json.dumps({'pid':process.pid,'elapsed_seconds':time.monotonic()-begin,'renderer':renderer,'running':False,'returncode':process.returncode},indent=2)+'\n')
    preserved=before=={str(p):digest(p)for p in protected if p.is_file()}
    shaders_preserved=shaders=={str(p):digest(p)for p in(project/'shaders').glob('*.glsl')}
    (out/'protected-state.json').write_text(json.dumps({'saved_state_and_views_unchanged':preserved,'shaders_unchanged':shaders_preserved},indent=2)+'\n')
    # Preserve viewable evidence even when a later runtime counter fails QA.
    images=[]
    for path in sorted(out.glob('checkpoint-*.bmp'),key=lambda p:int(p.name.split('-')[1])):
        png=path.with_suffix('.png');Image.open(path).convert('RGB').save(png);path.unlink();images.append(str(png))
    assert preserved,'Saved project/viewpoint/preference changed'
    assert shaders_preserved,'Shaders changed during native session'
    assert result.returncode==0,f'Native route failed; see {out/"run.log"}'
    route_result=json.loads((out/'route.json').read_text());perf=json.loads((out/'profile.json').read_text());world=json.loads((out/'profile.json.world.json').read_text())
    assert route_result['complete']and route_result['failures']==0,route_result
    assert route_result['checkpoints']==metadata['capture_checkpoints']
    assert 'AMD'in perf['renderer']and'llvmpipe'not in perf['renderer'].lower(),perf['renderer']
    for key in ['gl_error','failed_tiles','remaining_texture_references','weather_failures']:assert world[key]==0,(key,world[key])
    assert world['asset_streaming']['failed']==0 and world['stronghold']['failures']==0
    assert route_hash==digest(route)and binary_hash==digest(binary),'Route or binary changed during native session'
    rows=list(csv.DictReader((out/'route.frames.csv').open()));motion=[r for r in rows if r['phase']=='motion'and r['capture_readback']=='0'];areas={label:summarize([r for r in motion if r['label']==label])for label in dict.fromkeys(r['label']for r in rows)}
    assert len(motion)>=args.minimum_motion_frames,'No substantial in-motion performance coverage'
    if args.mode=='captures':
        assert len(images)==metadata['capture_checkpoints']and route_result['captures']==len(images)
        width,height,cols=400,250,4;sheet=Image.new('RGB',(width*cols,height*((len(images)+cols-1)//cols)),(15,18,22));draw=ImageDraw.Draw(sheet)
        for i,file in enumerate(images):
            image=Image.open(file);image.thumbnail((width,height-25));x,y=(i%cols)*width,(i//cols)*height;sheet.paste(image,(x,y+25));draw.text((x+5,y+5),Path(file).stem,fill=(240,240,240))
        sheet.save(out/'contact-sheet.png')
    else:assert not images and route_result['captures']==0 and not route_result['capture_readbacks_contaminate_timing']
    frame_areas={label:{'motion':frame_metrics([r for r in rows if r['label']==label and r['phase']=='motion'and r['capture_readback']=='0']),
        'held':frame_metrics([r for r in rows if r['label']==label and r['phase']=='hold'and r['capture_readback']=='0']),
        'loading_or_warmup_frames':sum(r['label']==label and r['phase']=='loading'for r in rows),
        'capture_readback_frames':sum(r['label']==label and r['capture_readback']=='1'for r in rows),
        'timing_contaminated_by_capture':args.mode=='captures'}for label in areas}
    report={'passed':True,'kind':plan['kind'],'mode':args.mode,'render_scale':args.scale,'renderer':perf['renderer'],'route':route_result,'motion':summarize(motion),'per_area':areas,'per_area_frame_metrics':frame_areas,'capture_timings_are_not_benchmark_results':args.mode=='captures','images':images,'saved_state_and_views_unchanged':True,'route_sha256':route_hash,'binary_sha256':binary_hash,'memory':monitor.summary(),'asset_streaming':world['asset_streaming'],'local_probes':world['local_probes'],'gl_error':world['gl_error'],'weather_failures':world['weather_failures']}
    (out/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

if __name__=='__main__':main()
