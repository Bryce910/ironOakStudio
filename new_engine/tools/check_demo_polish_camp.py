#!/usr/bin/env python3
"""Native camp acceptance at saved quality; retains every post-warmup interval.

Run with --native --output docs/performance/demo-polish/<fresh directory>.
The two-minute collision/grounding/separation audit runs before measured play.
Detailed GPU queries are omitted; no graphics setting or effect is changed.
"""
import argparse
import json
import math
from pathlib import Path
import subprocess
import traceback

import benchmark_graphics_settings as common
import check_demo_finish as finish
import check_cinematic_particles as particles


def main():
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--native',action='store_true',required=True)
    cli.add_argument('--output',type=Path,required=True)
    args=cli.parse_args()
    out=args.output.resolve()
    assert out.is_relative_to(common.ROOT/'docs/performance')
    out.mkdir(parents=True,exist_ok=False)
    with finish.idle_lock():
        static,_=finish.static_check()
        before=finish.metadata();frozen=finish.fingerprints()
        tool_sha=common.digest(Path(__file__))
        prefs=common.preference_state()
        assert finish.metadata()==before
    points=static['polish']['details']['camp']['approach']
    path=points+points[-2::-1]
    length=sum(math.dist(a,b)for a,b in zip(path,path[1:]))
    walking_frames=math.ceil(length/3.5*60)
    route_frames=math.ceil((walking_frames+240)/120)*120
    # Audio observes frame-end displacement. A frame which crosses the return
    # corner measures a chord, while route telemetry measures both segments.
    position=path[0];next_point=1;audio_distance=0.;integrated_frames=0
    while next_point<len(path):
        previous=position;remaining=3.5/60;integrated_frames+=1
        while remaining>1e-8 and next_point<len(path):
            target=path[next_point];distance=math.dist(position,target)
            if distance<1e-6:next_point+=1;continue
            step=min(remaining,distance)
            position=tuple(a+(b-a)/distance*step for a,b in zip(position,target));remaining-=step
            if math.dist(position,target)<1e-6:next_point+=1
        audio_distance+=math.dist(previous,position)
    assert integrated_frames==walking_frames
    (out/'source-freeze.json').write_text(json.dumps(dict(sha256=frozen,tool_sha256=tool_sha,preferences=prefs),indent=2)+'\n')
    rows=[]
    for name,frames,view in [('clearing',1800,'sweep'),('approach-return',route_frames,'river')]:
        stem=out/name;env,_=common.native_environment()
        env['VALWOOD_QA_FRAME_TIMES_ONLY']='1'
        env['VALWOOD_QA_VALIDATE_TRAIL_CAMP']='1'
        env['VALWOOD_VILLAGE_LIGHTING']='afternoon'
        cmd=[str(common.LAUNCHER),'--scenic','rainforest-camp','--benchmark','--view',view,
             '--width','1600','--height','900','--frames',str(frames),'--warmup','120',
             '--no-hud','--profile',str(stem.with_suffix('.json')),'--capture',str(stem.with_suffix('.bmp'))]
        row=dict(name=name,frames=frames,command=cmd)
        print('Native camp acceptance:',name,frames,'frames',flush=True)
        try:
            with stem.with_suffix('.log').open('x')as log:
                p=subprocess.run(cmd,cwd=common.ROOT,env=env,stdout=log,stderr=subprocess.STDOUT)
            assert p.returncode==0,p.returncode
            row['timing']=finish.load_timing(stem,frames)
            row.update(particles.other_world_check(stem,frames,True,'rainforest-camp'))
            w=json.loads(Path(row['world_report']).read_text())
            row['route']=w['benchmark_route']
            assert w['authored_instances']==static['combined_records'] and w['authored_source_files']==9
            assert {Path(s['path']).name:s['instances']for s in w['authored_sources']}==static['sources']
            c=w['trail_camp'];a=w['wildlife']
            assert c['active'] and (c['actors'],c['walking'],c['seated'],c['pose_assets'])==(4,1,2,20)
            assert (a['actors'],a['walking'],a['idling'],a['pose_assets'])==(4,2,2,24)
            assert c['unsafe_roots']==c['failures']==a['unsafe_roots']==a['load_failures']==0
            assert w['trail_camp_audit_samples']==3600
            expected_path_samples=3*sum(math.ceil(math.dist(a,b)/.15)+1 for a,b in zip(points,points[1:]))
            assert w['trail_camp_path_audit_samples']==expected_path_samples
            assert a['loads']==1 and a['unloads']==w['travel_loading_frames']==0
            row['particles']=particles.check_particles(w,frames)
            row['lighting']=particles.check_lighting(w,frames,False)
            row['atmosphere']=finish.check_rainforest_atmosphere(w,'rainforest-camp')
            row['probes']=finish.check_probes(w,frames,True)
            row['material_preparation']=finish.check_main_preparation(w,'rainforest-camp')
            row['texture_blocks']=finish.check_texture_blocks(w)
            row['texture_source_storage']=finish.check_source_storage(w)
            assert not w['gpu_stage_timing_available']
            assert w['adaptive_gpu_timing']['begun_frames']==0
            assert w['presentation']['swap_interval']==1 and w['presentation']['vsync_unchanged']
            audio=w['demo_audio'];r=w['benchmark_route']
            assert audio['ready'] and audio['updates']==frames and audio['update_failures']==0 and audio['fire_sources']==35
            if view=='river':
                assert r['active'] and r['complete'] and r['id']=='old_tanthoon_wayfarers_clearing_return'
                assert r['waypoints_reached']==len(path)-1 and abs(r['length_m']-length)<1e-6
                assert r['waiting_frames']==120 and r['walking_frames']==walking_frames
                assert r['collision_blocked_steps']==r['slope_blocked_steps']==r['sliding_steps']==0
                assert abs(r['actual_distance_m']-length)<.01 and r['return_error_m']<.001
                assert abs(audio['walked_m']-audio_distance)<1e-5
                assert abs(audio['steps']-math.floor(audio['walked_m']/1.2))<=1
            else:
                assert not r['active']
                row['audio']=finish.check_audio(w,frames,'camp',True)
            row.update(correctness_pass=True,trail_camp=c,wildlife=a,route=r)
        except (AssertionError,OSError,ValueError,KeyError,TypeError)as error:
            row.update(correctness_pass=False,error=type(error).__name__+' at line '+str(traceback.extract_tb(error.__traceback__)[-1].lineno)+': '+str(error))
        rows.append(row)
        print(json.dumps(dict(name=name,correctness_pass=row['correctness_pass'],error=row.get('error'),timing=row.get('timing'))),flush=True)
        assert finish.metadata()==before and common.preference_state()==prefs
    with finish.idle_lock():assert finish.fingerprints()==frozen
    assert common.digest(Path(__file__))==tool_sha and common.preference_state()==prefs
    correct=all(r['correctness_pass']for r in rows)
    gate=correct and all(r['timing']['strict_45fps_pass']for r in rows)
    all_frames=correct and all(r['timing']['all_frames_within_45fps_budget']for r in rows)
    result=dict(correctness_pass=correct,strict_45fps_pass=gate,all_frames_within_45fps_budget=all_frames,
                preferences_preserved=True,external_resource_monitor=False,gpu_stage_timing_available=False,
                timing_scope='Every interval after exactly120 warmup frames, including hitches; fixed saved quality, VSync unchanged.',
                route_length_m=length,runs=rows)
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    return 0 if correct and gate else 1


if __name__=='__main__':raise SystemExit(main())
