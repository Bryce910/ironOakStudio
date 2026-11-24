#!/usr/bin/env python3
"""Pipeline one frozen genuine character through actual GL motion/sever/resource QA."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,sys
from PIL import Image,ImageDraw
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parent;R=N/'arena_orcs_roster';P=N/'roster-review';BUILD=P/'ROSTER_PREVIEW_BUILD.json';B=P/'preview_orc_roster'
IDS={'orc01','orc02','orc03','orc04','orc05','orc06','azrak','costalmed','dardan','girra','gnur','kanit','magur','ursag'}
def require(ok,msg):
 if not ok:raise RuntimeError(msg)
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def pin(p,d,h=None):
 p=Path(p).resolve();v=sha(p);require(h is None or h==v,'Changed dependency: '+str(p));d[str(p)]=v;return v
def main():
 parser=argparse.ArgumentParser();parser.add_argument('character',choices=sorted(IDS));parser.add_argument('--review-existing',action='store_true');args=parser.parse_args();identity=args.character
 assets_path=P/('SINGLE_CHARACTER_'+identity+'.json');assets=json.loads(assets_path.read_text());require(assets['passed']and assets['character_count']==1 and assets['characters'][0]['id']==identity,'Current strict child contract required')
 inputs={};pin(Path(__file__),inputs);pin(assets_path,inputs)
 for k in ('inputs','outputs_sha256'):
  for p,h in assets[k].items():pin(p,inputs,h)
 pin(BUILD,inputs);pin(B,inputs);build=json.loads(BUILD.read_text());require(build['compiled']and build['binary_sha256']==sha(B),'Preview binary stale')
 for p,h in build['inputs'].items():pin(p,inputs,h)
 for p in (N/'arena-project/shaders').rglob('*'):
  if p.is_file():pin(p,inputs)
 source=json.loads((N/'intake_roster/ORC_SOURCE_ROSTER.json').read_text());entry=next(r for r in source['characters']if r['id']==identity)
 folder=P/'characters'/identity;folder.mkdir(parents=True,exist_ok=True);private=folder/'preview-project';private.mkdir(exist_ok=True);link=private/identity
 require(not link.exists()or link.is_symlink()and link.resolve()==R/identity,'Preview bank link changed')
 if not link.is_symlink():link.symlink_to(R/identity,target_is_directory=True)
 cfg=private/'ORC_ROSTER.cfg';text='VALWOOD_ORC_ROSTER 1\ndefault '+identity+'\ncharacter '+identity+' '+json.dumps(entry['display_name'])+' "'+identity+'/body" "'+identity+'/anatomy" "'+identity+'/ORC_MATERIALS.cfg"\nend\n'
 if args.review_existing:require(cfg.read_text()==text,'Existing private config changed')
 else:cfg.write_text(text)
 pin(cfg,inputs);out=folder/'render';out.mkdir(exist_ok=True);log=folder/'character-render.log';preflight=folder/'RENDER_PREFLIGHT.json';command=[str(B),str(cfg),str(N/'arena-project/shaders'),str(out),identity]
 if args.review_existing:
  old=json.loads(preflight.read_text());require(old['inputs']==inputs and old['command']==command,'Existing character render is not current')
 else:
  preflight.write_text(json.dumps(dict(inputs=inputs,command=command,physical_bank=str(link.resolve())),indent=2,sort_keys=True)+'\n')
  env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LP_NUM_THREADS='4',LIBGL_ALWAYS_SOFTWARE='1',MESA_DEBUG='1')
  with log.open('w')as f:subprocess.run(command,cwd=N,env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
 raw_path=out/'ROSTER_GPU_RAW.json';raw=json.loads(raw_path.read_text());require(raw['passed']and raw['reviewed_characters']==1 and raw['peak_resident_orc_banks']==1 and raw['final_resident_orc_banks']==0 and raw['all_real_geometry_and_texture_storage_released'],'Real per-bank GL load/render/release failed')
 row=raw['characters'][0];require(row['id']==identity and row['frames']>=800 and row['body_poses']==172 and row['rigid_pairs']==217 and row['detached_regions_drawn']==8 and row['cap_draws']>0 and row['pose_uploads']>0 and row['actors_peak']>=4 and row['real_gl_geometry_released']and row['texture_cache_released']and row['gl_error']==0,'Actual primitive/motion/sever/crowd coverage incomplete')
 require('llvmpipe'in raw['renderer'].lower()or 'soft'in raw['renderer'].lower(),'Only explicitly software GL timings are supported')
 if identity=='gnur':require(row['late_load_failure_cleanup_verified']and row['failed_selection_previous_bank_restored'],'Actual late-failure GL teardown and prior-bank recovery were not verified')
 outputs={};captures=sorted((out/identity).glob('*.png'));require(len(captures)==row['captures']and len(captures)>=25,'Source frame captures missing')
 for p in (raw_path,log,preflight,*captures):pin(p,outputs)
 if identity=='gnur':
  for p in (out/identity/'late-failure-view/PRIVATE_FAILURE_VIEW',out/identity/'late-failure-view/body/combat_body_171.glb'):pin(p,outputs)
 sheet=Image.new('RGB',(5*256,((len(captures)+4)//5)*282),(18,23,29));draw=ImageDraw.Draw(sheet)
 for i,p in enumerate(captures):
  with Image.open(p)as image:
   require(image.size==(640,640),'Actual render dimensions changed');small=image.convert('RGB');small.thumbnail((256,256));x=i%5*256;y=i//5*282;sheet.paste(small,(x,y+26));draw.text((x+6,y+6),identity+' '+p.stem,fill='white')
 sheet_path=folder/'character-render-sheet.jpg';sheet.save(sheet_path,quality=88);pin(sheet_path,outputs)
 quick_names=['idle-47','run-63','cuts-22','cuts-58','cuts-94','cuts-130','cuts-166','guards-89','crowd-47'];quick=Image.new('RGB',(3*320,3*346),(18,23,29));draw=ImageDraw.Draw(quick)
 for i,name in enumerate(quick_names):
  p=out/identity/(name+'.png')
  with Image.open(p)as image:
   thumb=image.convert('RGB');thumb.thumbnail((320,320));x=i%3*320;y=i//3*346;quick.paste(thumb,(x,y+26));draw.text((x+6,y+6),entry['display_name']+' '+name,fill='white')
 quick_path=folder/'character-motion-montage.jpg';quick.save(quick_path,quality=90);pin(quick_path,outputs)
 require(inputs=={p:sha(p)for p in inputs},'Frozen dependencies changed during character render')
 report=dict(schema='valwood.orc-roster.character-render.v1',passed=True,character_id=identity,inputs=inputs,outputs_sha256=outputs,actual_gl_result=raw,resource_measurement=row,contact_sheet=str(sheet_path),motion_montage=str(quick_path),command=command,peak_resident_orc_banks=1,all_real_bank_storage_released=True,
  complete_roster_acceptance=False,hardware_vram_or_fps_accepted=False,controller_feel_accepted=False,
  limits=['Actual production motion/anatomy/vertex paths, original materials and all8sever regions are rendered under software OpenGL. Neutral compact preview fragment lighting differs from the full-game renderer.',
  'Declared buffer and registered image storage exclude driver/context/framebuffer costs, so they are not physical VRAM residency or3GB hardware certification. Software render timing is not target GPU FPS.',
  'This one frozen character is prepared/rendered/freed independently. Actual sequential14selection is validated separately in the ordinary full-game cycle.'])
 target=folder/'CHARACTER_RENDER_REVIEW.json';target.write_text(json.dumps(report,indent=2,sort_keys=True)+'\n');print('PASS actual character GL/render/8region sever/resource cleanup',identity,str(target))
if __name__=='__main__':main()
