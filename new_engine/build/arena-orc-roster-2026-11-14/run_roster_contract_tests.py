#!/usr/bin/env python3
"""Test production roster parser and bounded bank transaction without live game."""
from pathlib import Path
import hashlib,json,shlex,subprocess
HERE=Path(__file__).resolve().parent;OUT=HERE/'roster-review';SOURCE=HERE/'source-v1'
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 OUT.mkdir(exist_ok=True);unit=OUT/'roster_contract_test.cpp';binary=OUT/'roster_contract_test';cc='/usr/bin/x86_64-linux-gnu-g++-13';flags=['-std=c++17','-O2','-I'+str(SOURCE)]
 deps=subprocess.check_output([cc,*flags,'-MM',str(unit)],text=True).replace('\\\n',' ')
 inputs={str(Path(p).resolve()):sha(p)for p in shlex.split(deps.split(':',1)[1])};inputs[str(Path(__file__).resolve())]=sha(__file__)
 command=[cc,*flags,str(unit),'-o',str(binary)];subprocess.run(command,check=True)
 raw=subprocess.check_output([str(binary)],text=True);result=json.loads(raw);assert result['passed']and result['malformed_rosters_rejected']>=30 and result['mock_peak_live_banks']==1 and result['mock_final_live_banks']==0
 after={p:sha(p)for p in inputs};assert inputs==after,'Production roster header changed during unit test'
 log=OUT/'roster_contract_test.stdout.json';log.write_text(raw)
 receipt=dict(schema='valwood.orc-roster.parser-switch-contract.v1',passed=True,actual_roster_asset_acceptance=False,actual_gpu_residency_acceptance=False,
              result=result,before_sha256=inputs,after_sha256=after,outputs_sha256={str(binary):sha(binary),str(log):sha(log)},command=command,
              limits=['The actual production header is used; bank allocation is a tracked mock. Actual per-character textures/geometry/contacts/GPU lifecycle require separate checks.'])
 (OUT/'ROSTER_CONTRACT_TESTS.json').write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n');print(raw,end='')
if __name__=='__main__':main()
