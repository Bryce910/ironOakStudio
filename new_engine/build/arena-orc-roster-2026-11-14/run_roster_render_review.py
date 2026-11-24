#!/usr/bin/env python3
"""Sequential actual production geometry/motion, material/sever and GL ownership QA."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,sys,time
from PIL import Image,ImageDraw
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parent;R=N/'arena_orcs_roster';P=N/'roster-review';D=P/'sequential-render';BUILD=P/'ROSTER_PREVIEW_BUILD.json';B=P/'preview_orc_roster';REPORT=P/'ROSTER_RENDER_REVIEW.json'
IDS={'orc01','orc02','orc03','orc04','orc05','orc06','azrak','costalmed','dardan','girra','gnur','kanit','magur','ursag'}
def require(ok,msg):
 if not ok:raise RuntimeError(msg)
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def pin(p,d,h=None):
 p=Path(p).resolve();v=sha(p);require(h is None or h==v,'Changed dependency: '+str(p));d[str(p)]=v;return v
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--review-existing',action='store_true');args=parser.parse_args();P.mkdir(exist_ok=True);D.mkdir(exist_ok=True)
 subprocess.run([sys.executable,str(N/'check_orc_roster_assets.py'),'--check-only'],check=True)
 assets_path=P/'ROSTER_ASSET_REVIEW.json';assets=json.loads(assets_path.read_text());require(assets['passed']and assets['strict_complete']and set(assets['reviewed_ids'])==IDS,'Complete strict source roster required')
 inputs=dict(assets['inputs']);inputs.update(assets['outputs_sha256']);pin(assets_path,inputs);pin(Path(__file__),inputs);pin(BUILD,inputs);pin(B,inputs)
 build=json.loads(BUILD.read_text());require(build['compiled']and build['binary_sha256']==sha(B),'Preview binary stale')
 for p,h in build['inputs'].items():pin(p,inputs,h)
 for p in (N/'arena-project/shaders').rglob('*'):
  if p.is_file():pin(p,inputs)
 for p in (R/'ORC_ROSTER.json',R/'ORC_ROSTER.cfg'):pin(p,inputs)
 roster=json.loads((R/'ORC_ROSTER.json').read_text());require({r['id']for r in roster['entries']}==IDS,'Advertised runtime IDs differ')
 command=[str(B),str(R/'ORC_ROSTER.cfg'),str(N/'arena-project/shaders'),str(D)];env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LP_NUM_THREADS='4',LIBGL_ALWAYS_SOFTWARE='1',MESA_DEBUG='1')
 raw_path=D/'ROSTER_GPU_RAW.json';log=D/'sequential-render.log';preflight=D/'RENDER_PREFLIGHT.json'
 if args.review_existing:
  old=json.loads(preflight.read_text());require(old['inputs']==inputs and old['command']==command,'Existing GPU review was not made from current frozen inputs')
 else:
  preflight.write_text(json.dumps(dict(inputs=inputs,command=command),indent=2,sort_keys=True)+'\n')
  with log.open('w')as f:subprocess.run(command,cwd=N,env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
 raw=json.loads(raw_path.read_text());rows=raw['characters'];require(raw['passed']and raw['reviewed_characters']==14 and len(rows)==14 and {r['id']for r in rows}==IDS and raw['peak_resident_orc_banks']==1 and raw['final_resident_orc_banks']==0 and raw['all_real_geometry_and_texture_storage_released'],'Actual sequential GL bank coverage/resource cleanup failed')
 require('llvmpipe'in raw['renderer'].lower()or 'soft'in raw['renderer'].lower(),'Performance diagnostic must explicitly be software GL')
 outputs={};images=[];sheets=[];summaries=[]
 for p in (raw_path,log,preflight):pin(p,outputs)
 for entry in roster['entries']:
  identity=entry['id'];row=next(r for r in rows if r['id']==identity)
  require(row['body_poses']==172 and row['rigid_pairs']==217 and row['frames']>=800 and row['detached_regions_drawn']==8 and row['cap_draws']>0 and row['pose_uploads']>0 and row['actors_peak']>=4 and row['real_gl_geometry_released']and row['texture_cache_released']and row['gl_error']==0,'Incomplete actual motion/sever/crowd/resource coverage')
  captures=sorted((D/identity).glob('*.png'));require(len(captures)==row['captures']and len(captures)>=25,'Actual source-frame captures missing')
  sheet=Image.new('RGB',(5*256,((len(captures)+4)//5)*282),(18,23,29));draw=ImageDraw.Draw(sheet)
  for i,p in enumerate(captures):
   pin(p,outputs);images.append(str(p))
   with Image.open(p)as image:
    require(image.size==(640,640),'Capture resolution changed');small=image.convert('RGB');small.thumbnail((256,256));x=i%5*256;y=i//5*282;sheet.paste(small,(x,y+26));draw.text((x+6,y+6),identity+' '+p.stem,fill='white')
  sheet_path=D/(identity+'-render-sheet.jpg');sheet.save(sheet_path,quality=88);pin(sheet_path,outputs);sheets.append(str(sheet_path))
  summaries.append(dict(id=identity,declared_bank_buffer_bytes=row['peak_buffer_bytes'],registered_shared_texture_gpu_bytes=row['peak_texture_gpu_bytes'],measured_declared_peak_bytes=row['peak_buffer_bytes']+row['peak_texture_gpu_bytes'],software_gl_draw_finish_ms_mean=row['software_gl_update_draw_finish_ms_mean'],load_seconds=row['load_seconds']))
 require(inputs=={p:sha(p)for p in inputs},'Frozen source/bank changed during GL review')
 report=dict(schema='valwood.orc-roster.render-review.v1',passed=True,expected_ids=sorted(IDS),reviewed_ids=sorted(r['id']for r in rows),inputs=inputs,outputs_sha256=outputs,production_vertex_shader_and_motion=True,scenarios=['idle','Run','cuts5directions','guards5directions','reactions5directions','eight_detached_regions','both_hands_weapon_hidden','four_actor_crowd'],native_gl_result=raw,per_character_resource_and_software_cost=summaries,contact_sheets=sheets,command=command,
  peak_resident_orc_banks=1,all_real_bank_buffer_and_texture_storage_released=True,hardware_vram_or_fps_accepted=False,controller_feel_accepted=False,
  limits=['Geometry buffers/VAOs are counted through real production OpenGL calls and shared texture storage through the real cache. These declared bytes exclude driver/context/framebuffer overhead and are not physical VRAM residency or a3GB hardware certification.',
  'Software OpenGL timing is a reproducible diagnostic, not target GPU FPS. The isolated neutral preview uses production motion/vertex/anatomy code with a compact material-lighting fragment shader; full-game native material/selection views are separate.',
  'Each bout selects one distinct character. This does not implement simultaneous mixed-character streaming or certify controller feel.'])
 REPORT.write_text(json.dumps(report,indent=2,sort_keys=True)+'\n');print('PASS actual14character sequential GL/render/sever/resource check:',REPORT)
if __name__=='__main__':main()
