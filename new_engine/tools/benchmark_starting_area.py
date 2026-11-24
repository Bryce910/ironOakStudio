#!/usr/bin/env python3
"""Capture and profile the dressed starting area on the native graphics driver.

Close the live engine first. GPU jobs run sequentially; measured timings never
accept software/offscreen renderers. Route uses actual ordinary-speed walking.
"""
import argparse,json,os,subprocess
from pathlib import Path
from PIL import Image
from benchmark_cinematic_scenery import expected_scenery,verify_integrated_lighting
ROOT=Path(__file__).resolve().parents[1]
VIEWS=['start','courtyard','workshop','overlook','grove']
RUNS={'route':(6240,'start','sweep'),**{v:(1800,v,'arrival' if v=='start' else 'sweep') for v in VIEWS}}
def read(path):return json.loads(path.read_text())
def verify_capture(capture):
    d=read(Path(str(capture)+'.json'))
    assert d['capture_ok'] and not d['offscreen'] and d['width']==1600 and d['height']==900
    assert d['renderer'].startswith('AMD Radeon RX 5300'),d['renderer']
    Image.open(capture).save(capture.with_suffix('.png'))
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--scene',choices=['all',*RUNS],default='all');p.add_argument('--captures',action='store_true');p.add_argument('--frames',type=int);p.add_argument('--output',type=Path,default=ROOT/'docs/performance/starting-area');a=p.parse_args()
    if a.frames is not None and a.frames<=(0 if a.captures else 120):p.error('--frames must be positive for captures and exceed120 warm-up frames for profiles')
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);env=os.environ.copy();env['SDL_AUDIODRIVER']='dummy'
    runs={v:(300,v,'arrival') for v in VIEWS} if a.captures else RUNS
    for name,(frames,scenic,view) in runs.items():
        if a.scene not in ['all',name]:continue
        suffix='view' if a.captures else 'final';capture=out/f'{name}-{suffix}.bmp';profile=out/f'{name}-{suffix}.json'
        if a.captures:
            cmd=[str(ROOT.parent/'Launch_Valwood_World.sh'),'--scenic',scenic,'--frames',str(a.frames or frames),'--width','1600','--height','900','--no-hud','--capture',str(capture)]
        else:
            assert (a.frames or frames)>120
            cmd=[str(ROOT/'tools/benchmark_valwood_world.sh'),'--scenic',scenic,'--view',view,'--frames',str(a.frames or frames),'--profile',str(profile),'--capture',str(capture)]
        print('Native starting-area '+suffix+': '+name,flush=True)
        with (out/f'{name}-{suffix}.log').open('w') as log:subprocess.run(cmd,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
        verify_capture(capture)
        if not a.captures:
            d=read(profile);w=read(Path(str(profile)+'.world.json'));count,trees,props=expected_scenery()
            assert d['renderer'].startswith('AMD Radeon RX 5300')
            assert d['frames']==(a.frames or frames) and d['samples']==d['frames']-120 and d['dropped_samples']==0
            assert w['failed_tiles']==0 and w['gl_error']==0 and w['travel_loading_frames']==0
            verify_integrated_lighting(w,d['frames'])
            assert w['authored_instances']==count and w['authored_tree_instances']==trees and w['authored_prop_instances']==props
            assert w['authored_largest_batch']<=768 and w['temporal_aa'] and w['hemisphere_lighting']
            assert w['remaining_texture_entries']==0 and w['remaining_texture_references']==0
            assert w['authored_source_files']==4 and w['authored_sources'][0]['instances']==283
            route=w['benchmark_route']
            if name=='route':
                assert route['active'] and route['collision_blocked_steps']==0 and route['slope_blocked_steps']==0 and route['sliding_steps']==0,route
                if not a.frames or a.frames>=6240:
                    assert route['complete'] and route['return_error_m']<.001 and abs(route['actual_distance_m']-route['length_m'])<.01,route
            print(json.dumps({'scene':name,**{k:d[k] for k in ['mean_fps','one_percent_low_fps','p99_ms','max_ms','within_45fps_budget_percent','mean_render_total_gpu_ms']}}),flush=True)
if __name__=='__main__':main()
