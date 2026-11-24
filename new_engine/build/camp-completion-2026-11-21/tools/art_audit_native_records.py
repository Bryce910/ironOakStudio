"""Audit every new art root with current production world_atlas water, native parser bounds."""
from pathlib import Path
import json,hashlib,re,shlex,subprocess,math,numpy as np
N=Path(__file__).resolve().parents[1];P=N/'world-project';R=N/'review/art';j=json.loads((R/'ART_PATCH.json').read_text());source=(N/'source-v1/engine/world_scenery.cpp').read_text();tree={m[0]:m[1]=='true'for m in re.findall(r'\{"([^\"]+)",\s*"[^\"]+",\s*(true|false),',source)}
for line in(P/'world.studio').read_text().splitlines():
 q=shlex.split(line)
 if q and q[0]=='asset':tree[q[1]]=q[5]=='2'
for rec in json.loads((R/'STUDIO_ART_PATCH.json').read_text()):q=rec['after'].split();tree[q[1]]=q[5]=='2'
files={f.name:f.read_text().splitlines()for f in(P/'scenery').glob('*.txt')};records=[];seen=set()
for rec in j['changed_rows']+j.get('far_treeline_replacements',[]):
 file=Path(rec['file']).name;line=files[file][rec['line']-1];q=line.split();assert q[0]==rec.get('family',rec.get('alias',q[0])),(file,rec['line'],q[0]);key=(file,rec['line'])
 if key not in seen:records.append({'file':file,'line':rec['line'],'row':line});seen.add(key)
for rec in j['new_placements']:
 matches=[i for i,line in enumerate(files['authored_instances.txt'],1)if line==rec['row']];assert len(matches)==1,(rec['alias'],matches);key=('authored_instances.txt',matches[0])
 if key not in seen:records.append({'file':key[0],'line':key[1],'row':rec['row']});seen.add(key)
points=[];bad=[]
for rec in records:
 q=rec['row'].split();core=q[:q.index('wind')]if'wind'in q else q;nums=list(map(float,core[1:]));alias=q[0];cap=2.5 if alias=='broadleaf'else 2. if tree[alias]else 4.;checks=[len(core)==14,len(' '.join(core))<=256,len(rec['row'])<=320,all(math.isfinite(x)for x in nums),0<=nums[0]<=1,0<=nums[1]<=1,-360<=nums[2]<=360,.15<=nums[3]<=cap,nums[4]==0,nums[5]==0,-128<=nums[6]<=128,abs(nums[7])<=360,abs(nums[8])<=360,all(.1<=x<=8 for x in nums[9:12]),.1<=nums[12]<=12000]
 if'wind'in q:checks +=[len(q)==17,all(0<=float(v)<=2 for v in q[-2:])]
 if not all(checks):bad.append(rec|{'reason':'native_numeric_range','failed_checks':[i for i,x in enumerate(checks)if not x],'cap':cap})
 points.append([(nums[0]-.5)*100800,(nums[1]-.5)*100800*2818/5009]);rec['pivot']=nums[12]
np.asarray(points,'<f8').tofile(R/'all-new-roots.points.bin');sampler=N/'tools/art_native_ground_sampler';r=subprocess.run([str(sampler),str(P),str(R/'all-new-roots.points.bin'),str(R/'all-new-roots.samples.bin')],capture_output=True,text=True);(R/'all-new-roots-sampler.log').write_text(r.stdout+r.stderr);r.check_returncode();samples=np.fromfile(R/'all-new-roots.samples.bin','<f8').reshape(-1,8)
for rec,sample in zip(records,samples):
 rec['sample']={'ground_y':float(sample[2]),'walk_y':float(sample[3]),'wet':bool(sample[4]),'water_y':float(sample[5])}
 if sample[4]and rec['pivot']<=sample[5]+.1:bad.append(rec|{'reason':'native_water_root_rejection'})
out={'passed':not bad,'generation':str(N),'new_art_roots':len(records),'native_tree_base_scale_limit':2,'native_prop_base_scale_limit':4,'water_sampler_sha256':hashlib.sha256(sampler.read_bytes()).hexdigest(),'failures':bad,'records':records,'scene_sha256':{str(P/'scenery'/f):hashlib.sha256((P/'scenery'/f).read_bytes()).hexdigest()for f in files}};(R/'NATIVE_RECORD_AUDIT.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps({'passed':not bad,'new_roots':len(records),'failures':[{'file':b['file'],'line':b['line'],'alias':b['row'].split()[0],'reason':b['reason']}for b in bad]}))
