"""Private baseline/candidate prop route; never changes live settings/saves."""
from pathlib import Path
import importlib.util, json, os, shutil, subprocess, sys, hashlib
N=Path(__file__).resolve().parents[1];E=N.parents[1]
B=E/'build/camp-completion-2026-11-21'
kind,run=sys.argv[1:3]
R=N/'review'/run;R.mkdir(exist_ok=False);Q=R/'project';Q.mkdir()
P=E/'projects/valwood_current'
for f in P.iterdir():
    dest=Q/f.name
    if f.name in ['shaders','saves']:continue
    if f.name=='scenery':shutil.copytree(f,dest,symlinks=False)
    elif f.name=='models':
        dest.mkdir()
        for x in f.iterdir():(dest/x.name).symlink_to(x.resolve(),target_is_directory=x.is_dir())
    elif f.is_dir():dest.symlink_to(f.resolve(),target_is_directory=True)
    else:shutil.copy2(f,dest,follow_symlinks=True)
(Q/'saves').mkdir()
shutil.copy2(P/'saves/opening.progress',Q/'saves/opening.progress')
(Q/'shaders').symlink_to(B/'shared/shaders',target_is_directory=True)
if kind=='candidate':
    (Q/'models/camp_props_20261122').symlink_to(N/'models',target_is_directory=True)
    path=Q/'world.studio';v=path.read_text()
    replacements={'asset nm_city_region_camp camp_atmosphere/camp_hearth ':'asset nm_city_region_camp camp_props_20261122/camp_hearth ',
                  'asset camp_finish_bridge_planks art_completion/bridge_planks ':'asset camp_finish_bridge_planks camp_props_20261122/bridge_finish '}
    for old,new in replacements.items():assert v.count(old)==1;v=v.replace(old,new)
    # Grass-only exclusion: keep the tent bedding clear without changing tree,
    # fern or rock ecology. Radius includes measured source clump reach+wind.
    v+='\necology -11991.55 5167.9 5.8 0 0 0 0 11222026 256 40\n'
    path.write_text(v)
    bridge=json.loads((N/'bridge-work/PATCH.json').read_text())['placement_patch']
    path=Q/'scenery/authored_instances.txt';v=path.read_text()
    assert v.count(bridge['before'])==1;path.write_text(v.replace(bridge['before'],bridge['after']))
    if os.environ.get('VALWOOD_PROP_NEAR_ONLY')=='1':
        path=Q/'world.studio'
        v=path.read_text().replace('camp_props_20261122/camp_hearth 2 ','camp_props_20261122/camp_hearth 1 ').replace('camp_props_20261122/bridge_finish 2 ','camp_props_20261122/bridge_finish 1 ');path.write_text(v)
g=Q/'graphics.settings'
# Use the previously accepted private High2K settings in both comparison runs.
# This leaves the user's live choices untouched, including older-format files.
shutil.copy2(B/'review/high-tier-acceptance/project/graphics.settings',g)
spec=importlib.util.spec_from_file_location('current_launcher',B/'launch.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
cmd,env,_=m.private_configuration('world');cmd[cmd.index('--project')+1]=str(Q)
cmd+=['--scenic','city-meadow','--profile',str(R/'profile'),'--frames','100000','--graphics-preset','balanced','--auto-quality','off','--render-scale','.75','--benchmark']
env.update(VALWOOD_QA_GRAPHICS_FILE=str(g),SDL_GAMECONTROLLER_IGNORE_DEVICES='0x046d/0xc21d',VALWOOD_DEMO_PRESENTATION='0',VALWOOD_QA_SOURCE_ROUTE=str(N/'review/props.route'),VALWOOD_QA_SOURCE_ROUTE_REPORT=str(R/'route'),VALWOOD_QA_SOURCE_ROUTE_CAPTURE=str(R/'shot'))
env['LD_PRELOAD']=str(B/'tools/probe_gl_debug.so')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
models={str(p):sha(p) for p in (N/'models').glob('*.glb')} if kind=='candidate' else {}
(R/'launch.json').write_text(json.dumps(dict(command=cmd,binary_sha256=sha(Path(cmd[0])),world_sha256=sha(Q/'world.studio'),model_sha256=models,environment={k:v for k,v in env.items() if k.startswith('VALWOOD_')},project=str(Q)),indent=2)+'\n')
with (R/'PREFLIGHT.json').open('w') as out, (R/'preflight.log').open('w') as err:
    # This production parser/terrain/cover query is CPU only. The older scene
    # preflight creates an offscreen EGL device, incompatible with this host's
    # explicit hardware selection; the actual native run validates all models.
    subprocess.run([str(N/'audit-work/query_tent_grass'),str(Q),str(R/'grass-roots.csv')],check=True,stdout=out,stderr=err)
with (R/'game.log').open('w') as out:
    p=subprocess.Popen(cmd,cwd=E,env=env,stdout=out,stderr=subprocess.STDOUT)
    (R/'pid').write_text(str(p.pid));print('PID',p.pid,flush=True)
    try:rc=p.wait(timeout=600)
    except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=20);raise
    print('exit',rc,flush=True);sys.exit(rc)
