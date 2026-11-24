#!/usr/bin/env python3
from pathlib import Path
import json,math,re,subprocess,sys
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tools'))
import benchmark_graphics_settings as common
import check_demo_finish as finish
import check_cinematic_particles as particles
import argparse
cli=argparse.ArgumentParser(description='Run the complete native jungle trail with unchanged saved quality. Every post-warmup frame must stay within22.222ms; no interval filtering.')
cli.add_argument('--native',action='store_true',required=True)
cli.add_argument('--output',type=Path,required=True)
cli.add_argument('--ui-trace',action='store_true',help='Collect fixed-capacity UI wall/thread-CPU diagnostics; writes once after rendering')
cli.add_argument('--frame-times-only',action='store_true',help='Keep every frame interval; omit detailed GPU-stage query rings, matching ordinary manual-quality play')
args=cli.parse_args()
out=args.output.expanduser().resolve()
assert out.is_relative_to(root/'docs/performance'), 'Store evidence outside runtime inputs'
out.mkdir(parents=True,exist_ok=False)
with finish.idle_lock():
    static,_=finish.static_check();before=finish.metadata();frozen=finish.fingerprints()
    frozen['tools/check_rainforest_performance.py']=common.digest(Path(__file__))
    prefs=common.preference_state()
    assert finish.metadata()==before
(out/'source-freeze.json').write_text(json.dumps(dict(sha256=frozen,preferences=prefs),indent=2)+'\n')
source=(root/'engine/world_atlas.c').read_text().split('static const WorldAtlasTrailPoint rainforest_trail[]={',1)[1].split('};',1)[0]
points=[tuple(map(float,p))for p in re.findall(r'RAIN_TRAIL\(([-\d.]+),([-\d.]+)\)',source)]
assert len(points)==6
length=2*sum(math.dist(a,b)for a,b in zip(points,points[1:]))
frames=math.ceil((math.ceil(length/3.5*60)+240)/120)*120;stem=out/'rainforest-walk';env,_=common.native_environment()
if args.frame_times_only:env['VALWOOD_QA_FRAME_TIMES_ONLY']='1'
if args.ui_trace:env['VALWOOD_UI_TRACE_PATH']=str(out/'ui-trace.json')
cmd=[str(common.LAUNCHER),'--scenic','rainforest','--benchmark','--view','river','--width','1600','--height','900','--frames',str(frames),'--no-hud','--warmup','120','--profile',str(stem.with_suffix('.json')),'--capture',str(stem.with_suffix('.bmp'))]
print('Native complete jungle return walk',length,'m',flush=True)
monitor=common.MemoryMonitor()
monitor.start()
try:
    with (out/'engine.log').open('w')as log:p=subprocess.run(cmd,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT)
    assert p.returncode==0,p.returncode
    timing=finish.load_timing(stem,frames)
    checks=particles.other_world_check(stem,frames,True,'rainforest')
    w=json.loads(Path(checks['world_report']).read_text());route=w['benchmark_route']
    polish=json.loads((root/'projects/valwood_world/scenery/demo_polish_manifest.json').read_text())
    assert w['authored_instances']==7242+polish['record_count'] and w['authored_source_files']==9
    assert route['active']and route['complete']and route['id']=='old_tanthoon_rainforest_return'
    assert route['waypoints_reached']==10 and abs(route['length_m']-length)<1e-6
    assert route['collision_blocked_steps']==route['slope_blocked_steps']==route['sliding_steps']==w['travel_loading_frames']==0
    assert abs(route['actual_distance_m']-length)<.01 and route['return_error_m']<.001
    assert route['waiting_frames']==120 and route['walking_frames']==math.ceil(length/3.5*60)
    atmosphere=finish.check_rainforest_atmosphere(w,'rainforest')
    probes=finish.check_probes(w,frames,True);warm=finish.check_main_preparation(w,'rainforest')
    blocks=finish.check_texture_blocks(w)
    source_storage=finish.check_source_storage(w)
    # Reuse the existing distance-driven walking audio contract (minimum216m),
    # then additionally require the complete independent jungle-route distance.
    audio=finish.check_audio(w,frames,'arcade_route',True)
    # Audio measures frame-end displacement, whereas route distance adds every
    # segment traversed within a frame. Independently integrate those endpoints
    # so frames that cross a corner use the chord rather than the polyline arc.
    origin=(w['camera_final']['x']-points[0][0],w['camera_final']['z']-points[0][1])
    path=[(x+origin[0],z+origin[1])for x,z in points+points[-2::-1]]
    pos=path[0];n=1;chord=0.;sim_frames=0
    while n<len(path):
        previous=pos;remaining=3.5/60;sim_frames+=1
        while remaining>1.e-8 and n<len(path):
            dx,dz=path[n][0]-pos[0],path[n][1]-pos[1];distance=math.hypot(dx,dz)
            if distance<1.e-6:n+=1;continue
            step=min(remaining,distance);pos=(pos[0]+dx/distance*step,pos[1]+dz/distance*step);remaining-=step
            if math.dist(path[n],pos)<1.e-6:n+=1
        chord+=math.dist(previous,pos)
    assert sim_frames==route['walking_frames']
    assert abs(audio['walked_m']-chord)<1.e-5,(audio['walked_m'],chord)
    with finish.idle_lock():
        after=finish.fingerprints();after['tools/check_rainforest_performance.py']=common.digest(Path(__file__));assert after==frozen
    assert common.preference_state()==prefs
    assert w['adaptive_gpu_timing']['automatic_mode_only'] and w['adaptive_gpu_timing']['begun_frames']==0
    frame_only=env.get('VALWOOD_QA_FRAME_TIMES_ONLY')=='1'
    assert w['gpu_stage_timing_available']==(not frame_only)
    assert timing['gpu_stage_timing_available']==(not frame_only)
    assert timing['first_frame_monotonic_seconds']>0
    assert w['presentation']['swap_interval']==1 and w['presentation']['vsync_unchanged']
    optimization={name:w[name]for name in('pool_reflection_scissor','pool_reflection_clip','terrain_shadow_culling','horizon_selection_cache',
        'horizon_spatial_grid','zero_coverage_culling','wildlife_material_preparation',
        'zero_contribution_work_skip','vegetation_selection_work','shadow_fetch_grouping')}
    assert optimization['pool_reflection_scissor']['enabled'] and optimization['pool_reflection_scissor']['restricted_frames']>0
    assert optimization['pool_reflection_scissor']['geometry_pixels']<optimization['pool_reflection_scissor']['full_pixels']
    clip=optimization['pool_reflection_clip']
    assert clip['enabled'] and clip['tests']>0 and clip['proven_outside']==clip['skipped_passes']>=0
    assert all(clip[key] for key in ('pool_owner_only','opaque_snapshot_unchanged','loading_warm_unchanged','default_clip_planes'))
    assert optimization['terrain_shadow_culling']['rejected_tiles']>0
    grid=optimization['horizon_spatial_grid']
    assert grid['classifications']==optimization['horizon_selection_cache']['selections']>0
    assert 0<grid['retained_cells']<grid['cells'] and grid['candidate_roots']>0 and grid['fallbacks']==0
    assert grid['exact_point_predicate'] and grid['source_order_preserved'] and grid['cell_size_m']==512
    zero=optimization['zero_coverage_culling']
    assert zero['color_enabled'] and not zero['shadow_enabled'] and zero['guard_pixels']==2.99
    assert zero['existing_zero_coverage_endpoint_pixels']==3
    wildlife=optimization['wildlife_material_preparation']
    assert wildlife['enabled'] and wildlife['calls']==12
    assert wildlife['main_passes']==wildlife['reflection_passes']==wildlife['shadow_passes']==4
    assert wildlife['pose_draws']==288 and wildlife['failures']==wildlife['walking_fences']==0
    assert wildlife['loading_only'] and wildlife['main_color_attachments']==2 and wildlife['reflection_color_attachments']==1
    work=optimization['vegetation_selection_work']
    assert work['grass_classification_refreshes']>0 and work['grass_classified_seeds']>0
    assert work['grass_structural_revision']>0 and work['tree_dispatches']>0 and work['grass_dispatches']>0
    assert work['tree_fallbacks']==work['grass_fallbacks']==0 and work['density_ranges_caps_order_unchanged']
    assert all(optimization['zero_contribution_work_skip'].values())
    gather=optimization['shadow_fetch_grouping']
    assert gather['supported'] and gather['enabled']==('VALWOOD_QA_ENABLE_SHADOW_GATHER'in env and 'VALWOOD_QA_DISABLE_SHADOW_GATHER'not in env)
    assert gather['logical_taps']==9 and gather['scalar_border_fallback'] and gather['four_tap_unchanged']
    above45=timing['all_frames_within_45fps_budget']
    ui_trace=None
    if args.ui_trace:
        trace=json.loads((out/'ui-trace.json').read_text())
        assert trace['recorded_frames']==trace['total_frames']==frames and trace['dropped_frames']==0
        entries=[dict(zip(trace['columns'],r))for r in trace['frames']]
        fast=not (env.get('VALWOOD_QA_DISABLE_EMPTY_UI_FAST_PATH')=='1')
        assert trace['empty_fast_path_enabled']==fast
        assert all(r['thread_clock_available']for r in entries)
        for r in entries:
            if r['backend_skipped']:
                assert fast and r['passive'] and r['draw_lists']==r['vertices']==r['indices']==r['pending_textures']==0
        measured=entries[120:]
        assert all(r['passive'] and r['draw_lists']==r['vertices']==r['indices']==r['pending_textures']==0 for r in measured)
        assert all(r['backend_skipped']==int(fast) for r in measured)
        ui_trace=dict(enabled=True,empty_fast_path_enabled=fast,recorded_frames=frames,
            backend_skipped=sum(r['backend_skipped']for r in measured),
            backend_wall_mean_ms=sum(r['backend_wall_ms']for r in measured)/len(measured),
            backend_thread_cpu_mean_ms=sum(r['backend_thread_cpu_ms']for r in measured)/len(measured),
            note='Fixed-capacity diagnostics; no per-frame IO. Wall wait alone does not identify its cause.')
    result=dict(correctness_pass=True,strict_45fps_pass=timing['strict_45fps_pass'],all_frames_above_45fps=above45,preferences_preserved=True,optimization=optimization,ui_trace=ui_trace,
        gpu_stage_timing_available=not frame_only,adaptive_gpu_timing=w['adaptive_gpu_timing'],
        presentation=w['presentation'],
        timing=timing,route=route,audio_frame_endpoint_distance_m=chord,rainforest_atmosphere=atmosphere,local_probes=probes,
        texture_blocks=blocks,texture_source_storage=source_storage,main_material_preparation=warm,demo_audio=audio,command=cmd)
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(correctness_pass=True,strict_45fps_pass=timing['strict_45fps_pass'],all_frames_above_45fps=above45,mean_fps=timing['mean_fps'],one_percent_low_fps=timing['one_percent_low_fps'],max_ms=timing['max_ms'],over_budget=timing['frames_over_45fps_budget'],optimization=optimization)),flush=True)
    if not above45:raise SystemExit(1)
finally:
    monitor.stop();(out/'gpu-memory.json').write_text(json.dumps(monitor.samples,indent=2)+'\n')
