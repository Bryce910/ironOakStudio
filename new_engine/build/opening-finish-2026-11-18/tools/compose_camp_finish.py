#!/usr/bin/env python3
"""Recompose existing camp understory and material strokes without new rows."""
from pathlib import Path
import copy, hashlib, importlib.util, json, math, shlex, struct
import numpy as np

N=Path(__file__).resolve().parents[1]
P=N/'world-project'
ROOT=N.parents[1]
OUT=N/'review/scenery'
OUT.mkdir(parents=True,exist_ok=True)
def sha(b): return hashlib.sha256(b).hexdigest()
tool=ROOT/'docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py'
spec=importlib.util.spec_from_file_location('camp_mesh',tool)
mesh=importlib.util.module_from_spec(spec);spec.loader.exec_module(mesh)
raw=(P/'ground/showcase.height').read_bytes()
magic,nx,nz,xmin,zmin,sx,sz=struct.unpack_from('<8sII4d',raw)
assert magic==b'VWHGT1\0\0' and sx==sz==1
height=np.frombuffer(raw,'<f4',offset=48).reshape(nz,nx)
def ground(x,z):
    ix=(x-xmin)/sx;iz=(z-zmin)/sz
    if not 0<=ix<nx-1 or not 0<=iz<nz-1:return None
    i=int(ix);j=int(iz);tx=ix-i;tz=iz-j
    return float((1-tz)*((1-tx)*height[j,i]+tx*height[j,i+1])+tz*((1-tx)*height[j+1,i]+tx*height[j+1,i+1]))
studio_path=P/'world.studio';studio_bytes=studio_path.read_bytes();studio=studio_bytes.decode()
assert not any(s.startswith(('override ','group ')) for s in studio.splitlines())
assets={shlex.split(s)[1]:shlex.split(s)[2] for s in studio.splitlines() if s.startswith('asset ')}
geometry={}
def vertices(alias):
    if alias not in geometry:
        geometry[alias]=mesh.geometry(P/'models'/(assets[alias]+'.glb'))
    return geometry[alias]
def item(a):
    return dict(x=(float(a[1])-.5)*100800,z=(float(a[2])-.5)*(100800*2818/5009),
        yaw_deg=float(a[3]),scale=float(a[4]),pitch_deg=float(a[8]),roll_deg=float(a[9]),
        sx=float(a[10]),sy=float(a[11]),sz=float(a[12]),y=float(a[13]))
def encode_scale(a, pose):
    # The production source parser requires uniform scale >=.15; retain the
    # desired final geometry with the supported per-axis multipliers.
    uniform=max(.15,pose['scale']);factor=pose['scale']/uniform
    a[4]=f'{uniform:.10g}'
    for index,key in ((10,'sx'),(11,'sy'),(12,'sz')):
        axis=pose[key]*factor;assert .1<=axis<=8
        a[index]=f'{axis:.12g}'
rows={name:(P/'scenery'/name).read_text().splitlines() for name in ('bep_environment_instances.txt','demo_polish_instances.txt')}
before={name:'\n'.join(lines)+'\n' for name,lines in rows.items()}
changes=[]
targets=[
 ('bep_environment_instances.txt',57,'bep_nature_bush_a',-11994.9,5177.0,.125,17),
 ('bep_environment_instances.txt',116,'bep_nature_bush_a',-11994.2,5178.1,.140,123),
 ('bep_environment_instances.txt',235,'bep_nature_bush_a',-11980.9,5175.8,.130,-83),
 ('bep_environment_instances.txt',298,'bep_nature_bush_b',-11979.9,5176.8,.092,64),
 ('bep_environment_instances.txt',346,'bep_nature_bush_a',-11995.0,5166.8,.145,-135),
 ('bep_environment_instances.txt',539,'bep_nature_bush_b',-11994.1,5165.4,.100,178),
 ('bep_environment_instances.txt',55,'bep_nature_fern',-11994.2,5176.1,.52,35),
 ('bep_environment_instances.txt',350,'bep_nature_fern',-11993.2,5177.5,.47,-52),
 ('bep_environment_instances.txt',355,'bep_nature_fern',-11980.8,5174.5,.56,118),
 ('demo_polish_instances.txt',47,'bep_nature_fern',-11980.0,5177.8,.50,-111),
 ('demo_polish_instances.txt',420,'bep_nature_fern',-11993.7,5166.8,.53,159),
 ('demo_polish_instances.txt',423,'bep_nature_fern',-11995.8,5165.8,.48,-18),
]
for name,line,alias,x,z,scale,yaw in targets:
    old=rows[name][line-1];a=old.split();assert a[0]==alias and len(a)>=14
    prior=item(a);v=vertices(alias)
    support=copy.deepcopy(prior);mesh.support(support,v,ground)
    embed=support['y']-prior['y']
    new=copy.deepcopy(prior);new.update(x=x,z=z,scale=scale,yaw_deg=yaw)
    mesh.align_plant(new,ground);mesh.support(new,v,ground,embed)
    assert abs(ground(x,z)-11.8)<.10 and math.hypot(x+11988,z-5172)>6.4
    a[1]=f'{x/100800+.5:.12g}';a[2]=f'{z/(100800*2818/5009)+.5:.12g}'
    a[3]=f'{yaw:.10g}';encode_scale(a,new)
    a[8]=f'{new["pitch_deg"]:.10g}';a[9]=f'{new["roll_deg"]:.10g}';a[13]=f'{new["y"]:.12g}'
    updated=' '.join(a);rows[name][line-1]=updated
    changes.append(dict(file='scenery/'+name,line=line,kind='understory',alias=alias,before=old,after=updated,
        actual_basal_embedding_preserved_m=embed,ground_before=ground(prior['x'],prior['z']),ground_after=ground(x,z)))

# Shorten existing tuft silhouettes around the new islands, retaining every
# root, source index, wind flag and opaque physics identity.
islands=[(-11994.6,5177.1),(-11980.6,5176.1),(-11994.6,5166.2)]
pool=[]
for name,lines in rows.items():
    for i,line in enumerate(lines):
        a=line.split()
        if len(a)<14 or a[0] not in ('opening_verge_grass_long','opening_verge_grass_broad'):continue
        old=item(a);d=min(math.hypot(old['x']-x,old['z']-z) for x,z in islands)
        if d<5.0 and math.hypot(old['x']+11988,old['z']-5172)>6.4:pool.append((d,name,i))
for _,name,i in sorted(pool)[:24]:
    old=rows[name][i];a=old.split();prior=item(a);v=vertices(a[0])
    support=copy.deepcopy(prior);mesh.support(support,v,ground);embed=support['y']-prior['y']
    new=copy.deepcopy(prior);new['scale']*=.70;mesh.support(new,v,ground,embed)
    encode_scale(a,new);a[13]=f'{new["y"]:.12g}';updated=' '.join(a);rows[name][i]=updated
    changes.append(dict(file='scenery/'+name,line=i+1,kind='shorter_edge_grass',alias=a[0],before=old,after=updated,
        actual_basal_embedding_preserved_m=embed))
assert len(changes)==36 and len({(x['file'],x['line']) for x in changes})==36,len(changes)

replacements={
 'layer "Compacted camp earth" 12 13 -1 0.72 0.65 0.54 1.3 0.99 0 0.75 0':
 'layer "Compacted camp earth" 12 13 -1 0.72 0.65 0.54 0.95 0.99 0 0.95 0',
 'paint -11988 5170 18 0.52 7 0':'paint -11988 5170 18 0.64 7 0',
 'paint -11988 5172 5.6 0.92 5 0':'paint -11988 5172 5.15 0.90 5 0',
 'paint -11988 5177 3.9 0.82 5 0':'paint -11988 5177 3.4 0.82 5 0',
 'paint -11986 5181 3.2 0.68 5 0':'paint -11986 5181 2.8 0.72 5 0',
 'paint -11984 5185 2.6 0.55 5 0':'paint -11984 5185 2.8 0.70 5 0',
}
studio_changes=[];out=[]
for i,line in enumerate(studio.splitlines(),1):
    if line.startswith('surfaces '):
        record=json.loads(line[9:])
        if record in replacements:
            updated='surfaces '+json.dumps(replacements[record]);studio_changes.append(dict(line=i,before=line,after=updated));line=updated
    out.append(line)
assert len(studio_changes)==6
assert studio_path.read_bytes()==studio_bytes,'Concurrent studio mutation: rerun after merging'
files=[]
for name,lines in rows.items():
    p=P/'scenery'/name;assert not p.is_symlink()
    old=before[name].encode();assert p.read_bytes()==old
    new=('\n'.join(lines)+'\n').encode();(OUT/(name+'.before')).write_bytes(old);p.write_bytes(new)
    assert len(old.splitlines())==len(new.splitlines())
    files.append(dict(relative_path='scenery/'+name,before_sha256=sha(old),after_sha256=sha(new)))
new_studio=('\n'.join(out)+'\n').encode();(OUT/'world.studio.before').write_bytes(studio_bytes);studio_path.write_bytes(new_studio)
files.append(dict(relative_path='world.studio',before_sha256=sha(studio_bytes),after_sha256=sha(new_studio)))
report=dict(schema='valwood.camp-finish.v1',staged=True,gpu_used=False,files=files,instance_changes=changes,studio_changes=studio_changes,
    added_instances=0,removed_instances=0,source_aliases_and_row_order_unchanged=True,paint_count_unchanged=True,terrain_and_physics_unchanged=True,
    source_height_sha256=sha(raw),source_geometry_tool_sha256=sha(tool.read_bytes()),
    limits=['No new visual acceptance yet.','Pale road cut is real baked heightfield relief and world-XZ material stretching; geometry is not changed by this pass.'])
(OUT/'CANDIDATE.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(dict(files=files,changes=len(changes),studio_changes=len(studio_changes)),indent=2))
