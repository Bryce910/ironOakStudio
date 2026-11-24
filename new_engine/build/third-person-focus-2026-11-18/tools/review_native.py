from pathlib import Path
from collections import Counter
import json, math, hashlib, subprocess
from PIL import Image, ImageDraw

N=Path(__file__).resolve().parents[1];R=N/'review';D=R/'ordinary-duel'
e=json.loads((D/'EXECUTION.json').read_text());assert e['passed']
b=hashlib.sha256((N/'runtime/rpg').read_bytes()).hexdigest();assert e['binary_sha256']==b
rows=[json.loads(s) for s in (D/'DUEL_FRAMES.jsonl').read_text().splitlines()]
total=hero_inside=target_inside=crowd=0
def project(row,point):
    x,y,z,yaw,pitch,fov=row['eye'];v=[point[0]-x,point[1]-y,point[2]-z]
    forward=(math.sin(yaw)*math.cos(pitch),math.sin(pitch),-math.cos(yaw)*math.cos(pitch))
    right=(math.cos(yaw),0,math.sin(yaw));up=(-math.sin(yaw)*math.sin(pitch),math.cos(pitch),math.cos(yaw)*math.sin(pitch))
    depth=sum(a*c for a,c in zip(v,forward))
    return depth>0 and abs(sum(a*c for a,c in zip(v,right)))<=depth*math.tan(fov/2)*16/9 and abs(sum(a*c for a,c in zip(v,up)))<=depth*math.tan(fov/2)
for row in rows:
    if row['focus']<0:continue
    total+=1;crowd+=row['active']>=2
    hero_inside+=all(project(row,[row['root'][0],row['root'][1]+h,row['root'][2]]) for h in (.04,1.8))
    npc=row['npcs'][row['focus']]['position']
    target_inside+=project(row,[-1800+npc[0],64+npc[1]+1.25,-10500+npc[2]])
steps=[r for r in rows if r['dodge_time']>0];coverage=Counter(r['weapon'] for r in steps)
assert set(coverage)=={0,2,3,4} and all(r['skeletal_active'] and r['skeletal_draw'] and r['character']=='knight09' for r in steps)
world=e['world'];raw=e['raw']
passed=total>1000 and hero_inside==target_inside==total and world['gl_error']==0 and world['skeletal_hero_contact']['failures']==world['source_camera_collision']['query_failures']==world['arena_camera_policy']['unsafe_frames']==0
assert passed
record=dict(passed=passed,binary_sha256=b,scenes=raw['scenes'],focused_frames=total,
    player_head_and_feet_in_frustum=hero_inside,selected_target_center_in_frustum=target_inside,
    crowded_focused_frames=crowd,visibility_is_projected_envelope_not_pixel_occlusion_test=True,
    dodge_frames_by_weapon=dict(coverage),dodge_rig_failures=raw['dodge_rig_failures'],
    real_body_hits=raw['body_hits'],real_steel_contacts=raw['steel_contacts'],real_defenses=raw['defenses'],
    ordinary_virtual_controller=True,poses_injected=False,combat_outcomes_injected=False,
    ai_phases_injected=False,focus_accommodation='Private offscreen only',natural_controller_feel_certified=False,
    hardware_fps_certified=False,contact_query_failures=world['skeletal_hero_contact']['failures'],
    camera_query_failures=world['source_camera_collision']['query_failures'],camera_unsafe_frames=world['arena_camera_policy']['unsafe_frames'],gl_error=world['gl_error'])
(R/'NATIVE_FOCUS_REVIEW.json').write_text(json.dumps(record,indent=2)+'\n')
segments=[];shots=[]
for scene,start,end in [('sword-knight-duel',100,210),('sword-three-knights',245,435),('shield-two-orcs',120,250),('axe-two-orcs',120,250),('staff-two-orcs',250,400)]:
    selected=[r for r in rows if r['scene']==scene and start<=r['round_frame']<=end]
    assert selected
    segments.append(dict(scene=scene,first_frame=selected[0]['video_frame'],end_frame=selected[-1]['video_frame']+1))
    shots.append((scene,selected[len(selected)//2]['video_frame']))
filters=[f"[0:v]trim=start_frame={s['first_frame']}:end_frame={s['end_frame']},setpts=PTS-STARTPTS[v{i}]" for i,s in enumerate(segments)]
filters.append(''.join(f'[v{i}]' for i in range(len(segments)))+f'concat=n={len(segments)}:v=1:a=0[out]')
subprocess.run(['ffmpeg','-nostdin','-v','error','-y','-i',str(D/'continuous.mp4'),'-filter_complex',';'.join(filters),'-map','[out]','-an','-c:v','libx264','-threads','2','-preset','fast','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(R/'third-person-focus-preview.mp4')],check=True)
canvas=Image.new('RGB',(1280,1152),'#151a20');draw=ImageDraw.Draw(canvas)
for i,(scene,frame) in enumerate(shots):
    f=D/f'focus-review-{i}.png'
    subprocess.run(['ffmpeg','-nostdin','-v','error','-y','-i',str(D/'continuous.mp4'),'-vf',f'select=eq(n\\,{frame})','-frames:v','1',str(f)],check=True)
    x=i%2*640;y=i//2*384;canvas.paste(Image.open(f),(x,y+24));draw.text((x+8,y+6),scene,fill='white')
canvas.save(R/'third-person-focus-review.png')
(R/'REEL.json').write_text(json.dumps(dict(binary_sha256=b,actual_game_renderer=True,fps=60,audio=False,segments=segments,playback_retimed=False),indent=2)+'\n')
print('Native focused frames:',total,'player:',hero_inside,'target:',target_inside,'dodge coverage:',dict(coverage))
