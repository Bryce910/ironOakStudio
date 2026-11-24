"""Concentrate the art standard on the court and first 40m of forest path."""
from pathlib import Path
import sys,json,math,random,tempfile,ctypes as C,shutil
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from compose_visual_upgrade import rows,line,FILES,TRAIL
from build_starting_area import Atlas,segment_distance
P=ROOT/'projects/valwood_visual';R=ROOT/'docs/milestone/2026-10-30'

MILESTONE_VIEWS='    {"castle-milestone","Valdoa | The watch courtyard",.5-1342./100800.,.5-10284.5/(100800.*2818./5009.),2.635f,-.03f,1.75f},\n    {"rainforest-milestone","Emerald Wood | The shaded path",.5-26737./100800.,.5+9736./(100800.*2818./5009.),1.2f,-.03f,1.75f},\n    {"castle-hero-portrait","Valdoa | The watch knight",.5-1348./100800.,.5-10281./(100800.*2818./5009.),0.f,0.f,1.75f},'

def main():
 views=ROOT/'engine/world_demo_finish_views.inc';text=views.read_text()
 if '"castle-milestone"'not in text:views.write_text(text.splitlines()[0]+'\n'+MILESTONE_VIEWS+'\n'+'\n'.join(text.splitlines()[1:])+'\n')
 # The original entry stump's measured 4.12x2.69m footprint intersected
 # the new opening approach. Keep its stable record, move it onto the bank,
 # and break this one proxy before writing so the shared world stays intact.
 entry=P/'scenery/rainforest_instances.txt';text=entry.read_text();lines=text.splitlines();matches=[i for i,l in enumerate(lines)if '# entry_exposedroots_detail_003'in l];assert len(matches)==1
 i=matches[0];fields,comment=lines[i].split('#',1);tokens=fields.split();assert tokens[0]=='showcase_roots';tokens[2]=f"{9730.8/(100800.*2818./5009.)+.5:.15f}";lines[i]=' '.join(tokens)+' #'+comment
 if entry.is_symlink():entry.unlink()
 entry.write_text('\n'.join(lines)+'\n')
 original,counts=rows();old=json.loads((ROOT/'docs/visual-upgrade/2026-10-30/composition.json').read_text());placed=[dict(x)for x in old['placements']if x['zone']in ['castle_facade','castle_wall']];rng=random.Random(1821994)
 # Place slit backs on the measured inner stone face, not the wall pivot.
 # The wall is 1.746m thick before its authored .49/.54 scale.
 for r in placed:
  if r['zone']!='castle_wall':continue
  parent=next(p for p in original if r['parent']==p['file']+':'+str(p['line']))
  if r['yaw']==90:r['x']=parent['x']+.87305641*parent['scale']+.04
  else:r['z']=parent['z']-.87305641*parent['scale']-.04
 for parent in original:
  if parent['file']=='castle_village_instances.txt'and parent['line']in[129,130,131]:
   placed.append(dict(alias='visual_window_slit',x=parent['x'],z=parent['z']-.87305641*parent['scale']-.04,yaw=180,scale=.85,burial=0,elevation=1.9395,zone='castle_wall',parent=parent['file']+':'+str(parent['line'])))
 path=TRAIL[:3];rejections=0
 def gap(x,z):return min(segment_distance((x,z),a,b)for a,b in zip(path,path[1:]))
 with tempfile.TemporaryDirectory(prefix='valwood-milestone-author-')as t:
  a=Atlas(t);a.api.world_atlas_local_water.argtypes=[C.c_double,C.c_double,C.POINTER(C.c_float),C.POINTER(C.c_float)];a.api.world_atlas_local_water.restype=C.c_bool
  def add(alias,x,z,scale=1,yaw=0,burial=0,elevation=0,zone='court'):
   placed.append(dict(alias=alias,x=x,z=z,scale=scale,yaw=yaw,burial=burial,elevation=elevation,zone=zone))
  for x,z,yaw in [(-1360,-10265.4,0),(-1338,-10265.4,0),(-1324,-10265.4,0),(-1372.8,-10289.5,90)]:add('milestone_bed',x,z,yaw=yaw)
  add('bep_structure_well',-1337,-10275.5,.8,22)
  add('bep_prop_bucket',-1335.7,-10276.3,.85,25)
  add('starting_bucket',-1338.2,-10275.2,.9,-17)
  add('village_banner',-1342,-10272.5,.98,180)
  add('village_banner',-1331,-10272.5,.98,180)
  add('bep_prop_bench',-1346,-10276,.95,180)
  # Selected court edges get useful seating/cargo; the duel and return lanes
  # remain empty. Every source placement retains its original stable key.
  for alias,x,z,scale,yaw in [('bep_prop_bench',-1321,-10290,.95,90),('purchased_barrel',-1323,-10284,.75,18),('purchased_crate',-1321.8,-10283.4,.65,-15)]:add(alias,x,z,scale,yaw)
  def plant(alias,x,z,scale=1,zone='path'):
   nonlocal rejections
   tree=alias in ['milestone_canopy','bep_nature_palm'];radius=.48*scale if tree else(.75*scale if alias=='milestone_leaf'else .9*scale)
   if gap(x,z)<(1.8+radius if tree else 1.15+radius*.4)or math.hypot(x+26688,z-9670.4967)<18:
    rejections+=1;return False
   samples=[a.sample(x+dx*radius,z+dz*radius)for dx,dz in [(0,0),(-.65,0),(.65,0),(0,-.65),(0,.65)]]
   if any(s.land<.7 for s in samples)or any(a.api.world_atlas_local_water(x+dx*radius,z+dz*radius,None,None)for dx,dz in [(0,0),(-.65,0),(.65,0),(0,-.65),(0,.65)]):rejections+=1;return False
   if max(s.height for s in samples)-min(s.height for s in samples)>(1.1 if tree else .65):rejections+=1;return False
   add(alias,x,z,scale,rng.uniform(-180,180),.04 if tree else .012,zone=zone);return True
  for x,z,scale in [(-26737,9728,.91),(-26725,9744,1.03),(-26719,9727,.94),(-26735,9714,1.12),(-26717,9709,1.04),(-26719,9696,.96)]:
   for dx,dz in [(0,0),(-1,0),(1,0),(0,1)]:
    if plant('milestone_canopy',x+dx,z+dz,scale,'canopy'):break
  for x,z in [(-26743,9729),(-26717,9720),(-26724,9700)]:plant('bep_nature_palm',x,z,2.4,'canopy')
  # Coherent plant islands are scattered along alternating banks, without
  # putting large cards across the centre of the worn walking path.
  for i in range(13):
   t=i/12;cx=-26727+17*t;cz=9735.4967-35*t
   for side in [-1,1]:
    for alias,count,base in [('visual_grass',1,.69),('milestone_leaf',1,.88),('bep_nature_fern',1,.93)]:
     for j in range(count):
      for attempt in range(15):
       x=cx+side*rng.uniform(2.4,5.3)+rng.uniform(-.8,.8);z=cz+rng.uniform(-2.3,2.3)
       if plant(alias,x,z,base*rng.uniform(.8,1.18),'path_understory'):break
  # Root tangles and low shrubs anchor the new broad canopies in the soil.
  for x,z in [(-26739,9729),(-26726,9741),(-26718,9725),(-26734,9716),(-26716,9708),(-26720,9698)]:
   for alias,scale in [('showcase_roots',.55),('showcase_shrub',.7)]:
    for attempt in range(12):
     if plant(alias,x+rng.uniform(-2.5,2.5),z+rng.uniform(-2.5,2.5),scale,'path_grounding'):break
  assert len(original)+len(placed)<=8192,(len(original),len(placed))
  file=P/'scenery/demo_completion_instances.txt';base=file.read_text().split('# Visual upgrade authored layer')[0].rstrip();file.write_text(base+'\n# Visual upgrade authored layer\n'+'\n'.join(line(x)for x in placed)+'\n')
  source=R/'before/projects/valwood_visual/world.studio';doc=source.read_text().replace('VALWOOD_STUDIO 2','VALWOOD_STUDIO 3');lines=[]
  for l in doc.splitlines():
   if l.startswith('look '):l='look .34 1.10 1.25 1.06 15.65 -35 .22 .02 .65 .26'
   if l.startswith('asset visual_grass '):l='asset visual_grass visual_upgrade/grass_patch 2 0 1 45 10 1'
   lines.append(l)
  lines += ['asset milestone_canopy visual_milestone/canopy 2 3 2 240 28 1','asset milestone_leaf visual_milestone/broadleaf 2 0 1 80 18 1','asset milestone_bed visual_milestone/court_bed 1 1 0 190 45 1']
  # Replace the dense row of placeholder archers with two court sentries.
  # These are normal editable population overrides; the instructor and duel
  # actors retain their original identities and combat bindings.
  for i in [8,9,10,11,72,73,74,75]:
   enabled=i in [8,72];x,z=(-1320,-10268)if i==8 else(-1344,-10268)
   lines.append(f'actor "Court sentry {i}" {i} -1 0 {int(enabled)} 0 1.35 2 180 1 {x} {z}')
  # This replaces the nearby thin procedural canopy with six carefully spaced
  # photographed crowns. The smooth mask fades back into the larger forest.
  lines += ['foliage -26725 9720 31 0.12 1','foliage -26719 9700 20 0.2 1']
  for x,y,z,size in [(-1360,24.01,-10265.4,7.1),(-1338,24.01,-10265.4,7.1),(-1324,24.01,-10265.4,7.1),(-1327.5,24.01,-10286.5,4.0)]:lines.append(f'decal {x} {y} {z} {size} .5 2.4 .075 .06 .04 0 .18 .92 0')
  lines += ['decal -1337 24.01 -10275.5 3.2 .5 3.0 .075 .07 .045 17 .38 .35 3','decal -1337 24.01 -10263.7 22 .5 1.4 .07 .10 .045 0 .42 .94 1','decal -1341 24.01 -10287 4.5 .5 7 .14 .125 .095 20 .16 .88 2']
  lines += ['light -1345.5 27.1 -10284.5 6 .8 .87 .93 1 0 -45 180 0','light -1351.2 27.6 -10278.4 5 .65 1 .82 .61 0 -45 180 0']
  (P/'world.studio').write_text('\n'.join(lines)+'\n')
  result=dict(base_records=len(original),new_records=len(placed),total=len(original)+len(placed),hard_cap=8192,clear_trunk_corridor_m=1.8,selected_path=path,placements=placed,rejected_unsafe_candidates=rejections,source_documents='Only projects/valwood_visual; previous package retained',source_overrides=[dict(record='rainforest_instances.txt:359',z=9730.8,reason='Measured root footprint cleared from opening path')])
  (R/'composition.json').write_text(json.dumps(result,indent=2)+'\n');(P/'milestone_standard.json').write_text(json.dumps(dict(version=1,focus=['court','short jungle path','current armored hero close-up'],hero_modern_outfit='Source model still required',source='docs/milestone/2026-10-30'),indent=2)+'\n')
  dst=P/'assets/visual_milestone';dst.mkdir(parents=True,exist_ok=True);shutil.copy2(R/'composition.json',dst/'composition.json')
  for name in ['island_tree_01','calathea_orbifolia_01','fabric_pattern_07','cobblestone_floor_08']:shutil.copy2(ROOT/'assets/polyhaven/milestone'/name/'provenance.json',dst/(name+'.provenance.json'))
  print('COMPOSED',len(placed),'focused placements;',result['total'],'source records',flush=True)
if __name__=='__main__':main()
