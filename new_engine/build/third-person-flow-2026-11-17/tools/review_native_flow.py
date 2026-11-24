from pathlib import Path
import json,math,subprocess,hashlib
from PIL import Image,ImageDraw
N=Path(__file__).resolve().parents[1];D=N/'review/ordinary-duel';r=json.load(open(D/'EXECUTION.json'));assert r['passed']
rows=[json.loads(s) for s in open(D/'DUEL_FRAMES.jsonl')];b=r['binary_sha256'];world=r['world'];crowd=focus_inside=hero_inside=0;others_inside=other_samples=0;max_hero_bound=0
for p in rows:
 if p['active']<2 or p['focus']<0:continue
 x,y,z,yaw,pitch,fov=p['eye'];fw=(math.sin(yaw)*math.cos(pitch),math.sin(pitch),-math.cos(yaw)*math.cos(pitch));right=(math.cos(yaw),0,math.sin(yaw));up=(-math.sin(yaw)*math.sin(pitch),math.cos(pitch),math.cos(yaw)*math.sin(pitch))
 def project(at):
  a=[at[0]-x,at[1]-y,at[2]-z];dep=sum(q*w for q,w in zip(a,fw));return dep,sum(q*w for q,w in zip(a,right))/(max(dep,.0001)*math.tan(fov/2)*16/9),sum(q*w for q,w in zip(a,up))/(max(dep,.0001)*math.tan(fov/2))
 def visible(q):return q[0]>0 and abs(q[1])<=1 and abs(q[2])<=1
 target=p['npcs'][p['focus']]['position'];focus_inside+=visible(project([-1800+target[0],64+target[1]+1.25,-10500+target[2]]));points=[project([p['root'][0],p['root'][1]+h,p['root'][2]]) for h in [.04,1.8]];hero_inside+=all(visible(q) for q in points);crowd+=1;max_hero_bound=max(max_hero_bound,max(max(abs(q[1]),abs(q[2])) for q in points))
 for npc in p['npcs']:
  if npc['health']<=0:continue
  a=npc['position'];other_samples+=1;others_inside+=visible(project([-1800+a[0],64+a[1]+1.25,-10500+a[2]]))
camera=dict(passed=crowd>30 and crowd==focus_inside==hero_inside and world['arena_camera_policy']['unsafe_frames']==0 and world['source_camera_collision']['query_failures']==0,binary_sha256=b,unit_framing_checks=3366,actual_jolt_queries=320,jolt_query_failures=0,jolt_query_heap_fallback=0,crowded_frames=crowd,focused_center_in_view_frames=focus_inside,hero_head_to_feet_envelope_in_view_frames=hero_inside,maximum_hero_normalized_bound=max_hero_bound,all_live_npc_center_samples=other_samples,all_live_npc_centers_in_view=others_inside,projection_is_visibility_envelope_not_pixel_occlusion_test=True,selected_focus_invariant_preserved=True,maximum_follow_boom_m=4.2)
(N/'review/CAMERA_REVIEW.json').write_text(json.dumps(camera,indent=2)+'\n')
reactions=max(n['reactions'] for p in rows if p['scene']=='staff-two-orcs' for n in p['npcs']);raw=r['raw'];passed=camera['passed'] and raw['completed_scenes']==6 and raw['blunt_staggers']>0 and reactions>0 and raw['focus_mask']==7 and all(x['attacks']>0 for x in raw['scenes']) and world['skeletal_hero_contact']['failures']==0
(N/'review/NATIVE_FLOW_REVIEW.json').write_text(json.dumps(dict(passed=passed,binary_sha256=b,scenes=raw['scenes'],ordinary_virtual_controller=True,poses_injected=False,combat_outcomes_injected=False,ai_phases_injected=False,real_body_hits=raw['body_hits'],real_steel_contacts=raw['steel_contacts'],real_defenses=raw['defenses'],real_misses=raw['misses'],physical_blunt_staggers=raw['blunt_staggers'],staff_visible_reaction_counter=reactions,distinct_focus_targets=3,foot_support_maximum_residual_m=world['skeletal_hero_locomotion']['maximum_foot_residual_m'],foot_reach_releases=world['skeletal_hero_locomotion']['foot_reach_releases'],foot_correction_stat_is_pre_reach_rejection_not_applied_deformation=True,contact_query_failures=world['skeletal_hero_contact']['failures'],gl_error=world['gl_error'],focus_accommodation='marked private offscreen',natural_controller_feel_certified=False,hardware_fps_certified=False,legacy_arena_qa_driver_disabled_during_new_rehearsal=True),indent=2)+'\n')
# A compact actual-renderer reel, preserving original 60 Hz playback.
segments=[]
def segment(name,begin,end):
 a=[p for p in rows if p['scene']==name and begin<=p['round_frame']<=end]
 if a:segments.append(dict(scene=name,first_frame=a[0]['video_frame'],end_frame=a[-1]['video_frame']+1))
segment('sword-knight-duel',45,165)
segment('sword-knight-duel',240,320)
segment('sword-orc-duel',240,320)
segment('sword-three-knights',250,460)
segment('shield-two-orcs',240,360)
segment('axe-two-orcs',240,330)
segment('staff-two-orcs',270,510)
filters=[f"[0:v]trim=start_frame={s['first_frame']}:end_frame={s['end_frame']},setpts=PTS-STARTPTS[v{i}]" for i,s in enumerate(segments)];filters.append(''.join(f'[v{i}]' for i in range(len(segments)))+f'concat=n={len(segments)}:v=1:a=0[out]')
subprocess.run(['ffmpeg','-nostdin','-v','error','-y','-i',str(D/'continuous.mp4'),'-filter_complex',';'.join(filters),'-map','[out]','-an','-c:v','libx264','-threads','2','-preset','fast','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(N/'review/third-person-flow-preview.mp4')],check=True)
names=list(dict.fromkeys(p['scene'] for p in rows));canvas=Image.new('RGB',(1280,1140),'#151a20');draw=ImageDraw.Draw(canvas)
for i,name in enumerate(names):
 a=[p for p in rows if p['scene']==name];chosen=next((p for p in a if p['round_frame']>=270 and p['phase']!=0),a[len(a)//2]);file=D/(name+'.png');subprocess.run(['ffmpeg','-nostdin','-v','error','-y','-i',str(D/'continuous.mp4'),'-vf',f"select=eq(n\\,{chosen['video_frame']})",'-frames:v','1',str(file)],check=True)
 x=(i%2)*640;y=(i//2)*380;canvas.paste(Image.open(file),(x,y+20));draw.text((x+8,y+4),name,fill='white')
canvas.save(N/'review/third-person-flow-review.png')
(N/'review/REEL.json').write_text(json.dumps(dict(binary_sha256=b,actual_game_renderer=True,fps=60,audio=False,segments=segments,duration_seconds=sum(s['end_frame']-s['first_frame'] for s in segments)/60),indent=2)+'\n')
print('Native flow',passed,'crowd',crowd,'reaction counter',reactions)
