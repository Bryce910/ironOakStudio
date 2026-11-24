from pathlib import Path
import hashlib
import json

N = Path(__file__).resolve().parents[1]
R = N / 'review'
BASE = N.with_name('third-person-flow-2026-11-17')
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
binary = sha(N/'runtime/rpg')
executions = {name: json.loads((R/name/'EXECUTION.json').read_text()) for name in ('ordinary-duel', 'equipment')}
assert all(d['passed'] and d['binary_sha256'] == binary for d in executions.values())
for name in ('QUICK_STEP_REVIEW.json', 'NATIVE_DODGE_REVIEW.json'):
    data = json.loads((R/name).read_text())
    assert data['passed'] and data['binary_sha256'] == binary
before = json.loads((R/'LIVE_SAVE_BEFORE.json').read_text())
assert all(sha(path) == digest for path, digest in before.items())
unchanged = ('engine/world_skeletal_hero.cpp', 'engine/world_training_combat.cpp',
             'engine/world_combat_motion.cpp', 'engine/world_combat_closeup_camera.h')
assert all(sha(N/'source-v1'/p) == sha(BASE/'source-v1'/p) for p in unchanged)

def write(name, data):
    (R/name).write_text(json.dumps(data, indent=2)+'\n')

write('VISUAL_REVIEW.json', dict(
    passed=True, binary_sha256=binary,
    actual_renderer_contact_sheet_reviewed='third-person-dodge-review.png',
    shield_step_before_during_after_sequence_reviewed='quick-step-sequence.png',
    selected_knight_outfit_retained=True,
    trace_confirms_selected_rig_all_75_dodge_frames=True,
    playback='Original fixed-60-Hz silent private rehearsal, not a hardware performance measurement.',
    scope='Reviewed captured frames and pose trace across sword, shield, axe and staff; no outfit/body handoff observed.',
    limitations=['Natural controller feel awaits unlocked-desktop hands-on review.',
                 'Existing backward start/walk/stop captures are reused; no dedicated dodge/roll capture was added.',
                 'This does not certify every appearance, rough-terrain gait, or every mesh overlap.',
                 'The three-opponent scene has live combat but did not execute a dodge. Other scenes cover all four rigs.']))
write('FINAL_REVIEW.json', dict(
    passed=True, binary_sha256=binary,
    native_tests={name: dict(passed=d['passed'], binary_sha256=d['binary_sha256']) for name,d in executions.items()},
    quick_step_cpu_cases=12, native_scenes=6,
    final_binary_equipment_checks=executions['equipment']['raw']['checks'],
    inherited_generation=str(BASE), inherited_contact_and_camera_tests_not_claimed_rerun=True,
    inherited_production_source_unchanged=list(unchanged),
    new_production_change='Keep the selected skeletal hero eligible throughout an ordinary quick step.',
    live_saves_preserved=len(before), authored_worlds_modified=False,
    windows_work_performed=False, natural_controller_feel_certified=False,
    hardware_fps_certified=False))
(N/'GUIDE.txt').write_text('''Valwood — Third-person movement and combat flow — Linux

Use the stable launchers in /home/computer/Desktop/final_engine.
Launch_Valwood_Arena.sh opens arena setup in third person.
Demo, editor, arena, jousting and world share one executable and shader bank.

This generation retains the improved run-to-walk pose blend, physical blunt-hit
interruptions and shorter reaction entry, plus the smoothly expanded camera
room for two or three nearby live opponents. Player movement speed, weapon
reach, difficulty and attack timing are retained.

Quick steps now keep the selected skeletal character, equipment and appearance
active. They reuse the owned backward start/walk/stop captures rather than
switching to the legacy character body. Dodge distance, stamina, cooldown and
invulnerability timing are unchanged. This is not a new authored roll capture.

Final-binary checks: six ordinary-controller/live-AI arena scenes with knights
and orcs, quick steps on all four melee rigs, twelve CPU step/fresh-attack cases
at 30/60/120 Hz, and 1,524 equipment-transition checks. Actual renderer footage
and scope are recorded under review/. Prior flow-generation anatomical contact,
locomotion and Jolt camera regression results are inherited, not claimed rerun.

The private rehearsal uses SDL virtual-controller inputs and marked offscreen
focus accommodation. It does not certify natural controller feel, hardware FPS,
every model variant, rough-terrain gait or AAA perfection. The desktop was
locked during this final private review; hands-on play remains valuable.

Live progress and authored worlds are preserved. No Windows package was built.
''')
print('Final review complete; live progress preserved:', len(before))
