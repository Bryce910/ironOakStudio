from pathlib import Path
import json,shutil,importlib.util
n=Path('/home/computer/Desktop/final_engine/new_engine/build/road-polish-2026-11-15')
spec=importlib.util.spec_from_file_location('build_road',n/'build_native_candidate.py');b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
plan=b.plan();old=b.BASE/'runtime-native-v1';new=b.RUNTIME
by={r['logical_source']:r for r in json.loads((old/'BUILD_PLAN.json').read_text())['units']}
def remap(v):return v.replace(str(b.BASE/'source-v1'),str(b.SOURCE)).replace(str(old),str(new))
count=0;miss=[]
for r in plan['units']:
 if r['logical_source'] not in by:miss.append(r['logical_source']);continue
 o=by[r['logical_source']];receipt=json.loads(Path(o['target']).with_suffix('.json').read_text());deps={remap(p):h for p,h in receipt['actual_dependency_sha256'].items()}
 if [remap(x) for x in receipt['command']]!=r['command'] or not all(Path(p).is_file() and b.sha(p)==h for p,h in deps.items()):miss.append(r['logical_source']);continue
 target=Path(r['target']);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(o['target'],target)
 Path(r['dependency_file']).write_text(remap(Path(o['dependency_file']).read_text()))
 b.write_json(target.with_suffix('.json'),dict(receipt,**r,actual_dependency_sha256=deps,inherited_from_verified_object=str(o['target']),inherited_sha256=b.sha(o['target'])));count+=1
b.write_json(n/'CACHE_SEED_REVIEW.json',dict(copied_verified_objects=count,recompile=miss,hardlinks=False));print(count,miss)
