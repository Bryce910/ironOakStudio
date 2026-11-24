"""Review actual quick-step telemetry and make a silent, unretimed native reel."""
from pathlib import Path
from collections import Counter
import hashlib
import json
import re
import subprocess
from PIL import Image, ImageDraw

N = Path(__file__).resolve().parents[1]
R = N / 'review'
D = R / 'ordinary-duel'
execution = json.loads((D / 'EXECUTION.json').read_text())
assert execution['passed']
binary = hashlib.sha256((N / 'runtime/rpg').read_bytes()).hexdigest()
assert execution['binary_sha256'] == binary
rows = [json.loads(line) for line in (D / 'DUEL_FRAMES.jsonl').read_text().splitlines()]
steps = [row for row in rows if row['dodge_time'] > 0]
coverage = Counter(row['weapon'] for row in steps)
assert set(coverage) == {0, 2, 3, 4}
assert all(row['skeletal_active'] and row['skeletal_draw'] and row['character'] == 'knight09' for row in steps)
raw = execution['raw']
world = execution['world']
assert raw['dodge_rig_failures'] == 0 and len(steps) == raw['dodge_frames']
assert raw['body_hits'] > 0 and raw['steel_contacts'] > 0 and raw['defenses'] > 0 and raw['misses'] > 0
assert world['gl_error'] == 0
assert world['skeletal_hero_contact']['failures'] == 0
assert world['source_camera_collision']['query_failures'] == 0
assert world['arena_camera_policy']['unsafe_frames'] == 0

def write(name, data):
    (R / name).write_text(json.dumps(data, indent=2) + '\n')

unit_log = (R / 'quick-step-unit.log').read_text()
assert 'PASS 12 owned-rig quick-step/fresh-attack cases' in unit_log
travel = re.search(r'travel_m=([\d.]+)\.\.([\d.]+)', unit_log)
support = re.search(r'support_m=([\deE.+-]+)', unit_log)
write('QUICK_STEP_REVIEW.json', dict(
    passed=True, binary_sha256=binary, cpu_cases=12, weapon_rigs=4,
    rates_hz=[30, 60, 120], real_combat_dodge_api=True,
    actual_root_travel_m=[float(travel[1]), float(travel[2])],
    maximum_support_residual_m=float(support[1]),
    attack_during_step_rejected=True, fresh_attack_after_step_resolves_once=True,
    scope='Real owned CPU skeleton and combat clocks; isolated flat-ground movement, not a GPU test.',
    dedicated_roll_or_dodge_capture_added=False))
write('NATIVE_DODGE_REVIEW.json', dict(
    passed=True, binary_sha256=binary, scenes=raw['scenes'],
    dodge_frames_by_weapon=dict(coverage), dodge_frames_by_scene=dict(Counter(row['scene'] for row in steps)),
    retained_player_character='knight09', dodge_rig_failures=raw['dodge_rig_failures'],
    ordinary_virtual_controller=True, poses_injected=False, combat_outcomes_injected=False,
    ai_phases_injected=False, body_hits=raw['body_hits'], steel_contacts=raw['steel_contacts'],
    defenses=raw['defenses'], misses=raw['misses'], blunt_staggers=raw['blunt_staggers'],
    contact_query_failures=world['skeletal_hero_contact']['failures'],
    camera_query_failures=world['source_camera_collision']['query_failures'],
    camera_unsafe_frames=world['arena_camera_policy']['unsafe_frames'], gl_error=world['gl_error'],
    maximum_foot_support_residual_m=world['skeletal_hero_locomotion']['maximum_foot_residual_m'],
    foot_correction_stat_is_pre_reach_rejection_not_applied_deformation=True,
    focus_accommodation='Marked private offscreen only', natural_controller_feel_certified=False,
    hardware_fps_certified=False, every_model_variant_certified=False))

segments = []
shots = []
for weapon, label in [(0, 'Sword'), (2, 'Sword and shield'), (3, 'Two-handed axe'), (4, 'Staff')]:
    same = [row for row in steps if row['weapon'] == weapon]
    first = same[0]
    scene_steps = [row for row in same if row['scene'] == first['scene']]
    first_frame = max(0, first['video_frame'] - 35)
    end_frame = scene_steps[-1]['video_frame'] + 61
    segments.append(dict(scene=first['scene'], first_frame=first_frame, end_frame=end_frame, subject='Quick step and return'))
    shots.append((label, first['video_frame'] + 7))
for name, begin, end in [('sword-three-knights', 260, 440), ('staff-two-orcs', 325, 500)]:
    selected = [row for row in rows if row['scene'] == name and begin <= row['round_frame'] <= end]
    segments.append(dict(scene=name, first_frame=selected[0]['video_frame'], end_frame=selected[-1]['video_frame']+1, subject='Live combat'))
    shots.append((name, selected[len(selected)//2]['video_frame']))
filters = [f"[0:v]trim=start_frame={s['first_frame']}:end_frame={s['end_frame']},setpts=PTS-STARTPTS[v{i}]" for i,s in enumerate(segments)]
filters.append(''.join(f'[v{i}]' for i in range(len(segments))) + f'concat=n={len(segments)}:v=1:a=0[out]')
subprocess.run(['ffmpeg', '-nostdin', '-v', 'error', '-y', '-i', str(D/'continuous.mp4'),
    '-filter_complex', ';'.join(filters), '-map', '[out]', '-an', '-c:v', 'libx264',
    '-threads', '2', '-preset', 'fast', '-crf', '18', '-pix_fmt', 'yuv420p',
    '-movflags', '+faststart', str(R/'third-person-dodge-preview.mp4')], check=True)
canvas = Image.new('RGB', (1280, 1152), '#151a20')
draw = ImageDraw.Draw(canvas)
for i, (label, frame) in enumerate(shots):
    target = D / f'dodge-review-{i}.png'
    subprocess.run(['ffmpeg', '-nostdin', '-v', 'error', '-y', '-i', str(D/'continuous.mp4'),
        '-vf', f'select=eq(n\\,{frame})', '-frames:v', '1', str(target)], check=True)
    x, y = i % 2 * 640, i // 2 * 384
    canvas.paste(Image.open(target), (x, y+24))
    draw.text((x+8, y+6), label, fill='white')
canvas.save(R/'third-person-dodge-review.png')
write('REEL.json', dict(binary_sha256=binary, actual_game_renderer=True, fps=60,
    audio=False, segments=segments, duration_seconds=sum(s['end_frame']-s['first_frame'] for s in segments)/60,
    playback_retimed=False, pose_or_image_synthesis=False))
print('Dodge coverage:', dict(coverage), '; native checks passed; review media ready.')
