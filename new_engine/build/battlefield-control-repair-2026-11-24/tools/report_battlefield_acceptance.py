#!/usr/bin/env python3
"""Validate completed normal-input native evidence before writing a review receipt.

No game inputs, launches, process control, progress/settings changes, publication,
or implicit acceptance writes. --output is required and created exclusively only
after every guard passes. Performance collection validity and FPS targets are
reported separately from functional acceptance and never assert AAA quality.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import shlex
import stat
import sys

sys.dont_write_bytecode = True
SCENE = frozenset({
    'world.studio', 'opening/battlefield.settings', 'opening/road-traffic.settings', 'scenery/battlefield_instances.txt',
    *('models/battlefield/'+s+'.glb' for s in (
        'barricade','barricade_mid','discarded_arms','discarded_arms_mid','casualty_blood','corpse_back','corpse_back_mid',
        'corpse_side','corpse_side_mid','arrival_blood')),
    *('scenery/'+s+'_instances.txt' for s in (
        'authored','starting_area','rainforest','castle_village','tanthoon_bandit_camp',
        'castle_streets','bep_environment','demo_finish','demo_polish')),
})


class Refusal(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise Refusal(message)


def digest(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):
            h.update(block)
    return h.hexdigest()


def regular(path, maximum=16*1024*1024):
    path=Path(path)
    try:
        info=path.lstat()
    except FileNotFoundError as exc:
        raise Refusal('Required evidence is missing: '+str(path)) from exc
    require(stat.S_ISREG(info.st_mode),'Evidence must be a regular file without a symlink: '+str(path))
    require(info.st_size<=maximum,'Evidence exceeds bounded size: '+str(path))
    return path


def document(path):
    try:
        data=json.loads(regular(path).read_text())
    except (ValueError,UnicodeError) as exc:
        raise Refusal('Invalid JSON evidence: '+str(path)) from exc
    require(isinstance(data,dict),'Evidence must be a JSON object: '+str(path))
    return data


def below(path, root):
    path=Path(path).absolute()
    require(path.resolve().is_relative_to(root.resolve()) and path.resolve()!=root.resolve(),
            'Evidence/output must remain below '+str(root))
    return path


def safe_relative(value):
    require(isinstance(value,str),'Relative evidence path must be text.')
    path=PurePosixPath(value)
    require(not path.is_absolute() and '..' not in path.parts and value not in ('','.','..'),
            'Unsafe relative evidence path: '+value)
    return Path(*path.parts)


def fnv(body, basis=1469598103934665603):
    result=basis
    for byte in body.encode():
        result=((result^byte)*1099511628211)&((1<<64)-1)
    return result


def field(parts, name):
    try:
        return parts[parts.index(name)+1]
    except (ValueError,IndexError) as exc:
        raise Refusal('Missing native receipt field: '+name) from exc


def inventory(payload):
    parts=shlex.split(payload)
    require(parts[:2]==['VALWOOD_INVENTORY','1'],'Invalid native inventory schema.')
    begin=parts.index('items')+1;end=parts.index('equipment')
    counts=[int(x) for x in parts[begin:end]]
    require(len(counts)==16 and all(x>=0 for x in counts),'Invalid native inventory item counts.')
    return {'counts':counts,'weapon':int(parts[end+1]),'receipts':int(field(parts,'receipts'))}


def questless_inventory(value):
    # world_player_profile::strip_quest keeps durable belongings/receipts while
    # quest tokens are projected solely from the story checkpoint.
    counts=list(value['counts']);counts[14]=counts[15]=0
    return dict(value,counts=counts)


def final_saves(project):
    save_dir=project/'saves'
    require(save_dir.is_dir() and not save_dir.is_symlink(),'Private native save directory must be real.')
    for path in save_dir.iterdir():
        regular(path)
    opening=shlex.split(regular(save_dir/'opening.progress').read_text())
    require(opening[:2]==['VALWOOD_OPENING','1'],'Invalid native opening schema.')
    bf=shlex.split(field(opening,'battlefield_progress'))
    require(bf[:2]==['VALWOOD_BATTLEFIELD','1'],'Invalid saved battlefield schema.')
    battle={key:int(field(bf,key)) for key in ('checkpoint','sword','wins')}
    require(battle=={'checkpoint':3,'sword':1,'wins':3},'Final native save must contain campsite, one sword and three actual wins.')
    quest_payload=field(opening,'first_journey');header,body=quest_payload.split('\n',1)
    head=header.split()
    require(head[:2]==['VALWOOD_FIRST_JOURNEY_PROGRESS','1'] and len(head)==3 and int(head[2])==fnv(body),
            'Native first-journey checksum is invalid.')
    quest=shlex.split(body);progress=quest.index('progress')+1;reward=quest.index('reward')+1
    journey={'stage':int(field(quest,'stage')),'camp':int(field(quest,'camp')),
             'progress':[int(x) for x in quest[progress:progress+5]],
             'reward':[int(x) for x in quest[reward:reward+3]],'checksum_verified':True}
    require(journey['stage']==7 and journey['camp']==7 and journey['progress']==[1]*5 and journey['reward']==[12,1,1],
            'Native quest is not complete with the full twelve-copper reward.')
    require(int(field(opening,'camp_spoken'))==7,'Opening and quest traveler credits disagree.')
    inv=inventory(field(opening,'inventory'))
    profile_path=regular(save_dir/'player.progress');profile_bytes=profile_path.read_text()
    body,separator,tail=profile_bytes.rpartition('checksum ')
    require(separator and int(tail.strip(),16)==fnv(body,14695981039346656037),'Native durable player profile checksum is invalid.')
    profile=shlex.split(profile_bytes)
    require(profile[:2]==['VALWOOD_PLAYER_PROFILE','1'],'Invalid durable player profile schema.')
    durable=inventory(field(profile,'inventory'));coins=int(field(profile,'story'))
    require(questless_inventory(inv)==durable and coins==12,'Saved and durable non-quest inventory/reward receipts disagree.')
    require(inv['counts'][1]==1 and inv['counts'][11]==1 and inv['counts'][14]==0 and inv['counts'][15]==1 and inv['receipts']&29==29,
            'Final sword/bread/satchel/letter/reward inventory receipts are incomplete.')
    return {'battlefield':battle,'journey':journey,'inventory':inv,'durable_inventory':durable,'story_coins':coins}, {
        str(path.relative_to(project)):digest(path) for path in save_dir.iterdir()}


def result_record(path, session, project, pid):
    path=below(path,session);data=document(path)
    require(data.get('passed') is True and data.get('error') is None,'Native helper did not pass: '+str(path))
    require(data.get('normal_physical_inputs_only') is True,'Native result is not a normal-input review: '+str(path))
    require(data.get('pid')==pid and data.get('private_project')==str(project),
            'Native results must come from the same private PID/project: '+str(path))
    require(data.get('game_state_injected',False) is False and data.get('saved_progress_written_by_driver',False) is False,
            'Injected or driver-written game state cannot be accepted.')
    for image in data.get('screenshots',[]):
        regular(below(image,session),64*1024*1024)
    return data


def audit_trace(path):
    regular(path,1024*1024*1024)
    stages=set();wins=set();rows=0;previous_frame=-1;sword=False;sword_grants=0;ending=False
    with path.open() as stream:
        for line in stream:
            require(len(line)<=32768,'Native observer row exceeds bounds.')
            row=json.loads(line);frame=row.get('frame')
            require(type(frame) is int and frame>previous_frame,'Native observer frame identity repeated/reset.')
            previous_frame=frame;rows+=1;stages.add(row.get('stage'));wins.add(row.get('wins'))
            have_sword=row.get('sword') is True
            if have_sword and not sword:sword_grants+=1
            require(not sword or have_sword,'Recovered sword disappeared from the actual native trace.')
            sword=have_sword;ending=ending or row.get('screen')==5
    required={'inactive','dazed','recover','flee_to_ambush','duck','dead','tackle','struggle','stab','stand','escape','forest','campsite'}
    require(required.issubset(stages),'Native observer lacks the full actual apartment/QTE/escape/forest/camp sequence.')
    require(wins=={0,1,2,3} and sword and sword_grants==1 and ending,
            'Native observer lacks distinct three wins, one sword acquisition and the ending screen.')
    return {'path':str(path),'sha256':digest(path),'rows':rows,'stages':sorted(stages),
            'distinct_combat_wins':sorted(wins),'sword_acquisitions':sword_grants,'ending_screen_observed':ending}


def validate(candidate, session, results, benchmarks, target_fps=30.):
    candidate=Path(candidate).resolve(strict=True);review=candidate/'review'
    session=below(session,review).resolve(strict=True);project=session/'project'
    require(project.is_dir() and not project.is_symlink(),'Native project must be a real private directory.')
    require(document(project/'PRIVATE_CAPTURE_PROJECT.json').get('private') is True,'A private project marker is required.')
    closed=document(session/'state-preservation.json')
    require(type(closed.get('exit_code')) is int and closed['exit_code']==0 and closed.get('live_state_unchanged') is True,
            'Acceptance requires a clean exit0 and verified unchanged live state.')
    pid=int(regular(session/'pid',32).read_text());require(pid>0,'Invalid reviewed PID.')
    proc=Path('/proc')/str(pid)/'cmdline'
    if proc.exists():
        try:
            command=proc.read_bytes().split(b'\0')
        except OSError:
            command=[]
        require(str(project).encode() not in command,'Reviewed private game is still running.')
    launch=document(session/'launch.json');build_path=candidate/'runtime-native-v1/BUILD_RECEIPT.json';build=document(build_path)
    binary=(candidate/'runtime/rpg').resolve(strict=True);regular(binary,128*1024*1024);binary_hash=digest(binary)
    require(binary_hash==launch.get('binary_sha256')==build.get('binary_sha256'),
            'Current binary, review launch and BUILD receipt must match exactly.')
    command=launch.get('command',[])
    require(command and Path(command[0]).resolve()==binary and '--project' in command and command[command.index('--project')+1]==str(project),
            'Launch command must identify the actual candidate binary and private project.')
    require('--play' in command and not any(x.startswith('--qa') or x in ('--headless','--offscreen') for x in command),
            'Review launch must use ordinary native gameplay.')
    require(launch.get('resume') is None,'Full fresh-apartment acceptance cannot start from a seeded/resumed private save.')
    protected=launch.get('protected',{})
    require(isinstance(protected,dict) and protected and
            {'world.studio','graphics.settings'}.issubset({Path(p).name for p in protected}),
            'Launch must bind the protected published world/settings snapshot.')
    for path,expected in protected.items():
        require(digest(regular(path,256*1024*1024))==expected,'Protected live state changed: '+path)
    sources=build.get('source_sha256')
    require(isinstance(sources,dict) and any(str(p).startswith('engine/') for p in sources),'Complete compiled source hashes are required.')
    require(set(sources)=={str(p.relative_to(candidate/'source-v1')) for p in (candidate/'source-v1').rglob('*') if p.is_file()},
            'BUILD must bind the complete current source tree, including added files.')
    for relative,expected in sources.items():
        require(digest(regular(candidate/'source-v1'/safe_relative(relative),128*1024*1024))==expected,
                'Compiled source changed after BUILD: '+relative)
    plan_path=review/'PUBLISH_PLAN.json';plan=document(plan_path);rows=plan.get('files',[])
    require(len(rows)==23 and {r.get('relative_path') for r in rows}==SCENE,'Publication plan must bind exactly twenty-three expected scene files.')
    scene={}
    for row in rows:
        relative=row['relative_path'];path=safe_relative(relative);expected=row.get('candidate',{}).get('sha256')
        actual=digest(regular(candidate/'world-project'/path,256*1024*1024))
        require(actual==expected,'Scene file changed after PUBLISH_PLAN: '+relative)
        require(digest(project/path)==actual,'Reviewed private scene differs from publication candidate: '+relative)
        scene[relative]=actual
    require(set(results)=={'apartment','qte','escape','camp'},'All four native review results are required.')
    checked={key:result_record(path,session,project,pid) for key,path in results.items()}
    apartment=checked['apartment']
    require(apartment.get('review')=='fresh_apartment_to_battlefield_normal_inputs' and all(apartment.get(key) is True for key in (
        'apartment_started','bills_read','console_interacted','new_game_confirmed','battlefield_arrival_observed')),
        'Apartment/TV/transport review is incomplete.')
    require(apartment.get('cutscenes_skipped') is False and apartment.get('receipts')=={
        'opening_checkpoint':2,'battlefield_checkpoint':0,'sword':0,'wins':0},'Fresh unskipped apartment arrival receipts are incomplete.')
    qte=checked['qte']
    require(all(qte.get(key) is True for key in ('intentional_missed_duck_death','local_retry','fresh_duck_success','tap_struggle_success')),
            'Native missed-duck/retry/duck/struggle review is incomplete.')
    for key in ('opening','durable_profile'):
        inv=qte.get('receipts',{}).get(key,{})
        require(inv.get('sword_count')==1 and inv.get('weapon')==1 and inv.get('receipts',0)&16,
                'Native QTE sword grant did not commit exactly one sword.')
    escape=checked['escape'];duels=escape.get('duels',[])
    require(escape.get('review')=='battlefield_escape_normal_inputs' and len(duels)==3,'All three actual escape duels must be reviewed together.')
    for index,duel in enumerate(duels,1):
        require(duel.get('soldier')==index and duel.get('wins')==index and duel.get('attempts',0)>0 and
                duel.get('last_attack_serial',0)>duel.get('first_attack_serial',0),'Missing distinct real player attack/win evidence.')
    er=escape.get('receipts',{});ei=er.get('inventory',{})
    require(er.get('battlefield_checkpoint')==3 and er.get('real_wins')==3 and er.get('sword')==1 and
            ei.get('sword_count')==1 and ei.get('receipts',0)&16,'Escape did not verify the real campsite/sword checkpoint.')
    bridge=escape.get('bridge_samples',[])
    require(len(bridge)>=3 and max(r['root'][0] for r in bridge)-min(r['root'][0] for r in bridge)>=10 and
            min(r['root'][1] for r in bridge)>=11.95,'No continuous supported native bridge crossing evidence.')
    camp=checked['camp']
    require(camp.get('review')=='camp_first_quest_normal_inputs' and camp.get('raider_real_victory_reviewed') is True and
            camp.get('thanks_for_playing_observed') is True and camp.get('raider_attacks',0)>0 and
            camp.get('last_observer',{}).get('screen')==5,'Real quest combat and Thanks for Playing screen were not reviewed.')
    saved,saves=final_saves(project)
    cr=camp.get('receipts',{});cq=cr.get('opening',{});cp=cr.get('durable_profile',{})
    require(all(cq.get(key)==value for key,value in saved['journey'].items()) and cq.get('inventory')==saved['inventory'] and
            cp.get('story_coins')==12 and cp.get('inventory')==saved['durable_inventory'],'Native ending result and current checksummed saves disagree.')
    require(any('thanks-for-playing' in Path(p).name for p in camp.get('screenshots',[])),'Thanks for Playing screenshot is missing.')
    trace=audit_trace(session/'battlefield.jsonl')
    measurements=[];require(bool(benchmarks),'At least one thirty-second benchmark measurement is required.')
    require(math.isfinite(target_fps) and target_fps>0,'Invalid performance target.')
    for path in benchmarks:
        path=below(path,session);data=document(path);timing=data.get('timing',{})
        require(data.get('passed') is True and data.get('error') is None and data.get('collector_read_only') is True and
                data.get('sent_inputs') is False and data.get('launched_game') is False,'Benchmark collection was not valid and read-only.')
        require(data.get('private_project')==str(project) and data.get('process',{}).get('pid')==pid and
                data.get('process',{}).get('command')==command,
                'Benchmark does not belong to the reviewed native PID/project.')
        require(data.get('requested_seconds',0)>=30 and data.get('actual_seconds',0)>=29.9 and
                timing.get('observed_coverage_seconds',0)>=24 and data.get('frames',0)>=120,
                'Benchmark does not cover thirty seconds of normal playable frames.')
        fps=timing.get('mean_fps');require(isinstance(fps,(int,float)) and math.isfinite(fps) and fps>0,'Invalid measured FPS.')
        p95=timing.get('p95_ms');require(isinstance(p95,(int,float)) and math.isfinite(p95) and p95>0,'Invalid measured frame-time percentile.')
        require(data.get('graphics_settings_sha256')==data.get('graphics_settings_end_sha256'),
                'Benchmark mixed graphics settings.')
        settings=regular(path.parent/'graphics.settings')
        require(digest(settings)==data['graphics_settings_sha256'],'Captured benchmark graphics settings hash differs.')
        measurements.append({'report':str(path),'sha256':digest(path),'collection_valid':True,
            'duration_seconds':data['actual_seconds'],'frames':data['frames'],'render_sizes':data.get('render_sizes',{}),
            'timing':timing,'vram':data.get('vram',{}),'graphics_settings_sha256':digest(settings),
            'target_fps':target_fps,'mean_fps_target_met':fps>=target_fps,'p95_frame_budget_met':p95<=1000/target_fps,
            'target_met':fps>=target_fps and p95<=1000/target_fps})
    target_met=all(row['target_met'] for row in measurements)
    # Recheck identities at the write boundary; do not accept a concurrent build.
    require(digest(binary)==binary_hash and document(build_path)==build,'Candidate build changed while validating evidence.')
    return {'schema':'valwood.native-functional-acceptance.v1','created_utc':datetime.now(timezone.utc).isoformat(),
        'passed':True,'functional_acceptance_passed':True,'binary_sha256':binary_hash,'source_sha256':sources,
        'scene_sha256':scene,'private_session':str(session),'private_project':str(project),'pid':pid,
        'clean_exit':closed,'normal_native_inputs_only':True,'real_native_final_receipts':saved,'save_sha256':saves,'native_observer':trace,
        'evidence_sha256':{str(path):digest(path) for path in [session/'launch.json',build_path,plan_path,session/'state-preservation.json',*results.values()]},
        'performance':{'measurements':measurements,'fps_target_met':target_met,
            'criterion':'mean_fps >= target_fps and p95_ms <= 1000 / target_fps',
            'status':'target_met_in_recorded_measurements' if target_met else 'target_unmet',
            'fps_target_passed':target_met,'quality_certification':'not_assessed',
            'limitation':'Functional acceptance is not an AAA visual-quality certification; collection validity is not an FPS pass.'},
        'game_inputs_sent':False,'game_progress_or_settings_written':False,'publication_performed':False}


def write_receipt(report, output, candidate):
    output=below(output,Path(candidate)/'review')
    require('project' not in output.relative_to(Path(candidate)/'review').parts,'Receipt cannot be written inside a private game project.')
    require(output.parent.is_dir() and not output.parent.is_symlink(),'Receipt parent must already be a real review directory.')
    with output.open('x') as stream:
        json.dump(report,stream,indent=2);stream.write('\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--session',type=Path,required=True)
    for key in ('apartment','qte','escape','camp'):
        parser.add_argument('--'+key,type=Path,required=True)
    parser.add_argument('--benchmark',type=Path,action='append',required=True)
    parser.add_argument('--target-fps',type=float,default=30.)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();candidate=Path(__file__).resolve().parents[1]
    try:
        results={key:getattr(args,key) for key in ('apartment','qte','escape','camp')}
        report=validate(candidate,args.session,results,args.benchmark,args.target_fps)
        write_receipt(report,args.output,candidate)
    except (Refusal,OSError,ValueError,IndexError,KeyError,TypeError) as exc:
        print(json.dumps({'passed':False,'receipt_written':False,'error':str(exc)}),file=sys.stderr);return 1
    print(json.dumps({'passed':True,'functional_acceptance_passed':True,'receipt':str(args.output),
        'fps_target_met':report['performance']['fps_target_met'],'quality_certification':'not_assessed'}));return 0


if __name__=='__main__':
    raise SystemExit(main())
