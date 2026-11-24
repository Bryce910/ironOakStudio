#!/usr/bin/env python3
"""Sequential native scenery checks. Close a live engine before running.

Runs visible1600x900 moving-camera scenes using the existing single-session
benchmark launcher. No offscreen/software-renderer result is accepted here.
"""
from pathlib import Path
import argparse
import json
import math
import os
import subprocess
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
RUNS={
    'forest': (3600,['--scenic','forest']),
    'lake': (3600,['--scenic','lake']),
    'ridge': (1800,['--scenic','ridge']),
    'coast': (1800,['--scenic','coast']),
    'shore': (1800,['--scenic','shore']),
    'rainforest': (1800,['--scenic','rainforest']),
    'rainforest-deep': (1800,['--scenic','rainforest-deep']),
    'tour': (5400,['--landmark','brandalhall','--tour']),
}

def expected_scenery():
    aliases=[]
    for filename in ['authored_instances.txt','starting_area_instances.txt','rainforest_instances.txt','castle_village_instances.txt']:
        path=ROOT/'projects/valwood_world/scenery'/filename
        aliases.extend(line.split()[0] for line in path.read_text().splitlines() if line.strip() and not line.lstrip().startswith('#'))
    trees=sum(alias in ['pine','fir','broadleaf','pine_young'] for alias in aliases)
    return len(aliases),trees,len(aliases)-trees

def verify_integrated_lighting(world,frames,*,allow_loading=False,width=1600,height=900):
    """Check profiled renderer integration; capture-only reports lack GPU times.

    Fixed-size benchmarks must allocate each target once. A tour intentionally
    reuses its previous image while terrain streams, so only its rendered
    frames resolve contact lighting. Memory checks derive from target formats,
    not timing thresholds or one machine's observed frame cost.
    """
    if 'texture_cache_unmanaged_uploads' in world:
        assert world['texture_cache_unmanaged_uploads']==0,'Texture cache overflow bypassed image sharing'
        assert 0<world['texture_cache_source_mib']<=world['texture_cache_source_budget_mib'],'Invalid texture-cache residency'
    assert world['volumetric_clouds'] is True,'Volumetric sky disabled'
    assert world['contact_lighting'] is True,'Contact lighting disabled'
    assert world['occlusion_failures']==0,'Contact resolve failed'
    held=world['travel_loading_frames']
    assert isinstance(held,int) and 0<=held<frames,'Invalid held-frame count'
    if not allow_loading:assert held==0,'Settled view held an old image'
    rendered=frames-held
    assert world['occlusion_frames']==rendered,'Contact lighting did not resolve every rendered frame'
    assert world['sky_target_resizes']==0,'Fixed-size sky targets resized after allocation'
    assert world['occlusion_resizes']==1,'Contact targets were not allocated exactly once'
    assert isinstance(world['sky_view_steps'],int) and 0<world['sky_view_steps']<=12,'Unbounded sky view-step budget'
    calls=world['sky_draw_calls']
    assert isinstance(calls,int) and calls%2==0 and 2*rendered<=calls<=4*rendered,'Missing or repeated sky draws'
    reflection_width,reflection_height=world['reflection_width'],world['reflection_height']
    assert isinstance(reflection_width,int) and isinstance(reflection_height,int)
    assert 0<=reflection_width<=width and 0<=reflection_height<=height
    assert (reflection_width==0)==(reflection_height==0),'Incomplete reflection target dimensions'
    half_pixels=((width+1)//2)*((height+1)//2)
    reflection_pixels=((reflection_width+1)//2)*((reflection_height+1)//2)
    expected={
        'sky_target_mib':8*(half_pixels+reflection_pixels)/1048576,
        'sky_noise_mib':4*sum(side**3 for side in [64,32,16,8,4,2,1])/1048576,
        'indirect_target_mib':8*width*height/1048576,
        'occlusion_mib':8*(width*height+2*half_pixels)/1048576,
    }
    for field,value in expected.items():
        actual=world[field]
        assert math.isfinite(actual) and actual>0 and math.isclose(actual,value,rel_tol=0,abs_tol=.00001),(field,actual,value)
    for field in ['mean_sky_gpu_ms','mean_contact_gpu_ms']:
        assert math.isfinite(world[field]) and world[field]>0,(field,'Missing measured stage time')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scene',choices=['all',*RUNS],default='all')
    parser.add_argument('--frames',type=int,help='Override duration for a diagnostic run')
    parser.add_argument('--expect-lighting',action='store_true',help='Require shared hemisphere lighting and opaque-depth shoreline water')
    parser.add_argument('--expect-authored',action='store_true',help='Require authored districts, scenery and active temporal AA')
    parser.add_argument('--expect-integrated',action='store_true',help='Require profiled volumetric sky/contact lighting, stable target allocations and measured stage times (older reports otherwise remain compatible)')
    parser.add_argument('--output',type=Path,default=ROOT/'docs/performance/cinematic-scenery')
    args=parser.parse_args()
    if args.frames is not None and args.frames<=120:parser.error('--frames must exceed120 warm-up frames')
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    environment=os.environ.copy();environment['SDL_AUDIODRIVER']='dummy'
    for name,(default_frames,options) in RUNS.items():
        if args.scene not in ['all',name]:continue
        frames=args.frames or default_frames
        profile=output/(name+'-final.json');capture=output/(name+'-final.bmp')
        print('Native scenery check:',name,frames,'frames',flush=True)
        command=[str(ROOT/'tools/benchmark_valwood_world.sh'),*options,'--frames',str(frames),
                 '--warmup','120','--profile',str(profile),'--capture',str(capture)]
        with (output/(name+'-final.log')).open('w') as log:
            subprocess.run(command,cwd=ROOT,env=environment,stdout=log,stderr=subprocess.STDOUT,check=True)
        data=json.loads(profile.read_text());world=json.loads(Path(str(profile)+'.world.json').read_text())
        assert 'llvmpipe' not in data['renderer'].lower() and 'softpipe' not in data['renderer'].lower()
        assert data['frames']==frames and data['samples']==frames-120 and data['dropped_samples']==0
        assert world['failed_tiles']==0 and world['gl_error']==0
        assert world['remaining_texture_entries']==0 and world['remaining_texture_references']==0
        # New reports always validate their integration. The explicit option
        # also rejects an older report that has none of the new fields.
        if args.expect_integrated or 'volumetric_clouds' in world or 'contact_lighting' in world:
            verify_integrated_lighting(world,frames,allow_loading=name=='tour')
        if args.expect_authored:
            assert world['temporal_aa'] and world['animated_foliage']
            instances,trees,props=expected_scenery()
            assert world['authored_districts']==3 and world['authored_instances']==instances
            assert world['authored_tree_instances']==trees and world['authored_prop_instances']==props
            assert world['authored_largest_batch']<=768
        if args.expect_lighting:
            assert world['hemisphere_lighting'] and world['shoreline_depth']
            assert world['water_scene_peak_mib']<=16.48
            if name in ['lake','coast','shore']:assert world['water_scene_frames']>0
        Image.open(capture).save(capture.with_suffix('.png'))
        print(json.dumps({'scene':name,**{k:data[k] for k in ['mean_fps','one_percent_low_fps','p99_ms','max_ms',
            'within_45fps_budget_percent','mean_render_total_gpu_ms','mean_reflection_gpu_ms']},
            'loading_frames':world['travel_loading_frames']}),flush=True)

if __name__=='__main__':main()
