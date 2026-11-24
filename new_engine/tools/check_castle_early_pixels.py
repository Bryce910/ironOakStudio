#!/usr/bin/env python3
"""Exact-pixel renderer A/B, with explicit sequential native opt-in.

Default: verify an existing paired output directory; no launcher or full hashes.
--native captures a disabled reference then the optimized pass at fixed time22.
--population-frustum compares population culling with early tavern drawing
enabled in both captures; its default views are the interior and castle market.
No benchmark, build, cook, preference write or FPS claim is performed here.
--reference/--candidate compares existing BMPs without launching anything.
"""
import argparse
import fcntl
import json
import os
from pathlib import Path
import subprocess
import sys

import numpy as np
from PIL import Image
import benchmark_graphics_settings as common
import check_castle_streets as streets

ROOT=common.ROOT
VIEWS=('castle-tavern-interior','castle-tavern','castle-market')
require=common.require
EXTRA_INPUTS=(*streets.EXTRA_INPUTS,Path(__file__).resolve())


def compare_pixels(reference,candidate,difference_path=None):
    """Compare decoded RGBA pixels exactly; there is deliberately no tolerance."""
    with Image.open(reference)as image:a=np.array(image.convert('RGBA'),dtype=np.uint8)
    with Image.open(candidate)as image:b=np.array(image.convert('RGBA'),dtype=np.uint8)
    require(a.shape==b.shape,'Reference/candidate image dimensions differ')
    delta=np.abs(a.astype(np.int16)-b.astype(np.int16)).astype(np.uint8)
    changed=np.any(delta!=0,axis=2);ys,xs=np.nonzero(changed)
    bounds=[int(xs.min()),int(ys.min()),int(xs.max()+1),int(ys.max()+1)]if len(xs)else None
    if difference_path is not None:
        # RGB absolute error at its true magnitude. No amplification is used
        # to justify acceptance, and every one-LSB difference still fails.
        Image.fromarray(delta[:,:,:3]).save(difference_path)
    return {'reference':str(reference),'candidate':str(candidate),'width':a.shape[1],'height':a.shape[0],
            'pixel_equal':not bool(changed.any()),'changed_pixels':int(changed.sum()),
            'changed_rgb_pixels':int(np.any(delta[:,:,:3]!=0,axis=2).sum()),'changed_bbox_xyxy':bounds,
            'maximum_channel_error':int(delta.max()),'mean_absolute_channel_error':float(delta.mean()),
            'sum_absolute_channel_error':int(delta.sum(dtype=np.uint64)),
            'definition':'Exact equality of all decoded RGBA pixels; every nonzero difference fails.'}


def allowed_output(path):
    for directory in(ROOT/'engine',ROOT/'shaders',common.PROJECT/'models',common.PROJECT/'scenery',common.PROJECT/'ground'):
        require(path!=directory and not path.is_relative_to(directory),'Evidence cannot be inside frozen runtime inputs')


def input_metadata():
    result=common.metadata()
    for path in EXTRA_INPUTS:
        s=path.stat();result[common.label(path)]=[s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
    return result


def fingerprints_while_idle():
    lock=ROOT/'projects/seldonne/.valwood-instance.lock'
    require(lock.parent.is_dir(),'Prepare the ordinary game-instance lock directory first')
    with lock.open('a')as stream:
        try:fcntl.flock(stream.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError as error:raise ValueError('Close Valwood before full fingerprinting')from error
        try:
            result=common.fingerprints()
            result.update({common.label(p):common.digest(p)for p in EXTRA_INPUTS})
            return result
        finally:fcntl.flock(stream.fileno(),fcntl.LOCK_UN)


def population_frustum_check(world,enabled):
    state=world.get('population_frustum')
    require(isinstance(state,dict),'Missing population frustum telemetry')
    require(state.get('enabled')is enabled,'Unexpected population frustum mode')
    fields=('draw_calls','culled_batches','submitted_batches')
    require(all(isinstance(state.get(k),int)and not isinstance(state[k],bool)and state[k]>=0 for k in fields),
            'Invalid population frustum counters')
    if enabled:
        require(all(state[k]>0 for k in fields),'Population frustum candidate did not exercise calls, culls and submissions')
    else:require(all(state[k]==0 for k in fields),'Disabled population frustum reference still used culling')
    return state


def capture_environment(environment,enabled,population_frustum):
    result=environment.copy()
    if not enabled:
        result['VALWOOD_QA_DISABLE_POPULATION_FRUSTUM'if population_frustum else'VALWOOD_QA_DISABLE_EARLY_OPAQUE']='1'
    return result


def validate_capture(stem,frames,view,time_of_day,static,enabled,save_png,population_frustum=False):
    row=streets.native_world_check(stem,frames,False,'pixels',view,time_of_day,static,save_png,validate_early=False)
    world=json.loads(Path(row['world_report']).read_text())
    early=streets.early_opaque_check(world,frames,'pixels',view,expected_enabled=True if population_frustum else enabled)
    require(common.finite_number(world['lighting'].get('visual_time'))and abs(world['lighting']['visual_time']-22)<.00001,'Fixed visual time22 did not apply')
    row.update(early_opaque=early,camera_final=world['camera_final'])
    if population_frustum:row['population_frustum']=population_frustum_check(world,enabled)
    return row


def parser():
    p=argparse.ArgumentParser(description=__doc__)
    mode=p.add_mutually_exclusive_group()
    mode.add_argument('--native',action='store_true',help='Explicitly launch sequential native captures; never profiles FPS')
    mode.add_argument('--verify-existing',action='store_true',help='Read existing paired captures/reports only (default)')
    p.add_argument('--population-frustum',action='store_true',help='Compare only population frustum culling; keep early tavern drawing enabled in both captures')
    p.add_argument('--views',nargs='+',choices=VIEWS,help='Default: interior; population mode: interior and market')
    p.add_argument('--time',choices=('day','dusk'),default='dusk')
    p.add_argument('--frames',type=int,default=180)
    p.add_argument('--output',type=Path,help='Fresh evidence directory; population mode has a separate default')
    p.add_argument('--reference',type=Path,help='Compare this existing BMP without launching or native-report checks')
    p.add_argument('--candidate',type=Path,help='Candidate BMP for the existing-image-only comparison')
    p.add_argument('--report',type=Path,help='Optional fresh JSON report for existing-image-only comparison')
    return p


def main(argv=None):
    p=parser();args=p.parse_args(argv)
    if args.reference is not None or args.candidate is not None:
        require(args.reference is not None and args.candidate is not None,'Both --reference and --candidate are required')
        require(not args.native,'Existing-image comparison cannot launch native jobs')
        require(not args.population_frustum,'--population-frustum requires paired runtime captures/reports')
        report=args.report.expanduser().resolve()if args.report else None
        if report:
            allowed_output(report);require(report.suffix=='.json'and not report.exists(),'Report must be a fresh JSON file')
            report.parent.mkdir(parents=True,exist_ok=True)
        result=compare_pixels(args.reference.expanduser().resolve(),args.candidate.expanduser().resolve(),report.with_suffix('.difference.png')if report else None)
        result['scope']='Existing decoded image comparison only; no native/runtime validation or performance claim.'
        if report:report.write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,indent=2));return 0 if result['pixel_equal']else 1
    require(args.report is None,'--report is for existing-image-only mode')
    require(1<=args.frames<=3600,'Capture frames must be1..3600')
    args.views=args.views or(['castle-tavern-interior','castle-market']if args.population_frustum else['castle-tavern-interior'])
    args.output=args.output or ROOT/('docs/performance/castle-streets/population-frustum-pixels'if args.population_frustum else'docs/performance/castle-streets/early-pixels')
    require(len(args.views)==len(set(args.views)),'Duplicate capture views')
    static,_=streets.static_check();preferences=streets.check_saved_preferences()
    output=args.output.expanduser().resolve();allowed_output(output)
    if args.native:
        require(not output.exists()or not any(output.iterdir()),'Output contains earlier evidence; use a fresh directory')
        require((ROOT/'rpg').is_file()and os.access(ROOT/'rpg',os.X_OK)and common.LAUNCHER.is_file(),'Prepare the normal executable/launcher first; this helper never builds')
        output.mkdir(parents=True,exist_ok=True)
    else:require(output.is_dir(),'Existing paired evidence directory missing')
    environment,removed=common.native_environment()
    environment.update(VALWOOD_QA_FIXED_TIME='1',VALWOOD_VILLAGE_LIGHTING='dusk'if args.time=='dusk'else'afternoon')
    comparison='population-frustum'if args.population_frustum else'early-opaque'
    scope='QA flag changes only whether population receives a color-pass frustum; early opaque stays enabled in both captures.'if args.population_frustum else'QA flag changes only early rendering/late exclusion.'
    before=input_metadata()if args.native else None
    frozen=fingerprints_while_idle()if args.native else None
    if args.native:
        require(input_metadata()==before,'Inputs changed while preparing fingerprints')
        (output/'source-freeze.json').write_text(json.dumps({'sha256':frozen,'preferences_before':preferences,
            'removed_environment_keys':removed,'saved_profile':'Custom85% manual Balanced effects/60FPS; no graphics CLI override',
            'comparison':comparison,'scope':'Untimed exact-pixel A/B; fixed visual time22; '+scope},indent=2)+'\n')
    rows=[];failures=[]
    try:
        for view in args.views:
            captures={}
            for enabled in(False,True):
                mode='optimized'if enabled else'reference';stem=output/f'{view}-{mode}'
                command=[str(common.LAUNCHER),'--scenic',view,'--width','1600','--height','900','--frames',str(args.frames),'--no-hud','--capture',str(stem.with_suffix('.bmp'))]
                run_environment=capture_environment(environment,enabled,args.population_frustum)
                if args.native:
                    print(f'Native pixel A/B {view} {mode}: {args.frames} untimed frames',flush=True)
                    with stem.with_suffix('.log').open('x')as stream:
                        process=subprocess.run(command,cwd=ROOT,env=run_environment,stdout=stream,stderr=subprocess.STDOUT,check=False)
                    require(process.returncode==0,f'Native capture exited{process.returncode}; evidence retained')
                captures[mode]=validate_capture(stem,args.frames,view,args.time,static,enabled,args.native,args.population_frustum)
                captures[mode]['command']=command
                if args.native:
                    require(input_metadata()==before,'Frozen inputs changed; remaining captures cancelled')
                    require(common.preference_state()==preferences,'Saved preferences changed; helper will not restore/overwrite user edits')
            require(captures['reference']['camera_final']==captures['optimized']['camera_final'],'Paired cameras differ')
            require(captures['reference']['lighting']==captures['optimized']['lighting'],'Paired lighting/fixed-time states differ')
            row={'view':view,'captures':captures,**compare_pixels(Path(captures['reference']['capture']),Path(captures['optimized']['capture']),output/f'{view}-difference.png'if args.native else None)}
            rows.append(row);print(json.dumps({k:v for k,v in row.items()if k!='captures'}),flush=True)
    except(OSError,ValueError,KeyError,TypeError)as error:failures.append(str(error))
    finally:
        if args.native:
            try:
                require(fingerprints_while_idle()==frozen,'Frozen runtime inputs changed during A/B')
                require(common.preference_state()==preferences,'Saved preferences changed; no restoration attempted')
            except(OSError,ValueError)as error:failures.append(str(error))
        result={'scope':'Untimed native fixed-time22 exact-pixel A/B; no FPS measurement or visual tolerance.',
                'comparison':comparison,
                'pairs':rows,'failures':failures,'preferences_preserved':common.preference_state()==preferences,
                'all_pixel_equal':len(rows)==len(args.views)and all(r['pixel_equal']for r in rows)and not failures,
                'note':'Any nonzero difference fails and requires review. '+('Both captures retain early opaque drawing; only whole-batch population culling changes.'if args.population_frustum else'Early opaque capture can change nearby water refraction input; no such difference is automatically accepted.')}
        if args.native:(output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
        else:print(json.dumps(result,indent=2))
    return 0 if result['all_pixel_equal']else 1


if __name__=='__main__':
    try:sys.exit(main())
    except(OSError,ValueError,KeyError,TypeError)as error:raise SystemExit(str(error))from error
