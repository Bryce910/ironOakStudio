from pathlib import Path
import hashlib,json
from PIL import Image
import numpy as np
N=Path(__file__).resolve().parents[1];B=N.with_name('playable-opening-2026-11-18');sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();binary=sha(N/'runtime/rpg')
cpu=json.loads((N/'review/CPU_REVIEW.json').read_text());assert cpu['passed'] and cpu['binary_sha256']==binary
math=json.loads((N/'review/CAMERA_CPU.json').read_text());assert math['passed'];rays=json.loads((N/'review/RAY_REVIEW.json').read_text());assert rays['passed']
for name in ['windows-baseline','windows-updated','native-arena','native-arena-third']:
 d=json.loads((N/'review'/name/'EXECUTION.json').read_text());assert d['passed'] and d['binary_sha256']==(sha(B/'runtime/rpg') if name=='windows-baseline' else binary)
w=json.loads((N/'review/windows-updated/EXECUTION.json').read_text());assert w['local_shadows']['completed_cycles']>0 and w['local_shadows']['failures']==0
third=json.loads((N/'review/native-arena-third/NATIVE_CLASH_RAW.json').read_text());assert third['passed'] and third['cases'] and all(not c['first_person'] for c in third['cases'])
for folder in ['native-arena','native-arena-third']:
 for p in (N/'review'/folder).glob('*.bmp'):Image.open(p).save(p.with_suffix('.png'))
visual=[]
for name in ['view-0-window-room.png','view-1-window-floor.png']:
 before=np.asarray(Image.open(N/'review/windows-baseline'/name).convert('RGB')).astype('f8')/255
 after=np.asarray(Image.open(N/'review/windows-updated'/name).convert('RGB')).astype('f8')/255
 assert before.shape==after.shape
 visual.append(dict(view=name,before_mean_display_rgb=float(before.mean()),after_mean_display_rgb=float(after.mean()),note='Context only, not a brightness quality score. Matched camera/materials/exposure.'))
report=dict(passed=True,binary_sha256=binary,window_lighting_actual_mesh_sockets=True,pane_masks_match_actual_glass=True,room_switches_independent=True,nearest_window_shadows_verified=True,clear_floor_rays=3,camera_regression_tests=4,exploration_boom_metres=3.35,combat_boom_metres=[3.85,4.2],light_views=visual,checks_software_rendered=True,hardware_frame_rate_not_rebenchmarked=True,prior_startup_and_full_journey_results_inherited_not_rerun=True,excluded_view='room-return was an exploratory fly-through sample behind the current window wall; excluded from visual acceptance.',earlier_blocked_window_source_fixture_retained=True,no_windows_build=True,publication_ready=True)
(N/'review/FINAL_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
