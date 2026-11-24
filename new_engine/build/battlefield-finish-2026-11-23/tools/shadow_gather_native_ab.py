#!/usr/bin/env python3
"""Sequential private scalar/gather comparison using genuine archived saves.

No engine, world, source checkpoint, or graphics-setting edits. Each run uses
normal Continue, then no movement/attack/camera inputs. Run only after the root
reviewer explicitly hands over the native UI and closes its prior session.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import importlib.util
import json
import os
import signal
import statistics
import subprocess
import sys
import time

N = Path(__file__).resolve().parents[1]
R = N / 'review/live-full'
sys.path.insert(0, str(N / 'tools'))
from private_review_resume import validate_resume_source
from benchmark_battlefield_native import process_identity


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def latest(path):
    if not path.is_file():
        return {}
    with path.open('rb') as stream:
        stream.seek(max(0, path.stat().st_size - 65536))
        rows = stream.read().splitlines()
    for raw in reversed(rows):
        try:
            return json.loads(raw)
        except (ValueError, UnicodeDecodeError):
            pass
    return {}


def run_checked(command, **kwargs):
    print('RUN', json.dumps([str(x) for x in command]), flush=True)
    return subprocess.run([str(x) for x in command], check=True, **kwargs)


def confirm_safe(row, stage, anchor=None):
    if row.get('screen') != 1 or row.get('loading') is not False or row.get('paused') is not False:
        raise RuntimeError('The private game is not normal unpaused gameplay.')
    if row.get('stage') != stage or row.get('enemy', -1) != -1 or row.get('health', 0) <= 0:
        raise RuntimeError('The real checkpoint is not a stationary safe benchmark view.')
    if anchor is not None:
        if max(abs(a-b) for a,b in zip(row['root'], anchor['root'])) > .02 or abs(row['yaw']-anchor['yaw']) > 1e-5:
            raise RuntimeError('Player/camera pose moved during the no-input benchmark.')
        if row.get('health') != anchor.get('health'):
            raise RuntimeError('Player health changed during the safe benchmark.')


def analyse_run(directory):
    benchmark = json.loads((directory/'benchmark/benchmark.json').read_text())
    if not benchmark['passed']:
        raise RuntimeError('The input-free frame collector rejected this interval.')
    first = benchmark['timing']['first_frame'] + 8
    last = benchmark['timing']['last_frame'] - 8
    fields = ('raw_update_ms','raw_render_cpu_ms','present_ms','render_total_gpu_ms',
              'forest_gpu_ms','shadow_gpu_ms','terrain_gpu_ms','reflection_gpu_ms','postfx_gpu_ms')
    with (directory/'profile/stationary.csv').open() as stream:
        rows = [row for row in csv.DictReader(stream) if first <= int(row['frame']) <= last]
    if len(rows) < 100:
        raise RuntimeError('Insufficient matching completed profile rows.')
    valid = [row for row in rows if float(row['render_total_gpu_ms']) > 0]
    if len(valid) < len(rows)*.9:
        raise RuntimeError('The native GPU query interval is incomplete.')
    summary = {key: {'mean_ms':statistics.mean(float(row[key]) for row in valid),
                     'median_ms':statistics.median(float(row[key]) for row in valid)} for key in fields}
    world = json.loads((directory/'profile/stationary.world.json').read_text())
    # The exact runtime shader-path readout is in the .world companion JSON.
    grouping = world.get('shadow_fetch_grouping')
    if grouping is None:
        primary = json.loads((directory/'profile/stationary').read_text())
        grouping = primary.get('shadow_fetch_grouping')
    summary.update(profile_rows=len(valid), trimmed_frame_range=[first,last],
                   shadow_fetch_grouping=grouping, benchmark=benchmark,
                   pose=json.loads((directory/'ab-pose.json').read_text()))
    (directory/'ab-analysis.json').write_text(json.dumps(summary,indent=2)+'\n')
    return summary


def execute(args):
    root = (N/'review').resolve()
    output = Path(args.output).resolve()
    if output == root or not output.is_relative_to(root) or output.exists():
        raise RuntimeError('--output must be a new private review child directory.')
    source = Path(args.resume_from).resolve()
    binary = N/'runtime/rpg'
    if sha(binary) != args.expected_binary:
        raise RuntimeError('The native binary changed; a new reviewed plan is required.')
    if R.exists():
        raise RuntimeError('live-full exists. Root must first close/archive its session explicitly.')
    provenance = validate_resume_source(source,root,binary,R)
    output.mkdir()
    source_settings = (source/'project/graphics.settings').read_bytes()
    baseline = {'binary_sha256':sha(binary),'resume':provenance,
                'graphics_sha256':sha(source/'project/graphics.settings'),
                'world_sha256':sha(N/'world-project/world.studio'),
                'shader_sha256':{str(p.relative_to(N)):sha(p) for p in (
                    N/'shared/shaders/world_foliage.fs.glsl',N/'shared/shaders/world_terrain.fs.glsl')},
                'warmup_seconds':args.warmup,'measure_seconds':args.seconds,
                'normal_continue_only':True,'movement_or_camera_inputs':False,
                'timing_source':'flushed quality_dt, matching native GPU CSV frame interval',
                'image_limitation':'Wind, NPCs and weather use real elapsed time; images are for visual review, not a pixel-identical GPU proof.'}
    scalar_reference=None
    if args.scalar_reference:
        reference=Path(args.scalar_reference).resolve()
        if not reference.is_relative_to(root):raise RuntimeError('Scalar reference must be private review evidence.')
        prior_plan=json.loads((reference.parent/'plan.json').read_text())
        for key in ('binary_sha256','graphics_sha256','world_sha256','shader_sha256','warmup_seconds','measure_seconds'):
            if prior_plan[key]!=baseline[key]:raise RuntimeError('Scalar reference differs in controlled '+key)
        if prior_plan['resume']['source_session']!=str(source):raise RuntimeError('Scalar reference used another checkpoint archive.')
        scalar_reference=json.loads((reference/'ab-analysis.json').read_text())
        preservation=json.loads((reference/'state-preservation.json').read_text())
        if not scalar_reference['benchmark']['passed'] or scalar_reference['shadow_fetch_grouping']['enabled'] or preservation!={'live_state_unchanged':True,'exit_code':0}:
            raise RuntimeError('Scalar reference is not a clean accepted native scalar interval.')
        baseline['scalar_reference']=str(reference)
    (output/'plan.json').write_text(json.dumps(baseline,indent=2)+'\n')
    results = {'scalar':scalar_reference} if scalar_reference else {}
    for variant in (['gather'] if scalar_reference else args.order.split(',')):
        if sha(binary) != args.expected_binary or sha(N/'world-project/world.studio') != baseline['world_sha256']:
            raise RuntimeError('Binary/world changed between controlled runs.')
        for name,digest in baseline['shader_sha256'].items():
            if sha(N/name) != digest:
                raise RuntimeError('Shader source changed between controlled runs.')
        if R.exists():
            raise RuntimeError('A private session path is already occupied.')
        env = os.environ.copy()
        env.pop('VALWOOD_QA_ENABLE_SHADOW_GATHER',None)
        env.pop('VALWOOD_QA_DISABLE_SHADOW_GATHER',None)
        env['VALWOOD_QA_ENABLE_SHADOW_GATHER' if variant=='gather' else 'VALWOOD_QA_DISABLE_SHADOW_GATHER']='1'
        command=[sys.executable,N/'tools/live_review.py','full','launch','--resume-from',source,
                 '--clean-performance','--profile',R/'profile/stationary','--shadow-fetch',variant]
        launcher_log=(output/(variant+'-launcher.log')).open('w')
        launcher=subprocess.Popen([str(x) for x in command],cwd=N,env=env,stdout=launcher_log,stderr=subprocess.STDOUT)
        pid = None
        identity = None
        error = None
        try:
            deadline=time.monotonic()+25
            while time.monotonic()<deadline and not (R/'pid').is_file():
                if launcher.poll() is not None:raise RuntimeError('Private launcher exited before creating its PID.')
                time.sleep(.1)
            pid=int((R/'pid').read_text())
            identity=process_identity(pid,(R/'project').resolve())
            launch=json.loads((R/'launch.json').read_text())
            flag='VALWOOD_QA_ENABLE_SHADOW_GATHER' if variant=='gather' else 'VALWOOD_QA_DISABLE_SHADOW_GATHER'
            if launch['environment'].get(flag)!='1':
                raise RuntimeError('Launcher stripped the requested shader-path flag; refuse inputs/measurement.')
            native_env=dict(item.split(b'=',1) for item in (Path('/proc')/str(pid)/'environ').read_bytes().split(b'\0') if b'=' in item)
            other='VALWOOD_QA_DISABLE_SHADOW_GATHER' if variant=='gather' else 'VALWOOD_QA_ENABLE_SHADOW_GATHER'
            if native_env.get(flag.encode())!=b'1' or other.encode() in native_env:
                raise RuntimeError('Actual native process environment did not select exactly the requested shader path.')
            # Copy an authentic receipt, never synthesize binary/source claims.
            receipt=N/'runtime-native-v1/BUILD_RECEIPT.json'
            if json.loads(receipt.read_text()).get('binary_sha256') != args.expected_binary:
                raise RuntimeError('Current compiled-source receipt does not match the actual binary.')
            (R/'BUILD_RECEIPT.json').write_bytes(receipt.read_bytes())
            run_checked([sys.executable,N/'tools/continue_private_checkpoint.py','--review',R,
                         '--output','continue-inputs','--expect-stage',args.expect_stage],cwd=N,timeout=260)
            anchor=latest(R/'battlefield.jsonl');confirm_safe(anchor,args.expect_stage)
            print(variant,'warming real gameplay for',args.warmup,'seconds',flush=True)
            until=time.monotonic()+args.warmup
            while time.monotonic()<until:
                process_identity(pid,(R/'project').resolve())
                confirm_safe(latest(R/'battlefield.jsonl'),args.expect_stage,anchor)
                time.sleep(.25)
            (R/'ab-pose.json').write_text(json.dumps(latest(R/'battlefield.jsonl'),indent=2)+'\n')
            if (R/'project/graphics.settings').read_bytes() != source_settings:
                raise RuntimeError('Settings changed after normal Continue.')
            run_checked([sys.executable,N/'tools/benchmark_battlefield_native.py','--review',R,
                         '--output','benchmark','--seconds',str(args.seconds),
                         '--expect-stage',args.expect_stage],cwd=N,timeout=args.seconds+15)
            confirm_safe(latest(R/'battlefield.jsonl'),args.expect_stage,anchor)
            # No recorder competes with the benchmark. These normal screenshots
            # are collected after timing, with a second still documenting wind.
            run_checked([sys.executable,N/'tools/live_review.py','full','shot','ab-view'],cwd=N,timeout=10)
            time.sleep(1.)
            run_checked([sys.executable,N/'tools/live_review.py','full','shot','ab-view-wind'],cwd=N,timeout=10)
        except BaseException as exc:
            error=repr(exc)
            raise
        finally:
            if pid is not None:
                try:
                    if process_identity(pid,(R/'project').resolve())!=identity:raise RuntimeError('Native PID identity changed before close.')
                    windows=[row.split()[0] for row in subprocess.check_output(['wmctrl','-lp'],text=True).splitlines()
                             if len(row.split())>2 and row.split()[2]==str(pid) and 'Legends of Valwood | A Place in the Watch' in row]
                    if len(windows)==1:run_checked(['wmctrl','-ic',windows[0]],timeout=5)
                    else:os.kill(pid,signal.SIGTERM)
                except (ProcessLookupError,RuntimeError):
                    pass
            try:launcher.wait(timeout=35)
            finally:launcher_log.close()
            if R.exists():
                (R/'ab-control.json').write_text(json.dumps({'variant':variant,'error':error,
                    'normal_continue_only':True,'no_motion_inputs':True,'clean_sigterm_requested':pid is not None},indent=2)+'\n')
                R.rename(output/variant)
        if launcher.returncode != 0:
            raise RuntimeError('Private native launcher did not exit cleanly.')
        result=analyse_run(output/variant)
        grouping=result['shadow_fetch_grouping']
        if not grouping or grouping.get('enabled') != (variant=='gather'):
            raise RuntimeError('Native profile did not confirm the requested shadow shader path.')
        results[variant]=result
    scalar,gather=results['scalar'],results['gather']
    if scalar['benchmark']['render_sizes'] != gather['benchmark']['render_sizes']:
        # Counts differ with FPS; compare dimensions rather than their counts.
        if set(scalar['benchmark']['render_sizes']) != set(gather['benchmark']['render_sizes']):
            raise RuntimeError('A/B render dimensions differ; do not promote.')
    if len(scalar['benchmark']['render_sizes']) != 1 or len(gather['benchmark']['render_sizes']) != 1:
        raise RuntimeError('Adaptive rendering changed resolution within a measured interval.')
    for key in ('root','yaw'):
        if scalar['pose'][key] != gather['pose'][key]:
            raise RuntimeError('A/B native launch poses differ; do not promote.')
    comparison={'passed_controls':True,'visual_review_required':True,'promoted':False,
                'scalar':scalar,'gather':gather,
                'mean_frame_ms_saved':scalar['benchmark']['timing']['mean_ms']-gather['benchmark']['timing']['mean_ms'],
                'gpu_ms_saved':scalar['render_total_gpu_ms']['mean_ms']-gather['render_total_gpu_ms']['mean_ms']}
    (output/'comparison.json').write_text(json.dumps(comparison,indent=2)+'\n')
    print(json.dumps({'comparison':str(output/'comparison.json'),'automatic_promotion':False},indent=2),flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--resume-from',required=True)
    parser.add_argument('--output',required=True)
    parser.add_argument('--expected-binary',required=True)
    parser.add_argument('--expect-stage',default='escape')
    parser.add_argument('--warmup',type=float,default=30.)
    parser.add_argument('--seconds',type=float,default=30.)
    parser.add_argument('--order',choices=('scalar,gather','gather,scalar'),default='scalar,gather')
    parser.add_argument('--scalar-reference',help='Reuse one clean scalar interval from this same exact binary/checkpoint/settings/world/shader plan; execute gather only.')
    parser.add_argument('--run',action='store_true',help='Explicitly execute after root hands over native UI.')
    args=parser.parse_args()
    if not 30<=args.warmup<=60 or not 30<=args.seconds<=60:
        parser.error('Use30–60 seconds for both warmup and measurement.')
    if not args.run:
        print(json.dumps({'prepared_only':True,'would_resume_from':args.resume_from,
              'would_output':args.output,'order':args.order,'inputs':'normal Continue only',
              'needs_explicit_ui_handoff':True},indent=2));return 0
    try:execute(args)
    except Exception as exc:
        print(json.dumps({'passed':False,'error':str(exc)}),file=sys.stderr);return 1
    return 0


if __name__=='__main__':
    raise SystemExit(main())
