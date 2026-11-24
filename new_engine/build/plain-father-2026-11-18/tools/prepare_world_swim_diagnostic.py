#!/usr/bin/env python3
"""Prepare an explicitly seeded World-mode BODY diagnostic; never launches it."""
from pathlib import Path
import hashlib,json,shutil,sys

N=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(N))
from launch import configuration

origin=N/'world-project'
private=Path('/tmp/valwood-plain-father-world-swim-body-20261118')
output=N/'review/world-swim-body'
private.mkdir(exist_ok=True)
output.mkdir(exist_ok=True)
for source in origin.iterdir():
    if source.name in ('saves','settings','studio-layout.ini','world.studio.workspace') or '.recovery' in source.name:
        continue
    target=private/source.name
    if target.exists() or target.is_symlink():
        continue
    if source.is_dir():
        target.symlink_to(source.resolve(),target_is_directory=True)
    else:
        shutil.copy2(source,target,follow_symlinks=True)
(private/'saves').mkdir(exist_ok=True)
(private/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
# Controlled bootstrap only: existing great-river ribbon control point.
# Its source depth is3m, level8m; final sampled water/support are native evidence.
x,z=-12006.652259311388,5077.9100137669775
project=(origin/'project.valwood').read_text()
project='\n'.join('spawn='+str(x)+' '+str(z)+' 0' if line.startswith('spawn=') else line for line in project.splitlines())+'\n'
(private/'project.valwood').write_text(project)
shaders=private/'shaders'
if shaders.is_symlink():
    shaders.unlink()
if not shaders.exists():
    shaders.symlink_to(N/'shared/shaders',target_is_directory=True)
settings=json.loads((N/'review/ACTIVE_SETTINGS.json').read_text())['text']
(private/'graphics.settings').write_text(settings)
command,env,_=configuration('world')
command[command.index('--project')+1]=str(private)
# No --scenic: city-showcase startup uses the project's spawn at viewer3884.
i=command.index('--scenic')
del command[i:i+2]
command[command.index('--width')+1]='1600'
command[command.index('--height')+1]='900'
# No --frames, --benchmark or --profile: project_runtime.c implicitly turns
# --profile into360 bounded frames, disabling ordinary swimming/keyboard input.
# This manual diagnostic uses external screenshots and native swimming logs.
env.update(SDL_AUDIODRIVER='dummy',MESA_DEBUG='1',SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='1',SDL_GAMECONTROLLER_IGNORE_DEVICES='0x046d/0xc21d')
record={
    'diagnostic':'controlled-bootstrap World-mode plain-body swimming/render review',
    'not_an_input_only_journey_certificate':True,
    'does_not_test_opening_mode_swimming':True,
    'private_project':str(private),
    'seed_xz':[x,z],
    'source_ribbon':'Seldonne northwest great river inner',
    'nominal_source_surface_m':8,
    'nominal_source_depth_m':3,
    'native_depth_support_verification_required':True,
    'spawn_branch':'world_viewer.cpp:3884; city_showcase && !source_viewpoint && !builtin_viewpoint',
    'no_opening_or_presentation':not any(k in env for k in ('VALWOOD_OPENING_PROLOGUE','VALWOOD_PRESENTATION','VALWOOD_PLAYTEST_SLICE')),
    'normal_controls_require_no_frames_or_profile':True,
    'binary_sha256':hashlib.sha256(Path(command[0]).read_bytes()).hexdigest(),
    'command':command,
    'environment':{k:v for k,v in env.items() if k.startswith(('VALWOOD_','SDL_','MESA_')) or k in ('DISPLAY','XAUTHORITY','WAYLAND_DISPLAY')},
    'cwd':str(N.parents[1]),
    'manual_sequence':['Wait for loading to finish; verify third-person plain clothing','W briefly for actual swimming-forward pose, then release','V for first-person surface hands','Ctrl or C to dive; release to hold submerged depth','W briefly underwater, then release','Space to rise; V to restore third person','Close window normally'],
    'capture_scope':'External screenshots plus native swimming entry/exit logs; no --profile, source QA flags or injected story pass',
}
(output/'PREPARED.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps({k:record[k] for k in ('diagnostic','private_project','seed_xz','nominal_source_depth_m','binary_sha256','command','normal_controls_require_no_frames_or_profile')},indent=2))
