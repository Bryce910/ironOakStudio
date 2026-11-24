#!/usr/bin/env python3
"""Sequential native rainforest captures, pocket sweeps and actual return walk."""
import argparse
import json
import os
from pathlib import Path
import subprocess
from benchmark_cinematic_scenery import expected_scenery,verify_integrated_lighting
from benchmark_starting_area import verify_capture

ROOT=Path(__file__).resolve().parents[1]
RUNS={'path':(7680,'rainforest','river'),'rainforest':(1800,'rainforest','sweep'),
      'rainforest-deep':(1800,'rainforest-deep','sweep'),
      'rainforest-pool':(1800,'rainforest-pool','sweep')}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scene',choices=['all',*RUNS],default='all')
    parser.add_argument('--captures',action='store_true')
    parser.add_argument('--verify-existing',action='store_true',help='Validate saved reports without launching or rerunning the renderer')
    parser.add_argument('--frames',type=int)
    parser.add_argument('--output',type=Path,default=ROOT/'docs/performance/rainforest')
    args=parser.parse_args()
    if args.frames is not None and args.frames<=(0 if args.captures else 120):parser.error('Invalid frame duration')
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    environment=os.environ.copy();environment['SDL_AUDIODRIVER']='dummy'
    for name,(duration,scene,view) in RUNS.items():
        if args.scene not in ['all',name] or args.captures and name=='path':continue
        frames=args.frames or (300 if args.captures else duration)
        suffix='view' if args.captures else 'final'
        capture=out/f'{name}-{suffix}.bmp';profile=out/f'{name}-{suffix}.json'
        if args.captures:
            command=[str(ROOT.parent/'Launch_Valwood_World.sh'),'--scenic',scene,'--no-hud','--width','1600','--height','900','--frames',str(frames),'--capture',str(capture)]
        else:
            command=[str(ROOT/'tools/benchmark_valwood_world.sh'),'--scenic',scene,'--view',view,'--frames',str(frames),'--profile',str(profile),'--capture',str(capture)]
        print('Native rainforest '+suffix+': '+name,flush=True)
        if not args.verify_existing:
            with (out/f'{name}-{suffix}.log').open('w') as log:subprocess.run(command,cwd=ROOT,env=environment,stdout=log,stderr=subprocess.STDOUT,check=True)
        verify_capture(capture)
        if args.captures:continue
        data=json.loads(profile.read_text());world=json.loads(Path(str(profile)+'.world.json').read_text())
        assert data['renderer'].startswith('AMD Radeon RX 5300') and data['samples']==frames-120 and data['dropped_samples']==0
        assert world['failed_tiles']==world['gl_error']==world['travel_loading_frames']==0
        assert world['remaining_texture_entries']==world['remaining_texture_references']==0
        verify_integrated_lighting(world,frames)
        total,trees,props=expected_scenery()
        assert world['authored_instances']==total and world['authored_tree_instances']==trees and world['authored_prop_instances']==props
        assert world['authored_source_files']==4 and world['authored_largest_batch']<=768 and world['scenery_color_culling']
        assert world['rainforest_weight']>.99 and not world['camera_final']['fly']
        assert world['rainforest_weather'] and world['weather_failures']==0
        assert world['weather_resolves']==frames and world['weather_resizes']==1
        assert world['weather_width']==1600 and world['weather_height']==900
        assert world['weather_half_width']==800 and world['weather_half_height']==450
        assert world['weather_fog_steps']==8 and world['weather_rows_per_update_cap']==8
        assert 0<world['weather_mib']<14 and world['weather_terrain_cached']
        assert world['weather_height_queries']<=(frames+9)*8*65
        assert world['shadow_mesh_lod'] and world['procedural_shadow_proxy_groups']==4
        # Six vegetation proxies plus active original/purchased architecture.
        # Replacing a prototype must remove its unused shadow resource too.
        original={'village_house_a','village_house_b','village_keep','village_inn','village_smithy',
                  'village_stall_red','village_stall_blue','village_stall_gold'}
        active=set()
        for source in (ROOT/'projects/valwood_world/scenery').glob('*instances.txt'):
            active.update(line.split()[0] for line in source.read_text().splitlines() if line.strip() and not line.lstrip().startswith('#'))
        purchased={alias for alias in active if alias.startswith('purchased_')}
        assert world['authored_shadow_proxy_groups']==6+len(active&original)+len(purchased) and world['procedural_shadow_proxies_synced']
        assert world['shadow_map_side']==2048 and world['soft_shadow_taps']==9
        assert world['rain_instances']<=4096 and world['splash_instances']<=768
        if name=='rainforest-pool':
            assert world['pool_draw_frames']>0 and world['pool_reflection_frames']>0
            assert world['pool_surface_y']==134 and world['water_scene_frames']>0

        route=world['benchmark_route']
        if name=='path':
            assert route['active'] and route['id']=='old_tanthoon_rainforest_return'
            assert route['collision_blocked_steps']==route['slope_blocked_steps']==route['sliding_steps']==0,route
            if frames>=7680:assert route['complete'] and route['return_error_m']<.001 and abs(route['actual_distance_m']-route['length_m'])<.01,route
        print(json.dumps({'scene':name,**{key:data[key] for key in ['mean_fps','one_percent_low_fps','p99_ms','max_ms','within_45fps_budget_percent','mean_render_total_gpu_ms']}}),flush=True)

if __name__=='__main__':main()
