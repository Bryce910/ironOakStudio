#!/usr/bin/env python3
"""Publish only this reviewed native arena; preserve the running session."""
from pathlib import Path
import os,json,subprocess,shutil
n=Path(__file__).resolve().parent;e=n.parents[1];root=e.parent
names=('Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh')
assert not (n/'ORC_FINISH_READY.json').exists(),'Release already published'
previous={name:(n/'previous-launchers'/name).read_bytes()for name in names}
assert all((root/name).read_bytes()==previous[name]for name in names),'Public launcher changed during review'
# Keep the latest persisted public preferences, player identity and shared save
# path; private capture profiles are never a source for this copy.
sources=[e/'build/arena-character-finish-2026-11-15/arena-project',e/'build/arena-woodland-2026-11-15/arena-project',e/'build/arena-material-quality-2026-11-15/arena-project']
for name in ('graphics.settings','arena_player_character.cfg','player-profile.config'):
 source=max((p/name for p in sources if (p/name).is_file()),key=lambda p:p.stat().st_mtime_ns)
 shutil.copy2(source,n/'arena-project'/name)
body='''#!/usr/bin/env bash
set -euo pipefail
arena_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
exec "$arena_root/new_engine/build/arena-orc-finish-2026-11-15/launch_arena.sh" "$@"
'''.encode()
def replace(path,data):
 temp=path.with_name(path.name+'.character-update.tmp')
 temp.write_bytes(data);temp.chmod(0o755);os.replace(temp,path)
try:
 for name in names:replace(root/name,body)
 subprocess.run(['python3',str(n/'check_orc_ready.py'),'--publish'],check=True)
 for name in names:subprocess.run([str(root/name),'--check-only'],check=True)
except BaseException:
 for name in names:replace(root/name,previous[name])
 (n/'ORC_FINISH_READY.json').unlink(missing_ok=True)
 raise
(n/'RELEASE_SWITCH.json').write_text(json.dumps(dict(published=True,launchers=list(names),running_session_interrupted=False,private_profiles_copied=False,windows_build_performed=False),indent=2)+'\n')
print('Published reviewed native arena. Existing running game left intact.')
