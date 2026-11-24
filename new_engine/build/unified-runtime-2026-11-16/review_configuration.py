from pathlib import Path
import json,os,sys,tempfile,time
N=Path(__file__).resolve().parent;sys.path.insert(0,str(N))
import launch
checks={};modes={m:launch.configuration(m) for m in launch.MODES}
checks['explicit_exploration_mode']=modes['world'][1].get('VALWOOD_WORLD_EXPLORATION')=='1' and 'VALWOOD_WORLD_EXPLORATION' not in modes['demo'][1]
checks['one_binary']=len({v[0][0] for v in modes.values()})==1
checks['one_world']=modes['demo'][2]==modes['editor'][2]==modes['world'][2]
checks['shared_shaders']=all((v[2]/'shaders').resolve()==(N/'shared/shaders').resolve() for v in modes.values())
old=os.environ.get('VALWOOD_JOUST_ARENA');os.environ['VALWOOD_JOUST_ARENA']='1'
try:
 checks['isolated_mode_flags']='VALWOOD_JOUST_ARENA' not in launch.configuration('demo')[1] and 'VALWOOD_OPENING_PROLOGUE' not in modes['arena'][1]
finally:
 if old is None:os.environ.pop('VALWOOD_JOUST_ARENA',None)
 else:os.environ['VALWOOD_JOUST_ARENA']=old
try:launch.configuration('demo',['--project','/tmp/wrong']);checks['project_override_rejected']=False
except ValueError:checks['project_override_rejected']=True
checks['cinematic_editor_workspace']=launch.configuration('editor',['--cinematics'])[1].get('VALWOOD_CINEMATIC_EDITOR')=='1'
checks['area_maps_preserved']=all(launch.configuration('world',['--area',area])[2]==project and launch.configuration('editor',['--area',area])[2]==project for area,(project,_) in launch.AREAS.items())
realn,reale,reala=launch.N,launch.E,launch.AREAS
with tempfile.TemporaryDirectory(prefix='valwood-config-review-') as temp:
 launch.N=Path(temp)/'build/test';launch.E=Path(temp)
 launch.AREAS={a:(Path(temp)/'projects'/a,v) for a,(_,v) in reala.items()}
 ps=[launch.N/'shared/graphics.settings',launch.E/'projects/valwood_current/graphics.settings',launch.N/'arena-project/graphics.settings',launch.N/'joust-project/graphics.settings',*[p/'graphics.settings' for p,_ in launch.AREAS.values()]]
 for i,p in enumerate(ps):p.parent.mkdir(parents=True,exist_ok=True);p.write_text('initial'+str(i));os.utime(p,ns=(10+i,10+i))
 ps[1].write_text('character_textures=2\n');os.utime(ps[1],ns=(100,100));launch.sync_settings(ps[2].parent)
 checks['demo_preference_reaches_arena']=ps[2].read_text()=='character_textures=2\n'
 ps[2].write_text('character_textures=3\n');os.utime(ps[2],ns=(200,200));launch.sync_settings(ps[3].parent);launch.sync_settings(ps[1].parent)
 checks['arena_preference_reaches_joust_and_demo']=ps[3].read_text()==ps[1].read_text()=='character_textures=3\n'
launch.N,launch.E,launch.AREAS=realn,reale,reala
record=dict(passed=all(checks.values()),checks=checks,mode_projects={m:str(v[2]) for m,v in modes.items()},binary=modes['demo'][0][0]);(N/'review/CONFIGURATION_REVIEW.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(checks,indent=2));assert record['passed']
