from pathlib import Path
import json,sys
N=Path(__file__).resolve().parents[1];sys.dont_write_bytecode=True;sys.path.insert(0,str(N))
from check_unified import verify_inputs
ready=dict(schema='valwood.hair-coverage.ready.v1',ready=True,generation=str(N),binary_reused=True,cpp_source_reused=True,character_assets_reused=True,**verify_inputs())
(N/'UNIFIED_READY.json').write_text(json.dumps(ready,indent=2,sort_keys=True)+'\n');print('Frozen hair shader patch')
