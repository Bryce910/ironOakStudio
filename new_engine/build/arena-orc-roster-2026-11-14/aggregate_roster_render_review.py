#!/usr/bin/env python3
"""Pin all14 genuine per-character fresh render/lifecycle proofs, without rerendering."""
from pathlib import Path
import hashlib,json,sys
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parent;P=N/'roster-review';R=N/'arena_orcs_roster';IDS={'orc01','orc02','orc03','orc04','orc05','orc06','azrak','costalmed','dardan','girra','gnur','kanit','magur','ursag'}
def require(ok,msg):
 if not ok:raise RuntimeError(msg)
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def pin(p,d,h=None):
 p=Path(p).resolve();v=sha(p);require(h is None or h==v,'Changed dependency: '+str(p));d[str(p)]=v;return v
def main():
 inputs={};outputs={};pin(Path(__file__),inputs);roster_path=R/'ORC_ROSTER.json';pin(roster_path,inputs);roster=json.loads(roster_path.read_text());entries=roster['entries'];require(len(entries)==14 and {r['id']for r in entries}==IDS,'Runtime roster IDs differ')
 children=[];summaries=[]
 for entry in entries:
  identity=entry['id'];path=P/'characters'/identity/'CHARACTER_RENDER_REVIEW.json';pin(path,inputs);doc=json.loads(path.read_text());require(doc['passed']and doc['character_id']==identity and doc['peak_resident_orc_banks']==1 and doc['all_real_bank_storage_released'],'Per-character render/lifecycle proof missing')
  for p,h in doc['inputs'].items():pin(p,inputs,h)
  for p,h in doc['outputs_sha256'].items():pin(p,outputs,h)
  require(sha(R/entry['asset_manifest'])==entry['asset_manifest_sha256'],'Runtime roster manifest differs from frozen child')
  row=doc['resource_measurement'];children.append(dict(id=identity,review=str(path),review_sha256=sha(path),contact_sheet=doc['contact_sheet'],motion_montage=doc['motion_montage'],resource_measurement=row))
  summaries.append(dict(id=identity,declared_bank_buffer_bytes=row['peak_buffer_bytes'],registered_shared_texture_gpu_bytes=row['peak_texture_gpu_bytes'],measured_declared_peak_bytes=row['peak_buffer_bytes']+row['peak_texture_gpu_bytes'],software_gl_draw_finish_ms_mean=row['software_gl_update_draw_finish_ms_mean'],load_seconds=row['load_seconds']))
 report=dict(schema='valwood.orc-roster.render-review.v1',passed=True,expected_ids=sorted(IDS),reviewed_ids=sorted(r['id']for r in children),inputs=inputs,outputs_sha256=outputs,character_reviews=children,per_character_resource_and_software_cost=summaries,
  peak_resident_orc_banks=1,all_real_bank_buffer_and_texture_storage_released=True,production_motion_vertex_and_anatomy=True,all_eight_regions_rendered_per_character=True,hardware_vram_or_fps_accepted=False,controller_feel_accepted=False,
  limits=['Independent actual GL load/render/free for every frozen source bank, with baseline geometry object/storage and texture-cache counts exactly restored. Actual sequential14selection in one full-game startup is a separate native receipt.',
  'The preview uses production motion/anatomy/vertex code and original maps with neutral compact fragment lighting. Hardware VRAM/FPS and controller feel are not certified. Declared bytes exclude driver/context/framebuffer costs. No simultaneous mixed-character streaming is implemented.'])
 target=P/'ROSTER_RENDER_REVIEW.json';target.write_text(json.dumps(report,indent=2,sort_keys=True)+'\n');print('PASS actual14character render/lifecycle aggregate:',target)
if __name__=='__main__':main()
