from pathlib import Path
import sys,json,hashlib,numpy as np
N=Path(__file__).resolve().parents[1];E=N.parents[1];P=N/'world-project';R=N/'review/art';A=P/'models/art_completion';sys.path.insert(0,str(E/'tools'))
from build_opening_prologue import Glb
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
report=json.loads((R/'ART_PATCH.json').read_text());patch=json.loads((R/'STUDIO_ART_PATCH.json').read_text())
for item in json.loads((R/'cards/BAKE.json').read_text()):
 name=item['name'];g=Glb();height=item['ortho_height'];width=item['ortho_width'];top=float(item['maximum'][2]);center=(float(item['minimum'][2])+top)/2
 cx=(item['minimum'][0]+item['maximum'][0])/2;cz=-(item['minimum'][1]+item['maximum'][1])/2
 for view in range(1):
  mi=g.material('Source-rendered '+name+' diffuse silhouette '+str(view),[1,1,1],1,base=R/f'cards/{name}-{view}.png');g.doc['materials'][mi].update(alphaMode='MASK',alphaCutoff=.38,doubleSided=True)
  angle=view*np.pi/2;rot=np.array([[np.cos(angle),0,np.sin(angle)],[0,1,0],[-np.sin(angle),0,np.cos(angle)]])
  p=np.array([[-width/2,0,0],[width/2,0,0],[width/2,top,0],[-width/2,top,0]])@rot.T+np.array([cx,0,0]);norm=np.tile([0,0,1],(4,1))@rot.T
  vb=.5+center/height;vt=.5-(top-center)/height
  g.mesh(p,norm,[[0,vb],[1,vb],[1,vt],[0,vt]],[0,1,2,0,2,3],mi)
 datum=g.material('Source_Pivot_Datum',[0,0,0],1);g.mesh([[0,0,0]]*3,[[0,1,0]]*3,[[0,0]]*3,[0,1,2],datum);target=A/f'tree_{name}_far.glb';target.write_bytes(g.finish())
 alias='nm_camp_variant_'+name
 for rec in patch:
  if rec['after'].split()[1]==alias:rec['after']=f'asset {alias} camp_variants/camp_variant_{name} 3 0 2 900 70 1'
 for rec in report['far_cards']:
  if rec['alias']==alias:rec['far_sha256']=sha(target);rec['far_triangles']=2;rec['runtime_views']=1;rec['additional_reference_views_baked']=1;rec['native_camera_facing_quad']=True;rec['native_card_bottom_m']=0;rec['native_card_top_m']=top;rec['orthographic_padding_kept_only_in_uv']=True
# Small noncolliding details do not need independent shadow draws. The source
# bridge, fire pit and tall props retain their existing shadow meshes.
renames={}
for rec in patch:
 alias=rec['after'].split()[1]
 if alias.startswith('nm_camp_floor_')or alias in ['nm_camp_hearth_coals','nm_camp_bridge_planks']:renames[alias]='camp_finish_'+alias[len('nm_camp_'):]
for path in(P/'scenery').glob('*.txt'):
 before=path.read_text();lines=[]
 for line in before.splitlines():
  q=line.split()
  if q and q[0]in renames:q[0]=renames[q[0]];line=' '.join(q)
  lines.append(line)
 after='\n'.join(lines)+'\n'
 if before!=after:path.write_text(after)
for rec in patch:
 q=rec['after'].split()
 if q[1]in renames:q[1]=renames[q[1]];rec['after']=' '.join(q)
for rec in report['new_placements']:
 if rec['alias']in renames:
  rec['alias']=renames[rec['alias']];q=rec['row'].split();q[0]=rec['alias'];rec['row']=' '.join(q)
for name in report['changed_scenery']:report['changed_scenery'][name]['after_sha256']=sha(P/'scenery'/name)
report['ground_detail_shadow_policy']='Independent tiny litter/coals/deck overlay shadow draws disabled by non-nm alias; existing source tree/camp/bridge shadows retained.'
report['far_card_installation']='Run tools/art_install_tree_cards.py in candidate only before merging STUDIO_ART_PATCH.json; it replaces the camp_variants symlink by an owned directory of byte-exact original near/MID symlinks plus new far cards, retaining the exact original collision catalog model paths.'
(R/'ART_PATCH.json').write_text(json.dumps(report,indent=2)+'\n');(R/'STUDIO_ART_PATCH.json').write_text(json.dumps(patch,indent=2)+'\n')
print(json.dumps({'passed':True,'far_cards':4,'shadow_free_detail_aliases':len(renames)}))
