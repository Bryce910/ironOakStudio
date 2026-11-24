"""Freeze reviewed character-quality inputs. Publication is a separate action."""
from pathlib import Path
import json
import sys
N=Path(__file__).resolve().parents[1]
sys.dont_write_bytecode=True;sys.path.insert(0,str(N))
from check_unified import verify_inputs
inputs=verify_inputs()
ready=dict(schema='valwood.thomas-quality.ready.v1',ready=True,generation=str(N),runtime_reused=False,source_reused=False,native_only=True,Windows_work_performed=False,**inputs)
p=N/'UNIFIED_READY.json';t=p.with_suffix('.json.tmp');t.write_text(json.dumps(ready,indent=2,sort_keys=True)+'\n');t.replace(p)
print('Frozen:',p)
