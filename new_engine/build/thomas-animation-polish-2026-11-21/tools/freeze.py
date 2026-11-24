from pathlib import Path
import datetime,json,sys
N=Path(__file__).resolve().parents[1];sys.dont_write_bytecode=True;sys.path.insert(0,str(N))
from check_unified import verify_inputs
ready=dict(schema='valwood.animation-polish.ready.v1',ready=True,generation=str(N),frozen_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),**verify_inputs())
(N/'UNIFIED_READY.json').write_text(json.dumps(ready,indent=2)+'\n');print('Animation application frozen')
