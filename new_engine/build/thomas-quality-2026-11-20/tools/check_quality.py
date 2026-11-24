"""Test saved quality migrations, startup selection and normal launcher routing."""
from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess
import sys
import tempfile

N=Path(__file__).resolve().parents[1]
sys.dont_write_bytecode=True
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
out=N/'review/quality-settings';out.mkdir(parents=True,exist_ok=True)
obj=N/'runtime-native-v1/native-objects/engine/world_graphics_settings.cpp.o'
receipt=obj.with_suffix('.json')
inputs=[N/'tools/check_quality.cpp',N/'source-v1/engine/world_character_quality.h',N/'source-v1/engine/world_graphics_settings.h',obj,receipt,N/'launch.py',N/'source-v1/engine/world_graphics_viewer.inc']
before={str(p):sha(p) for p in inputs}
data=json.loads(receipt.read_text())
for p,h in data['actual_dependency_sha256'].items():
    if sha(p)!=h:raise RuntimeError('Settings dependency changed: '+p)
command=['c++','-std=c++17','-O1','-Wall','-Wextra','-I',str(N/'source-v1'),str(N/'tools/check_quality.cpp'),str(obj),'-o',str(out/'check_quality')]
subprocess.run(command,check=True)
with tempfile.TemporaryDirectory(prefix='valwood-quality-settings-') as tmp:
    proc=subprocess.run([str(out/'check_quality'),tmp],capture_output=True,text=True)
(out/'STDOUT.json').write_text(proc.stdout);(out/'STDERR.txt').write_text(proc.stderr)
if proc.returncode:raise RuntimeError('Settings/loader checks failed: '+proc.stderr)
result=json.loads(proc.stdout)
spec=importlib.util.spec_from_file_location('quality_launch_tests',N/'launch.py');launch=importlib.util.module_from_spec(spec);spec.loader.exec_module(launch)
configs={}
for mode in launch.MODES:
    command,env,project=launch.configuration(mode)
    assert env['VALWOOD_SHADER_BANK_ROOT']==str(N/'shared/shaders')
    assert command[0]==str(N/'runtime/rpg')
    if mode in ('demo','world','editor'):
        assert env['VALWOOD_CHARACTER_QUALITY_ROOT']==str(N/'assets/character-quality')
        assert 'VALWOOD_STORY_CIVILIAN_APPEARANCE' not in env and 'VALWOOD_STORY_ARMORED_APPEARANCE' not in env
    configs[mode]=dict(project=str(project),runtime=command[0],shader_bank=env['VALWOOD_SHADER_BANK_ROOT'],quality_root=env.get('VALWOOD_CHARACTER_QUALITY_ROOT'))
ui=(N/'source-v1/engine/world_graphics_viewer.inc').read_text()
assert 'SeparatorText("Characters / models")' in ui and 'Combo("Character detail"' in ui
assert 'Low\\0Medium\\0High\\0Ultra\\0' in ui
assert 'Loaded character detail: %s' in ui and 'Restart the game after applying' in ui
after={str(p):sha(p) for p in inputs}
assert before==after
record=dict(passed=result['passed'],cpu_results=result,launcher_modes=configs,ui_label_and_loaded_status_verified=True,inputs_sha256=before,inputs_unchanged=True,settings_or_save_files_touched=False,binary_sha256=sha(out/'check_quality'))
(out/'RESULT.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps(record,indent=2))
