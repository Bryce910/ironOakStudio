#!/usr/bin/env python3
"""Sequential native animal habitat checks; timing runs have no sequence readbacks."""
import argparse,hashlib,json,os,subprocess,threading,time
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
SCENES={'farm':'wildlife-farm','woodland':'wildlife-woodland','rainforest':'wildlife-rainforest','market':'castle-market'}
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--scenes',nargs='+',choices=SCENES,default=list(SCENES))
    p.add_argument('--captures',action='store_true');p.add_argument('--return-check',action='store_true')
    p.add_argument('--frames',type=int);p.add_argument('--output',type=Path,default=ROOT/'docs/performance/wildmesh/native')
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    env=os.environ.copy()
    for key in list(env):
        if key.startswith('VALWOOD_QA_') or key.startswith('VALWOOD_DISABLE_') or key.endswith('_SEQUENCE') or key=='LIBGL_ALWAYS_SOFTWARE':env.pop(key,None)
    if env.get('SDL_VIDEODRIVER') in ('offscreen','dummy'):env.pop('SDL_VIDEODRIVER')
    env.update(SDL_AUDIODRIVER='dummy',VALWOOD_VILLAGE_LIGHTING='afternoon')
    inputs=[ROOT/'rpg',ROOT/'Makefile',*ROOT.glob('engine/world_*'),*ROOT.glob('engine/texture_cache.[ch]'),*ROOT.glob('shaders/world_*.glsl'),*ROOT.glob('projects/valwood_world/models/*'),*ROOT.glob('projects/valwood_world/scenery/*'),*ROOT.glob('projects/valwood_world/ground/village*')]
    inputs=sorted(x for x in inputs if x.is_file() and x.suffix!='.o')
    def freeze():return {str(x.relative_to(ROOT)):hashlib.sha256(x.read_bytes()).hexdigest() for x in inputs}
    frozen=freeze();(out/'source-freeze.json').write_text(json.dumps(frozen,indent=2)+'\n')
    stop=threading.Event();memory=[]
    def monitor():
        while not stop.is_set():
            row={'time':time.time()}
            for d in Path('/sys/class/drm').glob('card*/device'):
                for key in ('mem_info_vram_used','mem_info_vram_total','mem_info_gtt_used'):
                    try:row[key]=int((d/key).read_text())
                    except OSError:pass
            memory.append(row);stop.wait(1)
    t=threading.Thread(target=monitor,daemon=True);t.start();results=[]
    runs=[('return','wildlife-farm')] if a.return_check else [(x,SCENES[x]) for x in a.scenes]
    try:
        for name,scene in runs:
            frames=a.frames or (720 if a.return_check else 300 if a.captures else 1800)
            if frames<=(480 if a.return_check else 120 if not a.captures else 0):p.error('insufficient frames')
            stem=out/name;cmd=[str(ROOT.parent/'Launch_Valwood_World.sh'),'--scenic',scene,'--width','1600','--height','900','--frames',str(frames),'--no-hud','--capture',str(stem.with_suffix('.bmp'))]
            if not a.captures and not a.return_check:cmd+=['--benchmark','--view','sweep','--warmup','120','--profile',str(stem.with_suffix('.json'))]
            run_env=env.copy()
            if a.return_check:run_env['VALWOOD_QA_WILDLIFE_RETURN']='1'
            print('Native wildlife:',name,flush=True)
            with stem.with_suffix('.log').open('w') as f:subprocess.run(cmd,cwd=ROOT,env=run_env,stdout=f,stderr=subprocess.STDOUT,check=True)
            cap=json.loads(Path(str(stem.with_suffix('.bmp'))+'.json').read_text())
            assert cap['capture_ok'] and not cap['offscreen'] and 'AMD Radeon RX 5300' in cap['renderer'],cap
            Image.open(stem.with_suffix('.bmp')).save(stem.with_suffix('.png'))
            report=Path(str(stem.with_suffix('.json') if not a.captures and not a.return_check else stem.with_suffix('.bmp'))+'.world.json')
            w=json.loads(report.read_text());animals=w['wildlife']
            assert w['gl_error']==w['failed_tiles']==w['remaining_texture_entries']==w['remaining_texture_references']==0,w
            assert animals['load_failures']==animals['unsafe_roots']==0,animals
            assert w['authored_instances']==5289 and w['authored_source_files']==4
            if name=='return':assert animals['habitat']==-1 and animals['pose_assets']==0 and animals['loads']==animals['unloads']==1 and animals['peak_pose_assets']==48,animals
            elif name=='market':assert animals['habitat']==-1 and animals['pose_assets']==animals['loads']==animals['actors']==0,animals
            else:
                expect={'farm':(0,4,48),'woodland':(1,3,36),'rainforest':(2,1,12)}[name]
                assert (animals['habitat'],animals['species'],animals['pose_assets'])==expect and animals['actors']>0 and animals['peak_visible']>0,animals
            if not a.return_check:assert w['travel_loading_frames']==0,w['travel_loading_frames']
            row={'scene':name,'wildlife':animals,'gl_error':w['gl_error'],'failed_tiles':w['failed_tiles'],'travel_loading_frames':w['travel_loading_frames'],'remaining_texture_entries':w['remaining_texture_entries']}
            if not a.captures and not a.return_check:
                d=json.loads(stem.with_suffix('.json').read_text());assert d['samples']==frames-120 and d['dropped_samples']==0
                row.update({k:d[k] for k in ('mean_fps','one_percent_low_fps','p99_ms','max_ms','within_45fps_budget_percent','mean_render_total_gpu_ms','mean_raw_update_ms')})
                row['strict_45fps_pass']=d['mean_fps']>=45 and d['one_percent_low_fps']>=45
            assert freeze()==frozen,'Runtime source/assets changed during profiling'
            results.append(row);print(json.dumps(row),flush=True)
    finally:
        stop.set();t.join();(out/'gpu-memory.json').write_text(json.dumps(memory,indent=2)+'\n')
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
