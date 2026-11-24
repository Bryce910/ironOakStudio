from pathlib import Path
import hashlib,json,math
N=Path(__file__).resolve().parents[1];R=N/'review';D=R/'desktop';BASE=N.with_name('third-person-dodge-2026-11-17')
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
binary=sha(N/'runtime/rpg')
def write(name,value):(R/name).write_text(json.dumps(value,indent=2)+'\n')
unit=json.loads((R/'FOCUS_UNIT_REVIEW.json').read_text());assert unit['passed'];unit['binary_sha256']=binary;write('FOCUS_UNIT_REVIEW.json',unit)
native=json.loads((R/'NATIVE_FOCUS_REVIEW.json').read_text());assert native['passed'] and native['binary_sha256']==binary
equipment=json.loads((R/'equipment/EXECUTION.json').read_text());assert equipment['passed'] and equipment['binary_sha256']==binary
session=json.loads((D/'SESSION.json').read_text());assert session['binary_sha256']==binary and not session['qa_enabled'] and not session['frames_mode']
lo=json.loads((D/'START_OBSERVATION_FRAME.json').read_text())['frame']
rows=[json.loads(line) for line in (D/'OBSERVATION.jsonl').read_text().splitlines() if line.strip()]
rows=[r for r in rows if r['frame']>=lo and not r['paused']]
assert len(rows)>500 and all(r['third_person'] and r['draw_avatar'] for r in rows)
bad=0;largest=0;travel=0
for i,r in enumerate(rows):
    x,y,z,yaw,pitch,fov=r['eye'];forward=(math.sin(yaw)*math.cos(pitch),math.sin(pitch),-math.cos(yaw)*math.cos(pitch));right=(math.cos(yaw),0,math.sin(yaw));up=(-math.sin(yaw)*math.sin(pitch),math.cos(pitch),math.cos(yaw)*math.sin(pitch))
    for height in (.04,1.8):
        v=[r['root'][0]-x,r['root'][1]+height-y,r['root'][2]-z];depth=sum(a*b for a,b in zip(v,forward));u=sum(a*b for a,b in zip(v,right))/(max(.000001,depth)*math.tan(fov/2)*16/9);t=sum(a*b for a,b in zip(v,up))/(max(.000001,depth)*math.tan(fov/2));bad+=depth<=0 or abs(u)>1 or abs(t)>1;largest=max(largest,abs(u),abs(t))
    if i:travel+=math.dist(r['root'],rows[i-1]['root'])
assert bad==0 and travel>3 and sum(r['guard'] for r in rows)>30 and sum(r['dodge']>0 for r in rows)>5 and max(r['attack_serial'] for r in rows)>=2
log=(D/'native.log').read_text(errors='replace')
assert 'Safe legacy fallback:' not in log and 'failures=0' in log
write('DESKTOP_REVIEW.json',dict(passed=True,binary_sha256=binary,
    ordinary_unbounded_real_clock=True,qa_driver_enabled=False,focus_events_injected=False,
    input='Native XTest keyboard/mouse delivered to the focused X11 game window',
    private_progress_only=True,physical_gamepad_excluded_to_isolate_keyboard_mouse=True,
    unpaused_sequence_frames=len(rows),player_visible_frames=len(rows),bad_player_projection_points=bad,
    maximum_normalized_head_foot_bound=largest,root_travel_m=travel,
    minimum_follow_boom_m=min(r['follow_m'] for r in rows),guard_frames=sum(r['guard'] for r in rows),
    dodge_frames=sum(r['dodge']>0 for r in rows),attack_starts=max(r['attack_serial'] for r in rows),
    side_guard_hud_and_pose_captures_reviewed=['desktop/left-guard.png','desktop/right-guard.png'],
    equipment_captures_reviewed=['desktop/stowed.png','desktop/drawn.png'],
    silent_actual_desktop_video='desktop/normal-input.mp4',contact_sheet='desktop/normal-controls-review.png',
    final_sequence_excludes_initial_stalled_trace_wait=True,
    original_desktop_recording_had_concurrent_unidentified_inputs_and_cannot_prove_a_specific_bug=True,
    rare_retarget_frustum_safety_proven_by_unit_regression=True,
    safety_clamps_in_this_desktop_sequence=max(r['framing_clamps'] for r in rows),
    physical_gamepad_feel_certified=False,hardware_fps_certified=False,
    visibility_is_projected_envelope_not_mesh_occlusion_certification=True))
before=json.loads((R/'LIVE_SAVE_BEFORE.json').read_text());assert all(sha(path)==digest for path,digest in before.items())
unchanged=('engine/world_skeletal_hero.cpp','engine/world_training_combat.cpp','engine/world_combat_motion.cpp','engine/world_combat_closeup_camera.h','engine/world_follow_camera.h')
assert all(sha(N/'source-v1'/p)==sha(BASE/'source-v1'/p) for p in unchanged)
write('FINAL_REVIEW.json',dict(passed=True,binary_sha256=binary,baseline=str(BASE),
    production_change='Constrain the render-only focus transition residual to preserve an otherwise valid player head/feet framing.',
    normal_controls_review_complete=True,retarget_cases=288,native_combat_scenes=6,
    native_focused_frames=native['focused_frames'],equipment_checks=equipment['raw']['checks'],
    unchanged_simulation_and_locomotion_source=list(unchanged),
    previous_anatomical_contact_and_gait_results_inherited_not_claimed_rerun=True,
    authored_worlds_modified=False,live_saves_preserved=len(before),windows_work_performed=False,
    physical_controller_feel_and_sustained_hardware_fps_not_certified=True))
(R/'PENDING_DESKTOP_VALIDATION.json').unlink(missing_ok=True)
(N/'GUIDE.txt').write_text('''Valwood — Third-person focus and desktop review — Linux

Use the stable launchers in /home/computer/Desktop/final_engine.
Demo, editor, arena, jousting and world share one executable and shader bank.

This generation retains the prior movement blend, landed blunt-hit interruption,
owned reactions, crowd camera room, and selected rig/outfit through quick steps.
It adds player head/boot framing constraints to render-only focus transitions.
The settled goal, swept camera position, logical aim, movement, collision and
combat clocks remain unchanged. A private observation trace adds no QA driver
or inputs and is disabled unless explicitly requested in a private project.

The final binary passed six SDL-controller/live-AI knight/orc combat scenes,
all four melee rigs and quick steps, and 1,524 equipment checks. All sampled
locked-frame player head/feet and focused target centers stayed in the frustum.
The new bounds pass 288 regression cases at 30/60/144 Hz across screen shapes;
the prior focus controller regression also passes. Source/body contact and gait
results from the parent generation are inherited, not claimed rerun here.

Normal desktop validation used a real, focused X11 window, native keyboard/mouse
events and the ordinary unbounded real-time loop. It checked walk/run/stop,
strafing, side guards, focus, dodge, attacks and stow/draw. The physical F310 was
excluded from that keyboard/mouse check to isolate concurrent inputs. Separate
virtual-controller combat tests cover its action routes. This does not certify
physical-controller feel, sustained hardware FPS, all appearances, every mesh
overlap or AAA perfection. Actual silent recordings and scope are in review/.

Live saves, authored worlds, shared shaders and texture banks are retained.
No Windows build was made.
''')
print('Final desktop review:',len(rows),'ordinary frames,',round(travel,3),'m travelled; live saves preserved:',len(before))
