"""Exercise the candidate through the accepted CPU controller objects."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import time

N = Path(__file__).resolve().parents[1]
B = N.with_name('camp-atmosphere-2026-11-20')
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--appearance', type=Path)
parser.add_argument('--name', default='controller-cpu')
args = parser.parse_args()
if not args.name.replace('-', '').isalnum():
    raise ValueError('Choose an alphanumeric private review name')
out = N/'review'/args.name
out.mkdir(parents=True, exist_ok=True)
units = [B/'runtime-native-v1/native-objects/engine'/(name+'.o')
         for name in ('world_animation.cpp','world_skeletal_hero.cpp')]
ready = json.loads((B/'runtime-native-v1/READY.json').read_text())
inputs = [N/'tools/check_controller.cpp']
for obj in units:
    if ready['object_sha256'][str(obj)] != sha(obj):
        raise RuntimeError('Accepted controller object changed: '+str(obj))
    receipt = obj.with_suffix('.json')
    for path, expected in json.loads(receipt.read_text())['actual_dependency_sha256'].items():
        if sha(path) != expected:
            raise RuntimeError('Accepted controller dependency changed: '+path)
    inputs += [obj, receipt]
inputs += sorted((N/'motion/rigs').glob('*.glb'))
if args.appearance:
    inputs += [args.appearance, args.appearance.parent/'body.glb']
before = {str(p):sha(p) for p in inputs}
command = ['c++','-std=c++17','-O1','-I',str(N/'source-v1'),
           str(N/'tools/check_controller.cpp'),*[str(p) for p in units],'-o',str(out/'check_controller')]
subprocess.run(command, check=True)
command = [str(out/'check_controller'), str(N/'motion/rigs')]
if args.appearance:
    command += [str(args.appearance)]
start = time.monotonic()
proc = subprocess.run(command, capture_output=True, text=True)
(out/'RAW.json').write_text(proc.stdout)
(out/'STDERR.txt').write_text(proc.stderr)
after = {str(p):sha(p) for p in inputs}
record = dict(passed=proc.returncode==0 and before==after, exit_code=proc.returncode,
              elapsed_seconds=time.monotonic()-start, inputs_sha256=before,
              candidate_inputs_unchanged=before==after, exact_accepted_controller_objects=True,
              command=command, binary_sha256=sha(out/'check_controller'))
(out/'EXECUTION.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps(record, indent=2))
if proc.returncode:
    print(proc.stderr)
    raise SystemExit(proc.returncode)
if before != after:
    raise RuntimeError('Candidate assets changed during CPU rehearsal')
