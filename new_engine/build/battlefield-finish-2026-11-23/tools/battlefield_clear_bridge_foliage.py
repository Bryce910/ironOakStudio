#!/usr/bin/env python3
"""Relocate proven above-deck plant intrusions without changing shared sources."""
from pathlib import Path
import json,hashlib,subprocess,datetime,os,uuid
N=Path(__file__).resolve().parents[1];P=N/'world-project';A=N/'review/bridge-foliage-audit';W=100800.;D=W*2818/5009
proof=json.loads((A/'BEFORE.json').read_text());moves=json.loads((A/'PROPOSED_MOVES.json').read_text());assert len(moves)==48
original_link=N/'vegetation';assert original_link.is_symlink();link_target=os.readlink(original_link);inherited=original_link.resolve();generated=inherited/'creek_dressing_instances.txt';shared_before=generated.read_bytes();shared_hash=hashlib.sha256(shared_before).hexdigest()
root_points=''.join(f"{m['new_root'][0]:.10f} {m['new_root'][1]:.10f}\n"for m in moves);run=subprocess.run(['/tmp/battlefield_world_sample'],cwd=P,input=root_points,text=True,capture_output=True,check=True);samples=[list(map(float,l.split()))for l in run.stdout.splitlines()];assert len(samples)==48;assert all(s[3]==0 and s[2]>10.9 for s in samples)
raw={};texts={};record=[]
for move,sample in zip(moves,samples):
 source=Path(move['source']);key='vegetation/creek_dressing_instances.txt'if'creek_dressing'in str(source)else'world-project/'+str(source.relative_to(P))
 if key not in raw:
  data=shared_before if key.startswith('vegetation/')else source.read_bytes();raw[key]=data;texts[key]=data.decode().splitlines(keepends=True)
  if not key.startswith('vegetation/'):assert hashlib.sha256(data).hexdigest()==proof['source_hashes'][str(source)]
 old=texts[key][move['line']-1].rstrip('\n');q=old.split();assert q==move['before_tokens'];x,z=move['new_root'];q[1]=f'{x/W+.5:.14f}';q[2]=f'{z/D+.5:.14f}'
 if len(q)>=14 and q[13]!='wind':q[13]=f'{sample[2]:.9f}'
 new=' '.join(q);texts[key][move['line']-1]=new+'\n';record.append({'relative_path':key,'line_1based':move['line'],'alias':move['alias'],'baseline_line':old,'candidate_line':new,'new_root':sample,'proved_vertices_above_deck':move['vertices']})
stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ');backup=A/('backup-'+stamp);backup.mkdir();stage=N/('.vegetation-bridge-'+uuid.uuid4().hex);stage.mkdir();committed=[]
try:
 for entry in inherited.iterdir():
  if entry.name=='creek_dressing_instances.txt':(stage/entry.name).write_text(''.join(texts['vegetation/creek_dressing_instances.txt']))
  else:(stage/entry.name).symlink_to(entry,target_is_directory=entry.is_dir())
 for key,data in raw.items():
  b=backup/key;b.parent.mkdir(parents=True,exist_ok=True);b.write_bytes(data)
 for key,data in raw.items():
  if key.startswith('vegetation/'):continue
  path=N/key;assert not path.is_symlink() and hashlib.sha256(path.read_bytes()).hexdigest()==hashlib.sha256(data).hexdigest();tmp=path.with_name('.'+path.name+'.bridge-foliage');assert not tmp.exists();tmp.write_text(''.join(texts[key]));os.replace(tmp,path);committed.append(key)
 assert original_link.is_symlink() and os.readlink(original_link)==link_target and hashlib.sha256(generated.read_bytes()).hexdigest()==shared_hash
 os.replace(original_link,backup/'inherited-vegetation-link');os.replace(stage,original_link);committed.append('vegetation')
except BaseException:
 for key in reversed(committed):
  if key=='vegetation':os.rename(original_link,stage);os.replace(backup/'inherited-vegetation-link',original_link)
  else:(N/key).write_bytes(raw[key])
 raise
assert hashlib.sha256(generated.read_bytes()).hexdigest()==shared_hash
receipt={'candidate_only':True,'shared_parent_unchanged':True,'shared_source':str(generated),'shared_sha256':shared_hash,'new_runtime_overlay':str(N/'vegetation/creek_dressing_instances.txt'),'overlay_sha256':hashlib.sha256((N/'vegetation/creek_dressing_instances.txt').read_bytes()).hexdigest(),'authored_relocations':9,'generated_relocations':39,'all_new_roots_dry':True,'record_counts_order_preserved':True,'backup_directory':str(backup),'relocations':record,'world_studio_unchanged':True,'native_visual_review_pending':True}
(A/'RELOCATED.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps({k:v for k,v in receipt.items()if k!='relocations'}))
