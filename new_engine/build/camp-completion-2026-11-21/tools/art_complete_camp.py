"""Owned art overlay. Terrain supports use production sampler, original assets unchanged."""
from pathlib import Path
import json, hashlib, struct, sys, importlib.util, math, shlex, copy, collections
import numpy as np
from PIL import Image
N=Path(__file__).resolve().parents[1];E=N.parents[1];P=N/'world-project';R=N/'review/art';A=P/'models/art_completion';A.mkdir(exist_ok=True)
sys.path.insert(0,str(E/'tools'))
from build_opening_prologue import Glb,read_glb,array

def module(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
st=module('art_ground',E/'build/woodland-detail-2026-11-19/tools/build_forest_layers.py');st.N=N;st.P=P;ground,helper=st.sampler()
geo=module('art_geometry',E/'build/woodland-detail-2026-11-19/tools/review_forest_layout.py')
mesh=module('art_mesh',E/'docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py')
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
rng=np.random.default_rng(2026112109);W=100800.;D=W*2818/5009;camp=np.array([-11988.,5172.]);maps=E/'assets/polyhaven/castle_village';leaves=E/'assets/polyhaven/visual_upgrade/forest_leaves_03'
assert not(R/'ART_PATCH.json').exists()
old={p.name:p.read_text().splitlines()for p in(P/'scenery').glob('*.txt')};rows=copy.deepcopy(old);studio=(P/'world.studio').read_text();defs={q[1]:q for s in studio.splitlines()if(q:=shlex.split(s))and q[0]=='asset'}
paths=json.loads((P/'opening/first-journey.routes.json').read_text())['paths'];segments=[]
for points in paths.values():
 q=np.asarray(points)[:,[0,2]];segments.extend(zip(q[:-1],q[1:]))
traffic=np.asarray([[float(q[1]),float(q[3])]for s in(P/'opening/road-traffic.settings').read_text().splitlines()if(q:=s.split())and q[0]=='point']);segments.extend(zip(traffic[:-1],traffic[1:]));segments=np.asarray(segments)
actors=np.array([[-11990.15,5171.35],[-11985.8,5171.35],[-11987.9,5168.8]])
creek=np.array([[-11990,5140],[-12010,5148],[-12018,5162],[-12018,5180],[-12022,5193],[-12028,5202],[-12078,5292]])
creekseg=np.array(list(zip(creek[:-1],creek[1:])))

def encode(alias,p,wind=False):
 st.native_scale(p)
 q=[alias,f'{p["x"]/W+.5:.13f}',f'{p["z"]/D+.5:.13f}',f'{p["yaw_deg"]:.7f}',f'{p["scale"]:.9f}','0','0','0',f'{p["pitch_deg"]:.7f}',f'{p["roll_deg"]:.7f}',*[f'{p[k]:.9f}'for k in ['sx','sy','sz']],f'{p["y"]:.9f}']
 assert len(' '.join(q))<=256
 if wind:q+=['wind','1','1']
 return ' '.join(q)
def write_glb(path,d,b):
 d['buffers'][0]['byteLength']=len(b);j=json.dumps(d,separators=(',',':')).encode();j+=b' '*(-len(j)%4);b=bytes(b);b+=b'\0'*(-len(b)%4);path.write_bytes(struct.pack('<4sII',b'glTF',2,28+len(j)+len(b))+struct.pack('<I4s',len(j),b'JSON')+j+struct.pack('<I4s',len(b),b'BIN\0')+b)

# Purchased source tree forms: preserve certified near/MID geometry exactly,
# add actual two-view rendered far cards. Existing collision aliases unchanged.
aliaschanges=[];assetrecords=[]
for item in json.loads((R/'cards/BAKE.json').read_text()):
 name=item['name'];stem='art_completion/tree_'+name;source='camp_variants/camp_variant_'+name
 for lod in ['near','mid']:
  target=A/f'tree_{name}_{lod}.glb';target.symlink_to((P/'models'/(source+'_'+lod+'.glb')).resolve())
 g=Glb();ys=[float(item['minimum'][2]),float(item['maximum'][2])];height=item['ortho_height'];width=item['ortho_width'];center=(ys[0]+ys[1])/2
 for view in range(2):
  mi=g.material('Source-rendered '+name+' diffuse silhouette '+str(view),[1,1,1],1,base=R/f'cards/{name}-{view}.png');g.doc['materials'][mi]['alphaMode']='MASK';g.doc['materials'][mi]['alphaCutoff']=.38;g.doc['materials'][mi]['doubleSided']=True
  # Blender camera centres original source bounds. Recover its X/Z centre;
  # do not pretend every purchased tree grew perfectly around its origin.
  cx=(item['minimum'][0]+item['maximum'][0])/2;cz=-(item['minimum'][1]+item['maximum'][1])/2
  p=np.array([[-width/2,center-height/2,0],[width/2,center-height/2,0],[width/2,center+height/2,0],[-width/2,center+height/2,0]])
  p=p@mesh.rotation(view*90).T+np.array([cx,0,cz]);norm=np.tile([0,0,1],(4,1))@mesh.rotation(view*90).T;g.mesh(p,norm,[[0,1],[1,1],[1,0],[0,0]],[0,1,2,0,2,3],mi)
 # Source lower pivot must match near/MID, even when orthographic framing has padding.
 datum=g.material('Source_Pivot_Datum',[0,0,0],1);g.mesh([[0,0,0]]*3,[[0,1,0]]*3,[[0,0]]*3,[0,1,2],datum)
 target=A/f'tree_{name}_far.glb';target.write_bytes(g.finish());a='nm_camp_variant_'+name;before=' '.join(defs[a]);after=f'asset {a} {stem} 3 3 2 900 70 1';aliaschanges.append({'before':before,'after':after});assetrecords.append(dict(alias=a,source=(P/'models'/(source+'_near.glb')).as_posix(),near_sha256=sha(P/'models'/(source+'_near.glb')),far_sha256=sha(target),near_triangles=geo.geometry(P/'models'/(source+'_near.glb'))[1],far_triangles=4,views=2,description='Actual purchased source silhouette; neutral diffuse bake relit by native sky/weather'))

# Additional authentic scanned tree families already licensed CC0 and cooked.
# Their original three-stage LODs/material maps are reused, avoiding duplicate textures.
family={'pine':'cinematic_pine_near','fir':'cinematic_fir_near','broadleaf':'cinematic_broadleaf_near','pine_young':'cinematic_pine_young_near'}
# Derive an actual bare snag by retaining only the photographed pine's woody structure.
for lod in ['near','mid']:
 src=P/'models'/f'cinematic_pine_{lod}.glb';d,b=read_glb(src);d=copy.deepcopy(d);kept=0
 for m in d['meshes']:
  m['primitives']=[pr for pr in m['primitives']if d['materials'][pr.get('material',0)].get('alphaMode')!='MASK'];kept+=len(m['primitives'])
 assert kept>=2;dst=A/f'pine_snag_{lod}.glb';write_glb(dst,d,b)
# Third LOD is still the woody low mesh, never the live tree's foliage card.
(A/'pine_snag_far.glb').symlink_to(A/'pine_snag_mid.glb');aliaschanges.append({'before':None,'after':'asset nm_camp_pine_snag art_completion/pine_snag 3 3 2 650 70 1'});family['nm_camp_pine_snag']='art_completion/pine_snag_near'
cache={a:geo.geometry(P/'models'/(s+'.glb'))[0]for a,s in family.items()};changes=[];selected=set()
candidates=[]
for f,ss in rows.items():
 for i,line in enumerate(ss):
  q=line.split()
  if len(q)<14 or q[0]not in ['nm_city_region_oak','nm_city_region_beech']:continue
  p=st.decode(q);pt=np.array([p['x'],p['z']]);r=np.linalg.norm(pt-camp)
  if 18<r<160 and geo.dist(pt,segments)>6 and geo.dist(pt,creekseg)>11:candidates.append((f,i,line,p))
# Mixed clumps, not a repeating pine/oak checkerboard. Young trees favour glades;
# standing deadwood and leaning trees remain well away from travel lanes.
anchors=[(-12012,5157),(-11952,5150),(-11950,5187),(-12017,5207),(-11925,5222),(-12056,5167),(-11980,5115),(-11936,5146),(-12023,5261),(-11901,5177),(-11884,5260),(-12077,5205),(-11883,5151),(-12055,5104),(-11939,5083),(-11900,5123),(-12030,5282),(-11880,5210),(-11918,5255),(-12076,5249),(-11915,5070),(-12098,5173),(-11920,5301),(-11851,5178),(-12088,5125),(-12089,5277),(-11860,5247),(-11970,5051)]
sequence=['fir','pine','pine_young','broadleaf','pine','fir','nm_camp_pine_snag','pine_young','fir','broadleaf','pine','nm_camp_pine_snag','pine_young','fir','pine','broadleaf','pine','fir','pine_young','nm_camp_pine_snag','broadleaf','pine','fir','pine_young','nm_camp_pine_snag','fir','pine','broadleaf']
for j,(anchor,a)in enumerate(zip(anchors,sequence)):
 g=cache[a];choice=None
 for f,i,line,oldp in sorted(candidates,key=lambda x:np.linalg.norm(np.array([x[3]['x'],x[3]['z']])-anchor)):
  if(f,i)in selected:continue
  pt=np.array([oldp['x'],oldp['z']]);height=(6+rng.uniform(-1.1,1.3))if a in ['broadleaf','pine_young']else(12+rng.uniform(-2,3)if a=='nm_camp_pine_snag'else 17+rng.uniform(-2,4));scale=float(height/np.ptp(g[:,1]));p=dict(x=oldp['x'],z=oldp['z'],yaw_deg=oldp['yaw_deg'],pitch_deg=0.,roll_deg=0.,scale=scale,sx=1.,sy=1.,sz=1.)
  if j%7==4:p['roll_deg']=float(rng.choice([-1,1])*rng.uniform(5,9))
  st.native_scale(p);mesh.support(p,g,ground,.055);v=(g*np.array([p['sx'],p['sy'],p['sz']])*p['scale'])@mesh.rotation(p['yaw_deg'],p['pitch_deg'],p['roll_deg']).T;low=v[v[:,1]<v[:,1].min()+2.4];radius=float(np.linalg.norm(low[:,[0,2]],axis=1).max());clear=geo.dist(pt,segments)-radius
  basal=v[v[:,1]<v[:,1].min()+max(.025,height*.02)];basal=basal[np.linspace(0,len(basal)-1,min(48,len(basal)),dtype=int)];gaps=np.array([p['y']+b[1]-ground(*(pt+b[[0,2]]))for b in basal])
  if clear<3.6 or geo.dist(pt,creekseg)-radius<7.2 or gaps.max()>.16 or gaps.min()<-.45:continue
  after=encode(a,p,True);rows[f][i]=after;selected.add((f,i));changes.append(dict(file='scenery/'+f,line=i+1,before=line,after=after,kind='authentic_tree_family_replace',family=a,height_m=height,lean_degrees=p['roll_deg'],route_clearance_m=clear,support_min_m=float(gaps.min()),support_max_m=float(gaps.max())));choice=True;break
 if not choice:print('No safe tree slot',a,anchor)

# Batch fine solid details in small spatial cells: no alpha overdraw, no large
# scene-wide compound bounds, no colliders on sub-20 cm floor dressing.
class DetailGlb(Glb):
 def texture(self,raw,size=512):return super().texture(raw,min(size,512))

def beam(g,a,b,r,mi,segments=6):
 a=np.array(a);b=np.array(b);axis=b-a;axis/=np.linalg.norm(axis);u=np.cross(axis,[0,1,0]if abs(axis[1])<.85 else[1,0,0]);u/=np.linalg.norm(u);v=np.cross(axis,u);p=[];n=[];uv=[];ix=[]
 for j,(center,rad)in enumerate([(a,r),(b,r*.58)]):
  for k in range(segments):
   nn=u*math.cos(k*math.tau/segments)+v*math.sin(k*math.tau/segments);p.append(center+nn*rad);n.append(nn);uv.append([k/segments,np.linalg.norm(b-a)*j/.22])
 for k in range(segments):
  l=(k+1)%segments;ix +=[k,l,segments+l,k,segments+l,segments+k]
 g.mesh(p,n,uv,ix,mi)

def sphere(g,c,axes,mi,seed):
 r=np.random.default_rng(seed);p=[]
 for y,rr in [(0,1),(.62,.65),(-.5,.7)]:
  for j in range(6):
   a=j*math.tau/6;p.append(np.asarray(c)+np.asarray(axes)*[math.cos(a)*rr*(1+r.uniform(-.18,.18)),y,math.sin(a)*rr])
 p=np.array(p);ix=[]
 for i in range(6):
  j=(i+1)%6;ix +=[i,j,6+j,i,6+j,6+i,i,12+i,12+j,i,12+j,j]
 ix +=[6,7,8,6,8,9,6,9,10,6,10,11]
 n=(p-c)/np.maximum(np.array(axes),1e-4);n/=np.maximum(np.linalg.norm(n,axis=1,keepdims=True),1e-8);g.mesh(p,n,p[:,[0,2]]*1.8,ix,mi)

def allowed(pt,radius=.2):
 return geo.dist(pt,segments)-radius>.7 and geo.dist(pt,creekseg)-radius>6 and np.linalg.norm(pt-camp)>2.2+radius and np.linalg.norm(pt-actors,axis=1).min()>1.35+radius and not(-11993-radius<pt[0]<-11982+radius and 5162-radius<pt[1]<5171+radius)

clusteranchors=[[-11998,5178],[-11977,5170],[-11982,5189],[-11962,5193],[-12001,5152],[-11965,5151],[-11957,5219],[-11977,5247],[-11939,5256],[-11952,5280],[-11915,5317],[-11942,5353],[-11913,5393],[-11935,5434],[-11917,5470]]
placements=[];detailstats=[]
for ci,anchor in enumerate(clusteranchors):
 pieces=[]
 for j in range(58):
  pt=np.array(anchor)+rng.uniform(-3.0,3.0,2);kind=['leaf','leaf','pebble','twig','moss','root'][j%6];radius={'leaf':.14,'pebble':.15,'twig':.52,'moss':.28,'root':.7}[kind]
  if not allowed(pt,radius):continue
  h=ground(*pt);around=[ground(*(pt+d))for d in [[.4,0],[-.4,0],[0,.4],[0,-.4]]]
  if max(around)-min(around)>.40:continue
  pieces.append(dict(kind=kind,pt=pt,h=h,seed=ci*1000+j,yaw=float(rng.uniform(-math.pi,math.pi))))
 if len(pieces)<6:continue
 origin=np.array([anchor[0],min(p['h']for p in pieces)-.45,anchor[1]])
 for lod in ['near','mid']:
  g=DetailGlb();wood=g.material('Weathered windfall wood',[.46,.40,.30],.98,base=maps/'wood_base.png',normal=maps/'wood_normal.png',arm=maps/'wood_arm.png');stone=g.material('Small weathered trail stones',[.51,.51,.44],1,base=maps/'stone_base.png',normal=maps/'stone_normal.png',arm=maps/'stone_arm.png');leaf=g.material('Dry curled woodland litter',[.70,.67,.53],1,base=leaves/'diff.png',normal=leaves/'nor_gl.png',arm=leaves/'arm.png');moss=g.material('Soft irregular moss cushions',[.26,.35,.17],1,base=maps/'stone_base.png',normal=maps/'stone_normal.png',arm=maps/'stone_arm.png')
  counted=collections.Counter()
  for j,p in enumerate(pieces):
   kind=p['kind'];pt=p['pt'];x,z=pt-origin[[0,2]];h=p['h']-origin[1];a=p['yaw'];d=np.array([math.cos(a),0,math.sin(a)]);c=np.array([x,h+.025,z]);counted[kind]+=1
   if lod=='mid'and j%2 and kind in ['leaf','pebble']:continue
   if kind in ['pebble','moss']:sphere(g,c,[.06 if kind=='pebble'else .20,.045 if kind=='pebble'else .035,.08 if kind=='pebble'else .16],stone if kind=='pebble'else moss,p['seed'])
   elif kind in ['twig','root']:
    length=.27 if kind=='twig'else .46;r=.012 if kind=='twig'else .025
    aa=pt+d[[0,2]]*length;bb=pt-d[[0,2]]*length;a3=np.array([aa[0]-origin[0],ground(*aa)-origin[1]+r*.5,aa[1]-origin[2]]);b3=np.array([bb[0]-origin[0],ground(*bb)-origin[1]+r*.5,bb[1]-origin[2]])
    beam(g,a3,b3,r,wood)
    if kind=='root':
     for k in [-1,1]:
      end=pt+np.array([math.cos(a+k*.7),math.sin(a+k*.7)])*.58;ep=np.array([end[0]-origin[0],ground(*end)-origin[1]+.006,end[1]-origin[2]]);beam(g,c+[0,.012,0],ep,.018,wood)
   else:
    length=.10;width=.05;v=[]
    for t,side in [(-1,-1),(-1,1),(0,-1),(0,1),(1,-1),(1,1)]:
     offset=np.array([math.cos(a)*length*t-math.sin(a)*width*side,math.sin(a)*length*t+math.cos(a)*width*side]);world=pt+offset;v.append([world[0]-origin[0],ground(*world)-origin[1]+.006+.013*abs(t),world[1]-origin[2]])
    g.mesh(v,[[0,1,0]]*6,[[0,0],[0,1],[.5,0],[.5,1],[1,0],[1,1]],[0,1,2,1,3,2,2,3,4,3,5,4],leaf)
  datum=g.material('Source_Pivot_Datum',[0,0,0],1);g.mesh([[0,0,0]]*3,[[0,1,0]]*3,[[0,0]]*3,[0,1,2],datum);path=A/f'floor_{ci}_{lod}.glb';path.write_bytes(g.finish())
  if lod=='near':detailstats.append(dict(cluster=ci,origin=origin.tolist(),pieces=dict(counted),triangles=sum(g.doc['accessors'][pr['indices']]['count']//3 for m in g.doc['meshes']for pr in m['primitives']),materials=len(g.doc['materials']),maximum_detail_height_m=.12,collision='None, below step height',sha256=sha(path)))
 alias=f'nm_camp_floor_{ci}';aliaschanges.append({'before':None,'after':f'asset {alias} art_completion/floor_{ci} 2 0 0 85 24 1'});p=dict(x=origin[0],y=origin[1],z=origin[2],yaw_deg=0.,pitch_deg=0.,roll_deg=0.,scale=1.,sx=1.,sy=1.,sz=1.);placements.append(dict(alias=alias,row=encode(alias,p),kind='batched_solid_floor_details',piece_count=len(pieces)))

# Fine ash/charcoal fuel at the actual flame socket, no proxy/collider change.
g=DetailGlb();ash=g.material('Matte loose cold ash',[.28,.26,.23],1,base=maps/'stone_base.png',normal=maps/'stone_normal.png');char=g.material('Burnt log charcoal',[.055,.040,.027],1,base=maps/'wood_base.png',normal=maps/'wood_normal.png');origin=np.array([-11988.,11.65,5172.])
for j in range(26):
 a=j*2.39996;r=.23+.025*(j%13);pt=camp+[math.cos(a)*r,math.sin(a)*r];h=ground(*pt);sphere(g,[pt[0]-origin[0],h-origin[1]+.025,pt[1]-origin[2]],[.025+.01*(j%3),.02,.035],ash if j%3 else char,11000+j)
datum=g.material('Source_Pivot_Datum',[0,0,0],1);g.mesh([[0,0,0]]*3,[[0,1,0]]*3,[[0,0]]*3,[0,1,2],datum);(A/'hearth_coals.glb').write_bytes(g.finish());(A/'hearth_coals_mid.glb').symlink_to(A/'hearth_coals.glb');aliaschanges.append({'before':None,'after':'asset nm_camp_hearth_coals art_completion/hearth_coals 2 0 0 75 20 1'});p=dict(x=origin[0],y=origin[1],z=origin[2],yaw_deg=0.,pitch_deg=0.,roll_deg=0.,scale=1.,sx=1.,sy=1.,sz=1.);placements.append(dict(alias='nm_camp_hearth_coals',row=encode('nm_camp_hearth_coals',p),kind='hearth_coal_fragments'))

# Weathered photograph-textured planks follow exact existing straight deck.
# Max +19 mm above unchanged source collision; never creates an unsupported span.
g=DetailGlb();wood=[]
for i,rgb in enumerate([[.44,.40,.32],[.51,.46,.36],[.37,.34,.28]]):wood.append(g.material('Silvered bridge plank '+str(i),rgb,.96,base=maps/'wood_base.png',normal=maps/'wood_normal.png',arm=maps/'wood_arm.png'))
metal=g.material('Oxidized recessed nail heads',[.08,.075,.065],.94);origin=np.array([-12071.,10.8,5314.]);n=168
for j in range(n):
 x=-21+(j+.5)*42/n;top=11.214+.004*math.sin(j*1.7);g.box([x,top-origin[1]-.02,0],[42/n-.008,.04,3.52],wood[j%3],tile=.35)
 if j%2==0:
  for z in [-1.52,1.52]:g.box([x+.055,top-origin[1]+.002,z],[.016,.003,.016],metal)
datum=g.material('Source_Pivot_Datum',[0,0,0],1);g.mesh([[0,0,0]]*3,[[0,1,0]]*3,[[0,0]]*3,[0,1,2],datum);(A/'bridge_planks.glb').write_bytes(g.finish());(A/'bridge_planks_mid.glb').symlink_to(A/'bridge_planks.glb');aliaschanges.append({'before':None,'after':'asset nm_camp_bridge_planks art_completion/bridge_planks 2 0 0 180 35 1'});p=dict(x=origin[0],y=origin[1],z=origin[2],yaw_deg=0.,pitch_deg=0.,roll_deg=0.,scale=1.,sx=1.,sy=1.,sz=1.);placements.append(dict(alias='nm_camp_bridge_planks',row=encode('nm_camp_bridge_planks',p),kind='weathered_deck_overlay',maximum_deck_collision_delta_m=.019,existing_bridge_id=2))
# Append into a loaded standard scenery file; new ad-hoc files are not assumed loaded.
rows['authored_instances.txt']+=['# Camp completion: tiny batched floor dressing and source-weathered bridge.']+[p['row']for p in placements]
for name,ss in rows.items():
 if ss!=old[name]:(R/('before-'+name)).write_text('\n'.join(old[name])+'\n');(P/'scenery'/name).write_text('\n'.join(ss)+'\n')
(R/'STUDIO_ART_PATCH.json').write_text(json.dumps(aliaschanges,indent=2)+'\n')
report=dict(passed=True,visual_acceptance_pending=True,changed_rows=changes,new_placements=placements,tree_count_change=0,new_detail_records=len(placements),tree_forms_inventory=['purchased oak02','purchased oak04','purchased beech01','purchased beech02','scanned Scots pine','scanned fir','scanned young pine','scanned small broadleaf (species unspecified)','bare pine snag','leaning scanned pine/fir variants'],species_honesty='Only oak, beech, pine and fir are identified; forms/sizes are not claimed as new botanical species.',tree_family_replacements=dict(collections.Counter(c['family']for c in changes)),detail_batches=detailstats,far_cards=assetrecords,alias_patches=str(R/'STUDIO_ART_PATCH.json'),source_ground_sampler=str(helper),source_ground_sampler_sha256=sha(helper),terrain_changed=False,walk_collision_changed=False,maximum_bridge_deck_offset_m=.019,source_and_live_assets_modified=False,inputs={str(P/'world.studio'):sha(P/'world.studio'),str(P/'ground/showcase.height'):sha(P/'ground/showcase.height'),str(P/'opening/first-journey.routes.json'):sha(P/'opening/first-journey.routes.json')},changed_scenery={name:dict(before_sha256=hashlib.sha256(('\n'.join(old[name])+'\n').encode()).hexdigest(),after_sha256=sha(P/'scenery'/name))for name in rows if rows[name]!=old[name]},limitations=['Full native camera/performance approval required before publication.','Adds solid geometry, not alpha cards, to ground; all small details are below step height.','This does not claim 6–10 new botanical species or a rebuilt entire distant terrain.','Campsite tent and roast use previously restored photographed canvas/supplied models; geometry remains unchanged.'])
(R/'ART_PATCH.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k]for k in ['passed','tree_family_replacements','new_detail_records','tree_count_change']}))
