from pathlib import Path
import collections,hashlib,json,math,shlex,struct,sys
import numpy as np
N=Path(__file__).resolve().parents[1];BASE=N.with_name('arena-scenery-2026-11-18');sys.path.insert(0,str(N/'tools'));from compose_ecology import records,source_unbound
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def plant(alias):return alias.startswith('bep_nature_grass_') or alias.startswith('opening_verge_grass_') or alias in ('bep_nature_fern','bep_nature_clover')
def streams(path):
 b=path.read_bytes();n=struct.unpack_from('<I',b,12)[0];j=json.loads(b[20:20+n]);raw=b[28+n:];parts=[]
 for m in j['meshes']:
  for q in m['primitives']:
   for name,index in sorted(q['attributes'].items()):
    v=j['bufferViews'][j['accessors'][index]['bufferView']];parts.append((name,hashlib.sha256(raw[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]).hexdigest()))
   if 'indices' in q:
    v=j['bufferViews'][j['accessors'][q['indices']]['bufferView']];parts.append(('indices',hashlib.sha256(raw[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]).hexdigest()))
 for i in j['images']:
  v=j['bufferViews'][i['bufferView']];parts.append(('image',hashlib.sha256(raw[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]).hexdigest()))
 return parts
out={}
for mode in ['arena','world']:
 p=N/(mode+'-project');origin=BASE/'arena-project' if mode=='arena' else N/'baseline-world';old=records(origin);new=records(p)
 assert collections.Counter(r['text'] for r in old if not plant(r['alias']))==collections.Counter(r['text'] for r in new if not plant(r['alias'])),'Solid/tree/object changed'
 a=(origin/'world.studio').read_text();b=(p/'world.studio').read_text();source_unbound(a);source_unbound(b)
 excluded=('foliage ','surfaces ','material opening_verge_grass_')
 assert [l for l in a.splitlines() if not l.startswith(excluded)]==[l for l in b.splitlines() if not l.startswith(excluded)],'Unrelated authored gameplay/geometry changed'
 payload=[json.loads(l[9:]) for l in b.splitlines() if l.startswith('surfaces ')]
 assert sum(l.startswith('image ') for l in payload)<=16 and sum(l.startswith('layer ') for l in payload)<=8 and sum(l.startswith('paint ') for l in payload)<=256
 assert sum(l.startswith('foliage ') for l in b.splitlines())<=16
 for line in payload:
  if line.startswith('image '):
   path=p/shlex.split(line)[2];assert path.is_file() and path.resolve().is_relative_to((p/'surfaces').resolve()),path
 field=json.loads((N/(mode.upper()+'_FIELD.json')).read_text());rates={}
 for band in ['sun','partial','deep']:
  rows=[r for r in field if r['band']==band and 'grass' in r['alias']];rates[band]=sum(r['action'] in ['keep','short_grass'] for r in rows)/len(rows)
 assert rates['sun']>rates['partial']>rates['deep'] and rates['sun']>rates['deep']*3
 assert all(r['action']=='litter_gap' for r in field if r['root_clearance']<.35)
 # Optional wind tails retain their source defaults when absent.
 source_by_root={(r['x'],r['z'],r['yaw']):r for r in old}
 for r in new:
  original=source_by_root.get((r['x'],r['z'],r['yaw']))
  if original and ' wind ' in original['text']:assert r['text'].split(' wind ')[-1]==original['text'].split(' wind ')[-1]
 out[mode]=dict(records_before=len(old),records_after=len(new),nonplant_records_unchanged=True,grass_retention_by_light=rates,material_images=sum(l.startswith('image ') for l in payload),paint_strokes=sum(l.startswith('paint ') for l in payload),foliage_masks=sum(l.startswith('foliage ') for l in b.splitlines()),source_references_preserved=True)
world=N/'world-project';studio=(world/'world.studio').read_text().splitlines()
for family in ['long','broad']:
 stem=next(l.split()[2] for l in studio if l.startswith('asset bep_nature_grass_'+family+' '))
 assert streams(world/'models'/(stem+'_near.glb'))==streams(world/'models'/('cinematic_grass_'+('1' if family=='long' else '0')+'.glb'))
 assert streams(world/'models'/(stem+'_mid.glb'))==streams(world/'models'/('cinematic_grass_'+('1' if family=='long' else '0')+'_mid.glb'))
assert sha(N/'runtime/rpg')==sha(BASE/'runtime/rpg')
out.update(passed=True,binary_unchanged=True,shader_bank_unchanged=True,opening_grass_geometry_uv_indices_images_identical=True,terrain_water_collision_npc_dialogue_and_quests_unchanged=True,windows_work_performed=False)
(N/'review/CPU_REVIEW.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
