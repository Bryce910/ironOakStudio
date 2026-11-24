#!/usr/bin/env python3
"""Sequential native/Wine checks of the isolated, player-controlled slices."""
from pathlib import Path
import argparse
import csv
import fcntl
import hashlib
import json
import os
import statistics
import subprocess
import time

from PIL import Image
from benchmark_graphics_settings import native_environment

ROOT=Path(__file__).resolve().parents[1]
AREAS={'city':'city-walk','jungle':'rainforest-milestone','grandstone':'stronghold-town'}

def project_paths(project_root,areas,engine=ROOT):
    """Select prepared copies, never an authoring project or linked directory."""
    root=Path(project_root) if project_root is not None else engine/'build/playtest-slices'
    if root.is_symlink() or not root.is_dir():raise ValueError('Project root must be an ordinary prepared-copy directory')
    root=root.resolve(strict=True);source_projects=(engine/'projects').resolve()
    if root==source_projects or source_projects in root.parents:
        raise ValueError('Project root cannot enter source projects')
    selected={}
    for area in areas:
        if area not in AREAS:raise ValueError('Unknown playtest area')
        parent=root/area;project=parent/'project'
        if any(path.is_symlink() or not path.is_dir() for path in (parent,project)):
            raise ValueError('Playtest project must be an ordinary prepared-copy directory: '+area)
        for name in ('project.valwood','world.studio','graphics.settings','playtest.route'):
            path=project/name
            if path.is_symlink() or not path.is_file():raise ValueError('Missing ordinary prepared-copy file: '+str(path))
        if project_root is not None:
            origin=parent/'local-origin.json'
            if origin.is_symlink() or not origin.is_file():raise ValueError('Project override requires its preparation manifest')
            metadata=json.loads(origin.read_text())
            if metadata.get('schema')!='valwood.local-playtest-copy' or metadata.get('area')!=area:
                raise ValueError('Project override preparation manifest does not match area')
        selected[area]=project
    return selected

def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def windows(path):return 'Z:'+str(path.resolve()).replace('/','\\')

def timings(path):
    if not path.is_file():return {}
    rows=list(csv.DictReader(path.open()))
    values=[float(row['frame_ms']) for row in rows if 'frame_ms' in row]
    if not values:return {'samples':len(rows)}
    ordered=sorted(values,reverse=True);tail=ordered[:max(1,(len(values)+99)//100)]
    return {'samples':len(values),'mean_fps':1000/statistics.mean(values),'one_percent_low_fps':1000/statistics.mean(tail),
            'worst_ms':max(values),'frames_below45':sum(v>1000/45 for v in values),
            'scope':'Whole run including streaming and held save/load checks; no capture in walking mode. Not a strict frame-floor certification.'}

def combat_failures(world):
    """Refuse a pass from a lone success flag or an empty/untested fight."""
    failures=[]
    qa=world.get('playtest_combat_qa',{})
    if qa.get('enabled') is not True or qa.get('passed') is not True:failures.append('combat SDL workflow')
    for field,minimum in {'cases':10,'events':24,'loss_enemy_hits':5,'win_hits':3,'win_parries':1,'win_swings':3}.items():
        if qa.get(field,0)<minimum:failures.append('combat QA '+field)
    if qa.get('continue_exploring_ui') is not True:failures.append('combat result UI Continue exploring')
    if not 0<qa.get('win_health',0)<=100:failures.append('combat QA winning health')
    if qa.get('combat_state_mutations')!=0:failures.append('combat QA state mutation')
    challenge=world.get('playtest_combat',{})
    if challenge.get('ready') is not True:failures.append('combat lane unavailable')
    for field,minimum in {'starts':3,'wins':1,'defeats':1,'hero_draw_frames':1,'enemy_visible_frames':1}.items():
        if challenge.get(field,0)<minimum:failures.append('combat '+field)
    motion=world.get('combat_motion',{})
    if motion.get('actors')!=2 or motion.get('owned_pose_assets',0)<2 or motion.get('uploads',0)<1:
        failures.append('hero/opponent rig presentation')
    vfx=world.get('combat_vfx',{})
    for field in ['hits','parries','incoming','trail_segments','draws']:
        if vfx.get(field,0)<1:failures.append('combat VFX '+field)
    if vfx.get('failures')!=0:failures.append('combat VFX failures')
    return failures

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--area',choices=AREAS,action='append')
    parser.add_argument('--mode',choices=['walk','input','opening','combat'],default='walk')
    parser.add_argument('--wine',action='store_true')
    parser.add_argument('--binary',type=Path,help='Explicit staged executable; defaults to the installed slice runtime')
    parser.add_argument('--project-root',type=Path,help='Prepared-copy collection containing AREA/project; defaults to build/playtest-slices')
    parser.add_argument('--scale',type=float,default=.7)
    args=parser.parse_args()
    areas=list(dict.fromkeys(args.area or AREAS))
    try:projects=project_paths(args.project_root,areas)
    except (OSError,ValueError) as error:parser.error(str(error))
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    binary=(args.binary or ROOT/('build/windows/editor/bin/rpg.exe' if args.wine else 'build/playtest-slices/bin/rpg')).resolve()
    before_file=ROOT/'docs/playtest-slices/2026-11-02/source-state-before.json'
    protected={name:sha(ROOT/name) for name in json.loads(before_file.read_text())}
    result={'mode':args.mode,'platform':'Windows executable under Wine' if args.wine else 'Linux native',
            'binary_sha256':sha(binary),'render_scale':args.scale,'cases':[],
            'scope':'Actual walking/collision route and saved-session replay' if args.mode=='walk' else
                'Tagged SDL/UI control workflow' if args.mode=='input' else
                'Actual SDL attack/parry, undefended defeat, retry, victory and reset; Grandstone first-hall transition is setup, not walking proof' if args.mode=='combat' else
                'Opening and rendered HUD/resource check'}
    with (ROOT/'projects/seldonne/.valwood-instance.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        for area in areas:
            project=projects[area]
            print(f'{args.mode}: {area}',flush=True)
            env,_=native_environment();env.update(VALWOOD_PRESENTATION='1',VALWOOD_PLAYTEST_SLICE=area)
            env.pop('VALWOOD_SHOWCASE',None)
            env.pop('VALWOOD_QA_PLAYTEST',None);env.pop('VALWOOD_QA_PLAYTEST_INPUT',None);env.pop('VALWOOD_QA_PLAYTEST_COMBAT',None)
            if args.mode=='walk':env['VALWOOD_QA_PLAYTEST']='1'
            if args.mode=='input':env['VALWOOD_QA_PLAYTEST_INPUT']='1'
            if args.mode=='combat':env['VALWOOD_QA_PLAYTEST_COMBAT']='1'
            arg_path=windows if args.wine else str
            profile=out/(area+'.json');capture=out/(area+'.bmp')
            command=[arg_path(binary),'--project',arg_path(project),'--play','--scenic',AREAS[area],
                     '--frames',str(28000 if args.mode=='walk' else 18000 if args.mode=='combat' else 4200 if args.mode=='input' else 420),
                     '--warmup','120','--width','1600','--height','900','--auto-quality','off','--render-scale',str(args.scale),
                     '--profile',arg_path(profile)]
            if args.mode=='walk':command+=['--benchmark','--view','river']
            if args.mode in ['opening','combat']:command+=['--capture',arg_path(capture)]
            if args.wine:
                env.update(WINEPREFIX=str(ROOT/'build/windows/wine-prefix'),WINEDEBUG='-all',WINEDLLOVERRIDES='mscoree,mshtml=')
                command=['/usr/lib/wine/wine64']+command
            (out/(area+'-command.json')).write_text(json.dumps({'command':command,'environment':{k:v for k,v in env.items() if k.startswith('VALWOOD_') or k=='WINEPREFIX'}},indent=2)+'\n')
            started=time.monotonic()
            with (out/(area+'.log')).open('w') as log:
                try:code=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=1000 if args.mode=='walk' else 600 if args.mode=='combat' else 300).returncode
                except subprocess.TimeoutExpired:code=-1
            metrics=json.loads(profile.read_text()) if profile.is_file() else {}
            world_path=Path(str(profile)+'.world.json');world=json.loads(world_path.read_text()) if world_path.is_file() else {}
            mode=world.get('playtest_slice',{});renderer=metrics.get('renderer','')
            failures=[]
            if code:failures.append('process exit '+str(code))
            if not renderer or any(s in renderer.lower() for s in ['llvmpipe','swrast','softpipe']):failures.append('native GPU renderer missing')
            for field in ['gl_error','remaining_texture_references']:
                if world.get(field)!=0:failures.append(field)
            if world.get('asset_streaming',{}).get('failed',1)!=0:failures.append('streaming failure')
            if not mode.get('enabled') or mode.get('area')!=area:failures.append('wrong slice')
            if world.get('camera_final',{}).get('fly') is not False:failures.append('grounded camera missing')
            if args.mode=='walk':
                if not mode.get('qa_complete') or not mode.get('complete'):failures.append('walking route incomplete')
                if mode.get('blocked_steps') or mode.get('slope_steps'):failures.append('blocked route')
                if mode.get('save_load_checks',0)<(2 if area=='grandstone' else 1):failures.append('save/load replay')
                if mode.get('completed_sections',0)<(2 if area=='grandstone' else 1):failures.append('landmark completion')
            if args.mode=='input' and not world.get('playtest_input',{}).get('passed'):failures.append('input/UI workflow')
            if args.mode=='combat':failures+=combat_failures(world)
            if args.mode in ['opening','combat']:
                if not capture.is_file():failures.append('missing rendered capture')
                else:Image.open(capture).save(capture.with_suffix('.png'));capture.unlink()
            case={'area':area,'passed':not failures,'failures':failures,'exit':code,'elapsed_seconds':round(time.monotonic()-started,2),
                  'renderer':renderer,'playtest_slice':mode,'input':world.get('playtest_input',{}),
                  'combat':world.get('playtest_combat',{}),'combat_qa':world.get('playtest_combat_qa',{}),
                  'combat_motion':world.get('combat_motion',{}),'combat_vfx':world.get('combat_vfx',{}),
                  'timing':timings(profile.with_suffix('.json.csv'))}
            result['cases'].append(case);result['passed']=all(c['passed'] for c in result['cases'])
            (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
            print(area+(': PASS' if case['passed'] else ': FAIL '+', '.join(failures)),flush=True)
            if failures:break
    result['source_state_unchanged']=all(sha(ROOT/name)==digest for name,digest in protected.items())
    result['binary_unchanged']=sha(binary)==result['binary_sha256']
    result['passed']=result['passed'] and result['source_state_unchanged'] and result['binary_unchanged']
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    return 0 if result['passed'] else 1

if __name__=='__main__':raise SystemExit(main())
