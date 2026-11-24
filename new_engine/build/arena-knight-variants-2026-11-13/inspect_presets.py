"""Read-only recommendations from existing exact Unity inspection reports.
This does not pretend to resolve nested Unity prefab inheritance in plain YAML.
"""
from pathlib import Path
import hashlib,json,struct,io
from PIL import Image
ROOT=Path(__file__).resolve().parents[3]
PACK=ROOT/'new_engine/assets/purchased/modular_knights'
OUT=Path(__file__).resolve().parent
base=json.loads((PACK/'prefab_inspection.json').read_text())
parts={x['mesh_name']:x for x in base['skinned_pieces'] if x.get('is_lod0')}
assert len(parts)==196
PREF='Assets/_TheTalesFactory/Modular Knights/Models/Prefabs/'
MAT='Assets/_TheTalesFactory/Modular Knights/Textures/'
def selected(names, finish=None, overrides=None):
 out=[]
 for n in names:
  x=parts[n]; mats=list(x['material_paths'])
  if finish:
   repl=[]
   for m in mats:
    mp=Path(m); candidate=str(mp.with_name(mp.stem+'_'+finish+mp.suffix))
    repl.append(candidate if (PACK/'source'/candidate).exists() else m)
   mats=repl
  if overrides and n in overrides:mats=[MAT+overrides[n]]
  assert all((PACK/'source'/p).exists() for p in mats),(n,mats)
  out.append({'mesh_name':n,'material_paths':mats,'role':'body'})
 return out

def publisher(n):
 d=json.loads((PACK/f'variant{n:02}_inspection.json').read_text())
 removed=('KnightSword_LOD0','KnightShield01_LOD0','KnightShield02_LOD0','Crest02_LOD0','Feather_Armet01_LOD0','Labry03_LOD0','Labry04_LOD0','Labry_Pearl_LOD0','Lambrequin_LOD0')
 return [{'mesh_name':x['mesh_name'],'material_paths':x['material_paths'],'role':'body'} for x in d['skinned_pieces'] if x.get('active_lod0') and x['mesh_name'] not in removed]
base03=[x['mesh_name'] for x in publisher(9) if 'Armet' not in x['mesh_name']]
base02=['Gloves_LOD0','KnightDoublet_ArmOnly_LOD0','KnightHose_ThighOnly_LOD0','RidingBoots_FeetOnly_LOD0','Armor02_Forearm_LOD0','Armor02_Couter01_LOD0','Armor02_Spauldron01_LOD0','Armor02_Bevor02_GorgetOnly_LOD0','Armor02_Cuirass02_LOD0','Armor02_Gauntlets01_LOD0','Armor02_Greaves_LOD0','Armor02_Knees01_LOD0','Armor02_Legs_LOD0','Armor02_Sabaton_LOD0','KnightBelt_LOD0']
base01=[x['mesh_name'] for x in publisher(2) if not any(w in x['mesh_name'] for w in ('KnightGreatHelm','GreatHelm_ChainMail'))]
rows=[
 {'id':'decorated-great-helm','description':'Decorated Armor01 great helm and publisher heraldry08; broad closed-helm silhouette.','prefab':PREF+'Variants/Modular Knight_Prefab Variant 02.prefab','derivation':'Publisher Variant02 with decorative props, shield and sword removed.','pieces':publisher(2)},
 {'id':'rusted-armet','description':'Weathered Armor03 Armet01 with split visor, publisher coat06.','prefab':PREF+'Variants/Modular Knight_Prefab Variant 09.prefab','derivation':'Publisher Variant09 with decorative props, shield and sword removed.','pieces':publisher(9)},
 {'id':'blackened-armet','description':'Blackened Armor03 with distinct Armet02 helmet, clean readable heraldry03.','prefab':PREF+'Modular Knight_Prefab.prefab','derivation':'Custom coherent Armor03 assembly; fitting must be checked in actual deformation.','pieces':selected(base03+['Armor03_Armet02_LOD0','Armor03_Armet02_Bevor_LOD0','Armor03_Armet02_Visor_LOD0'],'Blackened',{'CoatOfArms03_LOD0':'Clothes/CoatOfArms03/CoatOfArms03_03.mat'})},
 {'id':'steel-sallet','description':'Plain Armor02 with Sallet01 and characteristic neck protection.','prefab':PREF+'Modular Knight_Prefab.prefab','derivation':'Custom coherent Armor02 assembly; fitting must be checked in actual deformation.','pieces':selected(base02+['Armor02_Sallet01_LOD0'])},
 {'id':'kettle-hat','description':'Armor01 with open kettle hat and chainmail aventail; distinct infantry silhouette.','prefab':PREF+'Modular Knight_Prefab.prefab','derivation':'Variant02 body recipe with plain metal finish and kettle hat; fitting must be checked.','pieces':selected(base01+['KnightKettleHat_LOD0','Armor01_KettleHat_ChainMail_LOD0'],None,{'CoatOfArm01_UnderArmor_LOD0':'Clothes/CoatOfArms01/CoatOfArms01_03.mat'})},
 {'id':'bareheaded-veteran','description':'Armor03, visible Head03 and source beard02; distinct from young clean-shaven hero.','prefab':PREF+'Modular Knight_Prefab.prefab','derivation':'Custom assembly; scalp coverage, head fit and material slots must be checked.','pieces':selected(base03+['KnightHead03_LOD0','Hair03_LOD0','Head03_Beard02_LOD0'],'Patina',{'CoatOfArms03_LOD0':'Clothes/CoatOfArms03/CoatOfArms03_05.mat'})},
]
for x in rows:
 x['estimated_lod0_vertices']=sum(parts[p['mesh_name']]['vertex_count'] for p in x['pieces'])
 x['estimated_lod0_triangles']=sum(parts[p['mesh_name']]['triangle_count'] for p in x['pieces'])
 x['source_geometry_exported']=False
 x['visual_fit_accepted']=False
 (OUT/(x['id']+'.selection.json')).write_text(json.dumps(x,indent=2)+'\n')
asset=PACK/'derived/polish-v4.1/modular_knight_hero_v4_1.glb';b=asset.read_bytes();n,t=struct.unpack_from('<II',b,12);d=json.loads(b[20:20+n]);off=20+n;bn,bt=struct.unpack_from('<II',b,off);bb=b[off+8:off+8+bn];dims={};memory=0
for im in d['images']:
 v=d['bufferViews'][im['bufferView']];p=Image.open(io.BytesIO(bb[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]));k=f'{p.width}x{p.height}';dims[k]=dims.get(k,0)+1;memory+=p.width*p.height*4*4/3
report={'schema':'valwood.arena-knight-variant-intake.v1','source_modified':False,'accepted_builds_modified':False,'source_fbx':str(PACK/'source/Assets/_TheTalesFactory/Modular Knights/Models/ModularKnight_Complete.fbx'),'publisher_prefab_count':18,'audited_modular_lod0_pieces':196,'recommendations':rows,'shared_rig':{'target':str(asset),'target_sha256':hashlib.sha256(b).hexdigest(),'target_palette_joints':len(d['skins'][0]['joints']),'target_animation_clips':len(d['animations']),'reuse':'Reuse the target named skeleton and sampled poses, bind each new purchased mesh by named bones with original four weights. valwood_weapon_r is the derivative-only weapon attachment. Verify rest bone matrices, right hand socket and head/limb sever groups after conversion.','warning':'Existing legacy NPC geometry is pose-baked. Renaming a hero GLB as a legacy enemy model does not preserve its vertex-animation, blade/grip streams or dismemberment contracts.'},'material_memory':{'current_single_hero_images':len(d['images']),'current_dimensions':dims,'current_rgba8_with_full_mips_mib':round(memory/2**20,2),'six_independent_sets_mib':round(memory*6/2**20,2),'strategy':'Deduplicate shared armor, cloth, skin, ORM and normal images by canonical source/content hash. Prepare one shared 1024 armor map set per necessary armor family, 512 secondary maps, 1024 close helmet map. Share body geometry and animation data across all combatants of each preset; retain only per-enemy pose/state. Use source LOD1 or LOD2 for nonfocused fighters after checking source topology and silhouette. Keep source sets read-only.'},'tools':{'unity_geometry_exporter':str(ROOT/'new_engine/build/modular-knights-2026-11-13/inspection-project/Assets/Editor/ModularKnightsGeometry.cs'),'cook':str(ROOT/'new_engine/tools/cook_modular_knight.py'),'materials':str(ROOT/'new_engine/tools/prepare_modular_knight_materials.py'),'readiness':'Recipes only; no new runtime asset, fit acceptance or GPU benchmark is claimed.'}}
(OUT/'ASSET_RECOMMENDATIONS.json').write_text(json.dumps(report,indent=2)+'\n')
# Earlier exploratory plain-YAML inheritance report was intentionally rejected.
old=OUT/'PUBLISHER_PRESETS.json'
if old.exists():old.unlink()
print(json.dumps({'report':str(OUT/'ASSET_RECOMMENDATIONS.json'),'presets':[(x['id'],x['estimated_lod0_triangles']) for x in rows],'single_rgba8_mips_mib':report['material_memory']['current_rgba8_with_full_mips_mib']},indent=2))
