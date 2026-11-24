"""Exact owned candidate row replacements; root merges aliases/catalog separately."""
from pathlib import Path
import json,hashlib,collections
N=Path(__file__).resolve().parents[1];P=N/'world-project';R=N/'review/art';patch=json.loads((R/'SCENERY_BIRCH_PATCH.json').read_text());assert patch['passed']and patch['staged_only'];groups=collections.defaultdict(list)
for change in patch['changes']:groups[change['file']].append(change)
pre={};post={}
for file,changes in groups.items():
 path=P/file;assert not path.is_symlink()and not path.parent.is_symlink();before=path.read_text();rows=before.splitlines();pre[file]=hashlib.sha256(before.encode()).hexdigest();(R/'birch'/('before-'+path.name)).write_text(before)
 for change in changes:
  i=change['line']-1;assert rows[i]==change['before'],(file,change['line'],'preimage mismatch');rows[i]=change['after']
 path.write_text('\n'.join(rows)+'\n');post[file]=hashlib.sha256(path.read_bytes()).hexdigest()
art=json.loads((R/'ART_PATCH.json').read_text());index={(c['file'],c['line']):c for c in patch['changes']}
for key in ['changed_rows','far_treeline_replacements']:
 for rec in art[key]:
  change=index.get((rec['file'],rec['line']))
  if not change:continue
  rec['before_birch']=change['before'];rec['after']=change['after'];rec['alias']=change['alias'];rec['soil_plane_glb_y']=change['soil'];rec['soil_height_error_m']=change['soil_height_error_m'];rec['height_above_soil_m']=change['height_above_soil_m']
  if'family'in rec:rec['family']=change['alias']
for file,sh in post.items():art['changed_scenery'][Path(file).name]['after_sha256']=sh
art['birch_count_neutral_replacements']={'changes':len(patch['changes']),'new_named_tree_types':1,'authentic_source_variants':2,'near':sum(c['kind']=='near_birch'for c in patch['changes']),'far':sum(c['kind']=='far_birch'for c in patch['changes']),'collision_catalog_merge_pending':True,'source_archive':'/home/computer/Downloads/village_3d_model(1).zip'};art['tree_forms_inventory']+=['purchased white-bark birch, young form','purchased white-bark birch, taller form'];art['species_honesty']='Five identified named types: oak, beech, pine, fir, birch. Additional forms/sizes and an unspecified small broadleaf are not claimed as new botanical species.';(R/'ART_PATCH.json').write_text(json.dumps(art,indent=2)+'\n');patch['staged_only']=False;patch['candidate_scenery_changed']=True;patch['applied_preimage_sha256']=pre;patch['applied_after_sha256']=post;(R/'SCENERY_BIRCH_PATCH.json').write_text(json.dumps(patch,indent=2)+'\n');print(json.dumps({'passed':True,'applied':len(patch['changes']),'tree_count_change':0,'world_studio_and_collision_catalog_untouched':True}))
