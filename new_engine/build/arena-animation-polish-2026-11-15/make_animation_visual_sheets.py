#!/usr/bin/env python3
"""Prepare actual-game frames for explicit visual review; never grants acceptance."""
from pathlib import Path
import hashlib,json,subprocess
from PIL import Image,ImageDraw
N=Path(__file__).resolve().parent;O=N/'animation-review';D=O/'native-cases';V=O/'visual-review'
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def sheet(items,path,cols=4):
 canvas=Image.new('RGB',(cols*320,205*((len(items)+cols-1)//cols)),(16,21,27));draw=ImageDraw.Draw(canvas)
 for i,(label,p) in enumerate(items):
  im=Image.open(p).convert('RGB');im.thumbnail((320,180));x=i%cols*320;y=i//cols*205;canvas.paste(im,(x,y+25));draw.text((x+5,y+6),label[:48],fill='white')
 canvas.save(path)
def main():
 native=json.loads((O/'ANIMATION_NATIVE_REVIEW.json').read_text());assert native['passed']
 rows=[json.loads(x) for x in (D/'ANIMATION_FRAMES.jsonl').read_text().splitlines()];V.mkdir(exist_ok=True)
 choices=[]
 for view,label,where in [('third','walk-forward',.5),('third','stop',1),('third','unarmed-run',.5),('third','guard-left',1),('first','guard-left',1),('first','guard-high',1),('first','guard-thrust',1),('first','guard-low',1)]:
  found=[r for r in rows if r['opener'] and r['view']==view and r['label']==label];assert found;choices.append((view+' '+label,found[round((len(found)-1)*where)]['video_frame']))
 for view in ['third','first']:
  found=[r for r in rows if r['opener'] and r['view']==view and r['label']=='bow-release'];assert found
  for j in [0,5,14,30]:choices.append((view+' bow return '+str(j),found[min(j,len(found)-1)]['video_frame']))
 movie=D/'ANIMATION_CONTINUOUS.mp4';selected=sorted(set(frame for _,frame in choices));expr='+'.join('eq(n\\,'+str(i)+')' for i in selected)
 subprocess.run(['/usr/bin/ffmpeg','-hide_banner','-loglevel','error','-i',str(movie),'-vf','select='+expr,'-vsync','0','-threads','1','-y',str(V/'actual-%03d.png')],check=True)
 mapping={frame:V/f'actual-{i+1:03d}.png' for i,frame in enumerate(selected)}
 items=[(label,mapping[frame]) for label,frame in choices];sheet(items,V/'ACTUAL_MOVEMENT_GUARDS_BOW.png')
 cases=[]
 for enemy in ['knight01','gnur']:
  for weapon,mode in [('sword','clash'),('shield','clash'),('axe','clash'),('staff','clash')]:
   for view in ['third','first']:
    p=D/f'{enemy}-{weapon}-{mode}-{view}-contact.png'
    if p.exists():cases.append((f'{enemy} {weapon} {view} steel',p))
 sheet(cases,V/'ACTUAL_MELEE_CONTACTS.png')
 outputs=[*mapping.values(),V/'ACTUAL_MOVEMENT_GUARDS_BOW.png',V/'ACTUAL_MELEE_CONTACTS.png']
 inputs={str(Path(__file__).resolve()):sha(__file__),str(movie):sha(movie),str(D/'ANIMATION_FRAMES.jsonl'):sha(D/'ANIMATION_FRAMES.jsonl')}
 (V/'SHEET_INPUTS.json').write_text(json.dumps({'inputs':inputs,'outputs':{str(p):sha(p) for p in outputs},'selected_frames':[{'label':label,'frame':frame} for label,frame in choices],'grants_visual_acceptance':False},indent=2)+'\n')
 print(V/'ACTUAL_MOVEMENT_GUARDS_BOW.png');print(V/'ACTUAL_MELEE_CONTACTS.png')
if __name__=='__main__':main()
