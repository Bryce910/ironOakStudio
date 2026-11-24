from pathlib import Path
import csv,hashlib,json,re
from PIL import Image
N=Path(__file__).resolve().parents[1];D=N/'review';binary=hashlib.sha256((N/'runtime/rpg').read_bytes()).hexdigest()
read=lambda p:json.loads(p.read_text())
before=D/'world/baseline/gameplay-views';after=D/'world/updated/gameplay-views'
b=read(before/'EXECUTION.json');a=read(after/'EXECUTION.json');assert b['passed'] and a['passed']
assert a['binary_sha256']==binary
for k,v in a['inputs'].items():
 if k.endswith('/runtime/rpg'):continue
 prior=k.replace('/updated/','/baseline/');assert b['inputs'].get(prior)==v
world_b=read(before/'performance.json.world.json');world_a=read(after/'performance.json.world.json')
texture_b=read(before/'TEXTURE_GPU.json');texture_a=read(after/'TEXTURE_GPU.json')
assert texture_b['base_2d_uploads']==texture_a['base_2d_uploads'],'Image upload dimensions/counts changed'
keys=['peak_textures','peak_buffers','peak_renderbuffers'];memory_b=sum(texture_b[k] for k in keys);memory_a=sum(texture_a[k] for k in keys)
views={name:dict(before_fps=1000./x['median_ms'],after_fps=1000./a['timings'][name]['median_ms'],before_p95_ms=x['p95_ms'],after_p95_ms=a['timings'][name]['p95_ms']) for name,x in b['timings'].items()}
assert memory_b-memory_a>800*1024**2
assert world_a['startup_ms']<world_b['startup_ms']*.5
assert all(v['after_fps']>v['before_fps']*.9 for v in views.values())
shipping=read(D/'shipping/EXECUTION.json');journey=read(D/'journey/EXECUTION.json');j=journey['world']['first_journey_qa'];jp=read(D/'journey/performance.json')
assert j['passed'] and j['walked_m']>750 and j['save_verified'] and j['restore_verified'] and j['reward_grants']==1
assert j['qa_player_pose_writes']==j['qa_story_state_writes']==j['qa_health_writes']==j['unexpected_walk_discontinuities']==0
perf=dict(passed=True,binary_sha256=binary,scope='Five matched 1600x900 fixed-resolution camera views with all player banks; separate normal adaptive idle and full input-driven journey.',startup_before_s=world_b['startup_ms']/1000,startup_after_s=world_a['startup_ms']/1000,tracked_peak_before_MiB=memory_b/1024**2,tracked_peak_after_MiB=memory_a/1024**2,tracked_MiB_saved=(memory_b-memory_a)/1024**2,allocation_note='Sum of tracked peak GL allocations; not measured driver VRAM residency.',matched_views=views,texture_upload_sizes_counts_unchanged=True,normal_adaptive_idle_fps=shipping['profile']['mean_fps'],journey_mean_fps=jp['mean_fps'],journey_p95_ms=jp['p95_ms'],journey_one_percent_low_fps=jp['one_percent_low_fps'],journey_max_ms=jp['max_ms'],journey_tail_note='Includes destination loading, shader/probe preparation, checkpoints and framebuffer readbacks. Do not claim hitch-free gameplay from this average.')
(D/'PERFORMANCE_REVIEW.json').write_text(json.dumps(perf,indent=2)+'\n')
tests=['journey','opening','river','native-arena','native-arena-orcs','native-joust','programs']
for name in tests:
 r=read(D/name/'EXECUTION.json');assert r['passed'] and r['binary_sha256']==binary
 log=(D/name/'native.log').read_text()
 for row in re.findall(r'\[Pose streaming\].*',log):assert 'failures=0 remaining=0' in row
for f in (D/'journey').glob('*.bmp'):Image.open(f).save(f.with_suffix('.png'))
final=dict(passed=True,binary_sha256=binary,tests=tests,walked_m=j['walked_m'],checkpoint_save_load_verified=True,live_authoring_and_progress_preserved=True,shader_and_texture_bank_unchanged=True,full_editor_UI_not_retested=True,windows_work_performed=False,limitations=['Destination-loading/probe/shader hitches remain; sustained 60 FPS is not achieved on the RX5300.','Pale exposed slopes, bare scenery and close-up character presentation still need art work.','The legacy quest rehearsal disables the optional first-NPC cinematic; it does not certify a newly supplied narration track.','River controls are rehearsed from a private walking viewpoint placed in the actual river; not an on-foot shoreline-entry audit.'])
(D/'FINAL_REVIEW.json').write_text(json.dumps(final,indent=2)+'\n');print(json.dumps(perf,indent=2));print(json.dumps(final,indent=2))
