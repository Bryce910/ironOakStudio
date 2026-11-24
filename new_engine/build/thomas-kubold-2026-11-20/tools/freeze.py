"""Freeze reviewed character inputs; publication remains a separate root action."""
from pathlib import Path
import json
import sys

N = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(N))
from check_unified import verify_inputs

inputs = verify_inputs()
ready = dict(schema='valwood.thomas-kubold.ready.v1', ready=True, generation=str(N),
             runtime_reused=True, source_reused=True, native_only=True,
             Windows_work_performed=False, **inputs)
path = N/'UNIFIED_READY.json'
temporary = path.with_suffix('.json.tmp')
temporary.write_text(json.dumps(ready, indent=2, sort_keys=True)+'\n')
temporary.replace(path)
print('Frozen:', path)
