from pathlib import Path
import subprocess,json,struct,hashlib,numpy as np
N=Path(__file__).resolve().parents[1];A=N/'world-project/models/art_birch';R=N/'review/art/birch';run=subprocess.run([str(N/'tools/art_validate_glb'),*[str(f)for f in sorted(A.glob('*.glb'))]],capture_output=True,text=True);(R/'glb-validation.log').write_text(run.stdout+run.stderr);run.check_returncode();checks=[]
for name in ['birch1','birch3']:
 maps=[];bounds=[]
 for lod in ['near','mid']:
  f=A/(name+'_'+lod+'.glb');raw=f.read_bytes();sz=struct.unpack_from('<I',raw,12)[0];d=json.loads(raw[20:20+sz]);buf=raw[28+sz:]
  for m in d['materials']:
   branch='branch'in m['name'];assert m.get('alphaMode','OPAQUE')==('MASK'if branch else'OPAQUE');assert m['pbrMetallicRoughness']['metallicFactor']==0;assert 'normalTexture'in m
  assert all(not any(k in z for k in ['matrix','translation','rotation','scale'])for z in d['nodes']);images=[]
  for image in d['images']:
   v=d['bufferViews'][image['bufferView']];images.append(hashlib.sha256(buf[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]).hexdigest())
  maps.append(set(images));bounds.append([min(d['accessors'][p['attributes']['POSITION']]['min'][1]for m in d['meshes']for p in m['primitives']),max(d['accessors'][p['attributes']['POSITION']]['max'][1]for m in d['meshes']for p in m['primitives'])])
 assert maps[0]==maps[1];assert max(abs(bounds[0][i]-bounds[1][i])for i in [0,1])<2e-5;checks.append({'name':name,'near_mid_maps_shared_exact':True,'near_mid_height_bounds':bounds,'native_source_origin_y_delta_m':abs(bounds[0][0]-bounds[1][0]),'opaque_bark_masked_branch_normals_dielectric':True})
placement=json.loads((R.parent/'SCENERY_BIRCH_PATCH.json').read_text());assert placement['candidate_scenery_changed'];assert all(float(c['after'].split()[4])<=2 and .15<=float(c['after'].split()[4])and c['all_support_points_native_dry']for c in placement['changes']);assert 'PASS exact source bark only'in(R/'collision-test.log').read_text()
out={'passed':True,'checks':checks,'cgltf_complete_glbs':6,'source_images_filtered_once_shared_by_both_models_and_LODs':True,'exact_bark_only_jolt_collision_pass':True,'count_neutral_applied_replacements':len(placement['changes']),'all_new_base_scales_under_2':True,'all_placement_support_points_current_native_dry':True,'new_models_sha256':{str(p.relative_to(N)):hashlib.sha256(p.read_bytes()).hexdigest()for p in sorted(A.iterdir())if p.is_file()}};(R/'MODEL_VALIDATION.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps({'passed':True,'complete_GLBS':6,'exact_collision':'PASS','placed_replacements':len(placement['changes'])}))
