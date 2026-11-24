#!/usr/bin/env python3
"""Sequential visible native castle-village captures, full walking route and GPU profiles."""
import argparse,json,os,subprocess,math,re
from pathlib import Path
from PIL import Image
from benchmark_cinematic_scenery import expected_scenery,verify_integrated_lighting
from check_castle_demo import EXPECTED_POPULATION,EXPECTED_PETS,generated_route
ROOT=Path(__file__).resolve().parents[1]
def castle_route_frames():
    route=generated_route(ROOT,'castle_expansion_route')
    return math.ceil(sum(math.dist(a,b) for a,b in zip(route,route[1:]))/3.5*60)+300
RUNS={'watch-close':(1800,'castle-watch-close','sweep'),'watch':(1800,'castle-watch','sweep'),'farm-tools':(1800,'castle-farm-tools','sweep'),'farms':(1800,'castle-farms','sweep'),'farms-reverse':(1800,'castle-farms-reverse','sweep'),
      'farm-route':(3318,'castle-farms','river'),'route':(14160,'castle-village','river'),'demo-route':(17640,'castle-demo','river'),'approach':(1800,'castle-village','sweep'),
      'market':(1800,'castle-market','sweep'),'stalls':(1800,'castle-stalls','sweep'),
      'alley':(1800,'castle-alley','sweep'),'courtyard':(1800,'castle-courtyard','sweep'),
      'housing':(1800,'castle-housing','sweep'),'marketrow':(1800,'castle-marketrow','sweep'),
      'forecourt':(1800,'castle-forecourt','sweep'),'hall':(1800,'castle-hall','sweep'),
      'hero-entry':(1800,'castle-hero-entry','sweep'),'hero-mid':(1800,'castle-hero-mid','sweep'),
      'hero-reverse':(1800,'castle-hero-reverse','sweep'),'hero-route':(1809,'castle-hero-entry','river'),
      'training':(1800,'castle-training','sweep'),'statue':(1800,'castle-statue','sweep'),
      'armory':(1800,'castle-armory','sweep'),'drawbridge':(1800,'castle-drawbridge','sweep'),
      'creek':(1800,'castle-creek','sweep'),'castle-route':(castle_route_frames(),'castle-drawbridge','river')}
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--scene',choices=['all',*RUNS],default='all')
    p.add_argument('--captures',action='store_true');p.add_argument('--frames',type=int)
    p.add_argument('--output',type=Path,default=ROOT/'docs/performance/castle-village')
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    if a.frames is not None and a.frames<=(0 if a.captures else 120):p.error('frames must exceed the warm-up count')
    env=os.environ.copy();env['SDL_AUDIODRIVER']='dummy'
    for name,(default,scene,view) in RUNS.items():
        if a.scene not in ['all',name] or (a.captures and name in ('route','demo-route','hero-route','castle-route','farm-route')):continue
        suffix='view' if a.captures else 'final';stem=out/f'{name}-{suffix}'
        frames=a.frames or (300 if a.captures else default)
        if a.captures:
            cmd=[str(ROOT.parent/'Launch_Valwood_World.sh'),'--scenic',scene,'--width','1600','--height','900',
                 '--frames',str(frames),'--no-hud','--capture',str(stem.with_suffix('.bmp'))]
        else:
            cmd=[str(ROOT/'tools/benchmark_valwood_world.sh'),'--scenic',scene,'--view',view,'--frames',str(frames),
                 '--profile',str(stem.with_suffix('.json')),'--capture',str(stem.with_suffix('.bmp'))]
        print('Native castle village:',name,suffix,flush=True)
        with stem.with_suffix('.log').open('w') as f:subprocess.run(cmd,cwd=ROOT,env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
        capture=json.loads(Path(str(stem.with_suffix('.bmp'))+'.json').read_text())
        assert capture['capture_ok'] and not capture['offscreen'] and capture['width']==1600 and capture['height']==900
        assert 'AMD Radeon RX 5300' in capture['renderer'],capture['renderer']
        Image.open(stem.with_suffix('.bmp')).save(stem.with_suffix('.png'))
        if a.captures:
            manifest=json.loads((ROOT/'projects/valwood_world/scenery/castle_village_manifest.json').read_text())
            views=[v for v in manifest.get('demo_views',[]) if v['id']==scene]
            if views:
                view=views[0];x,z=view['position_xz'];tx,tz=view['target_xz']
                expected=[manifest['origin_world_xz'][0]+x,manifest['origin_world_xz'][1]+z,math.atan2(tx-x,-(tz-z))]
                match=re.search(r'\[World\] End position ([-0-9.]+) [-0-9.]+ ([-0-9.]+) yaw ([-0-9.]+)',stem.with_suffix('.log').read_text())
                assert match and all(abs(float(v)-e)<.002 for v,e in zip(match.groups(),expected)),('Capture camera moved from authored view',scene)
            continue
        d=json.loads(stem.with_suffix('.json').read_text());w=json.loads(Path(str(stem.with_suffix('.json'))+'.world.json').read_text())
        assert d['samples']==frames-120 and d['dropped_samples']==0
        assert w['gl_error']==0 and w['failed_tiles']==0 and w['travel_loading_frames']==0
        assert w['remaining_texture_entries']==0 and w['remaining_texture_references']==0
        assert w['shadow_map_side']>=3072 and w['soft_shadow_taps']==9
        assert w['authored_source_files']==4 and w['authored_instances']==expected_scenery()[0]
        assert w['authored_sources'][3]['instances']>=300 and w['authored_sources'][3]['colliders']>100
        population=w['village_population']
        assert all(population[key]==value for key,value in EXPECTED_POPULATION.items())
        assert population['walking']>0 and population['idling']>0 and population['peak_visible']>0
        assert population['updates']>=frames and population['pose_assets']==76
        watch=w['village_watch']
        assert watch['actors']==4 and watch['pose_assets']==8 and watch['blend_batches']==8
        assert watch['gpu_pose_blending'] and watch['rooted_stations'] and watch['updates']>=frames
        if name in ('watch','watch-close','farm-tools'):assert watch['peak_visible']>0
        training=w['castle_training']
        assert training['actors']==7 and training['trainees']==6 and training['instructors']==1
        assert training['pose_assets']==28 and training['blend_batches']==28 and training['gpu_pose_blending']
        assert training['updates']>=frames
        if name in ('training','castle-route'):assert training['peak_visible']>0
        if name in ('drawbridge','creek','castle-route'):
            creek=w['castle_creek']
            assert creek['draw_frames']>0 and creek['reflection_frames']>0
            assert creek['water_surface_y']==22.9 and creek['bridge_deck_y']==24.02
            assert creek['outside_outer_town_walls'] and creek['explicit_surface_mask'] and creek['polyline_points']==22
            assert w['water_scene_frames']>0
        animation=w['village_animation']
        assert animation['gpu_pose_blending'] and animation['pose_batch_capacity']==91 and animation['pose_contract_version']==1
        pets=w['village_pets']
        assert all(pets[key]==value for key,value in EXPECTED_PETS.items())
        assert pets['pose_assets']==32 and pets['pose_batch_capacity']==38 and pets['gpu_pose_blending']
        assert pets['walking']>0 and pets['idling']>0 and pets['updates']>=frames
        # The exterior flank is beyond the pets' retained town routes and
        #220m visibility limit. Their full simulation/inventory is checked above.
        if name not in ('hall','creek','farms','farms-reverse','farm-route','farm-tools'):assert pets['peak_visible']>0
        verify_integrated_lighting(w,frames)
        route=w['benchmark_route']
        if name in ('route','demo-route','hero-route','castle-route','farm-route'):
            route_id = 'valdoa_castle_farms_return' if name == 'farm-route' else 'valdoa_castle_training_return' if name == 'castle-route' else 'valdoa_market_hero_return' if name == 'hero-route' else 'valdoa_castle_district_demo_return' if name == 'demo-route' else 'valdoa_castle_village_return'
            assert route['active'] and route['id']==route_id
            assert route['collision_blocked_steps']==route['slope_blocked_steps']==route['sliding_steps']==0,route
            if frames>=default:assert route['complete'] and route['return_error_m']<.001,route
        print(json.dumps({'scene':name,**{k:d[k] for k in ['mean_fps','one_percent_low_fps','p99_ms','max_ms','within_45fps_budget_percent','mean_render_total_gpu_ms']}}),flush=True)
if __name__=='__main__':main()
