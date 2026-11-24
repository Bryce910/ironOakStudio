from pathlib import Path
import importlib.util,json,sys,difflib
N=Path(__file__).resolve().parents[1];sys.dont_write_bytecode=True
spec=importlib.util.spec_from_file_location('animation_scope_builder',N/'build_native_candidate.py');b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
before,after=b.inventory(b.SOURCE_BASE/'source-v1'),b.inventory(b.SOURCE)
changes={p:dict(before=before.get(p),after=after.get(p)) for p in sorted(set(before)|set(after)) if before.get(p)!=after.get(p)}
shaders=b.inventory(N/'shared/shaders');prior=b.inventory(b.BASE/'shared/shaders')
b.write_json(N/'review/SOURCE_CHANGE_SCOPE.json',dict(baseline=str(b.SOURCE_BASE/'source-v1'),candidate=str(b.SOURCE),changes=changes))
b.write_json(N/'review/SHADER_CHANGE_SCOPE.json',dict(baseline=str(b.BASE/'shared/shaders'),candidate_sha256=shaders,changed_shader_sha256={p:h for p,h in shaders.items() if prior.get(p)!=h},same_as_published_hair_correction=True))
patch=''
for p in changes:
    a=(b.SOURCE_BASE/'source-v1'/p).read_text().splitlines(True);c=(b.SOURCE/p).read_text().splitlines(True)
    patch+=''.join(difflib.unified_diff(a,c,fromfile='baseline/'+p,tofile='candidate/'+p))
(N/'review/SOURCE_DIFF.patch').write_text(patch);print('Reviewed changes:',', '.join(changes))
