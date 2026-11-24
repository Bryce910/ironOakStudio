"""Record the final explicitly reviewed private source and shader scope."""
from pathlib import Path
import importlib.util
import json
import sys

N=Path(__file__).resolve().parents[1]
sys.dont_write_bytecode=True
spec=importlib.util.spec_from_file_location('quality_builder',N/'build_native_candidate.py')
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
before,after=b.inventory(b.SOURCE_BASE/'source-v1'),b.inventory(b.SOURCE)
changes={p:dict(before=before.get(p),after=after.get(p)) for p in sorted(set(before)|set(after)) if before.get(p)!=after.get(p)}
bank=b.inventory(N/'shared/shaders');baseline=b.inventory(b.SOURCE_BASE/'shared/shaders')
changed={p:h for p,h in bank.items() if baseline.get(p)!=h}
if set(bank)!=set(baseline):raise RuntimeError('Shader inventory changed; explicitly review additions/removals first')
for p,h in changed.items():
    if b.sha(b.SOURCE/'shaders'/p)!=h:raise RuntimeError('Source/runtime shader bank differs: '+p)
b.write_json(N/'review/SOURCE_CHANGE_SCOPE.json',dict(schema='valwood.thomas-polish.source-scope.v1',baseline=str(b.SOURCE_BASE/'source-v1'),candidate=str(b.SOURCE),changes=changes))
b.write_json(N/'review/SHADER_CHANGE_SCOPE.json',dict(schema='valwood.thomas-polish.shader-scope.v1',baseline_sha256=baseline,candidate_sha256=bank,changed_shader_sha256=changed))
print(json.dumps(dict(changed_source_files=sorted(changes),changed_shaders=sorted(changed)),indent=2))
