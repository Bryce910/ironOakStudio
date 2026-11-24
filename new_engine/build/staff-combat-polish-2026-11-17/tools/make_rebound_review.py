from pathlib import Path
import json,subprocess,math,hashlib
from PIL import Image,ImageDraw
N=Path(__file__).resolve().parents[1];D=N/'review/duel-film'
rows=[json.loads(s) for s in open(D/'ANIMATION_FRAMES.jsonl')]
execution=json.load(open(D/'EXECUTION.json'));assert execution['passed']
labels=['knight01-sword-clash-third','gnur-shield-clash-third','gnur-axe-clash-third','gnur-staff-body-third','gnur-staff-clash-third']
segments=[]
for name in labels:
 a=[r for r in rows if r['label']==name and not r['equipment_queued'] and r['attack_serial'] and not r['fresh_state']]
 assert len(a)>20,name
 first=max(a[0]['video_frame']-8,0);last=min(a[-1]['video_frame']+15,execution['raw']['frames'])
 segments.append(dict(label=name,first_frame=first,end_frame_exclusive=last))
filters=[]
for i,s in enumerate(segments):filters.append(f"[0:v]trim=start_frame={s['first_frame']}:end_frame={s['end_frame_exclusive']},setpts=PTS-STARTPTS[v{i}]")
filters.append(''.join(f'[v{i}]' for i in range(len(segments)))+f'concat=n={len(segments)}:v=1:a=0[out]')
subprocess.run(['ffmpeg','-nostdin','-v','error','-y','-i',str(D/'continuous.mp4'),'-filter_complex',';'.join(filters),'-map','[out]','-an','-c:v','libx264','-threads','2','-preset','fast','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(N/'review/third-person-rebound-preview.mp4')],check=True)
canvas=Image.new('RGB',(1080,1320),'#161b20');draw=ImageDraw.Draw(canvas)
for row,(label,title) in enumerate(zip([labels[0],labels[1],labels[2],labels[4]],['Sword','Sword and shield','Axe','Staff'])):
 for col,kind in enumerate(['contact','recoil','recovery']):
  im=Image.open(D/(label+'-'+kind+'.bmp')).convert('RGB');im=im.crop((390,240,870,660));im.thumbnail((360,300));x=col*360;y=row*330;canvas.paste(im,(x,y+26));draw.text((x+9,y+6),title+' - '+kind,fill='white')
canvas.save(N/'review/third-person-review.png')
(N/'review/REEL.json').write_text(json.dumps(dict(binary_sha256=execution['binary_sha256'],source='duel-film/continuous.mp4',fps=60,actual_game_renderer=True,audio=False,frames=sum(s['end_frame_exclusive']-s['first_frame'] for s in segments),segments=segments),indent=2)+'\n')
def recoil_steps(file):
 a=[json.loads(s) for s in open(file)];out={};p=None
 for r in a:
  q=p;p=r
  if q and r['label']==q['label'] and r['weapon']==q['weapon'] and r['attack_serial']==q['attack_serial'] and r['steel_recoil']>0 and q['steel_recoil']>0:
   distance=max(math.dist(r['blade'][:3],q['blade'][:3]),math.dist(r['blade'][3:],q['blade'][3:]));out[r['label']]=max(out.get(r['label'],0),distance)
 return out
b=recoil_steps(N/'review/duel-film-before-rebound/ANIMATION_FRAMES.jsonl');a=recoil_steps(D/'ANIMATION_FRAMES.jsonl');comparison=[dict(case=k,before_m=b[k],after_m=a[k],reduction_percent=100*(1-a[k]/b[k])) for k in a]
world=execution['world'];passed=all(x['reduction_percent']>10 for x in comparison) and world['gl_error']==0 and world['skeletal_hero_contact']['failures']==0 and world['source_camera_collision']['query_failures']==0 and world['arena_camera_policy']['unsafe_frames']==0
(N/'review/NATIVE_CONTACT_REVIEW.json').write_text(json.dumps(dict(passed=passed,binary_sha256=execution['binary_sha256'],native_contact_cases=execution['clash']['reviewed_cases'],fresh_attacks=execution['raw']['fresh_attacks_completed'],event_injection=False,real_source_geometry=True,camera_query_failures=world['source_camera_collision']['query_failures'],unsafe_camera_frames=world['arena_camera_policy']['unsafe_frames'],contact_query_failures=world['skeletal_hero_contact']['failures'],gl_error=world['gl_error'],rebound_comparison=comparison,live_saves_used=False,hardware_fps_certified=False),indent=2)+'\n')
print('Native review',passed,'comparisons',len(comparison),'reel seconds',sum(s['end_frame_exclusive']-s['first_frame'] for s in segments)/60)
