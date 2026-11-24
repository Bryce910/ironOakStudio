from pathlib import Path
import shutil,json,hashlib
N=Path(__file__).resolve().parent;E=N.parents[1]
origins={'city':'valwood_city_showcase','grandstone':'valwood_showcase','showcase':'valwood_visual','atlas':'valwood_world'}
immutable={'.glb','.fbx','.png','.jpg','.jpeg','.exr','.hdr','.wav','.ogg','.mp3','.bin','.ktx2','.webp','.ttf','.otf','.vbc7','.bank','.height','.coverage','.vwc','.pnt','.npz','.bmp'}
def clone(src,dst):
 dst.mkdir(parents=True,exist_ok=True)
 for f in src.iterdir():
  if f.name in ('recovery','autosaves','studio-layout.ini','world.studio.workspace','shaders','character-textures') or '.recovery' in f.name:continue
  p=dst/f.name
  if f.is_dir():clone(f.resolve(),p)
  elif not p.exists():
   if 'surfaces' not in dst.parts and (f.suffix.lower() in immutable or f.stat().st_size>16*1024*1024):p.symlink_to(f.resolve())
   else:shutil.copy2(f,p,follow_symlinks=True)
records=[]
for area,origin in origins.items():
 src=E/'projects'/origin;dst=E/'projects'/('valwood_'+area+'_current')
 if dst.exists():raise SystemExit('Area already prepared; do not overwrite artist work: '+str(dst))
 clone(src,dst);(dst/'shaders').symlink_to(N/'shared/shaders',target_is_directory=True);(dst/'character-textures').symlink_to((N/'shared/character-textures').resolve(),target_is_directory=True)
 (dst/'saves').mkdir(exist_ok=True);shutil.copy2(N/'shared/graphics.settings',dst/'graphics.settings')
 records.append(dict(area=area,origin=str(src),project=str(dst),world_sha256=hashlib.sha256((dst/'world.studio').read_bytes()).hexdigest(),authored_world_identical=(src/'world.studio').read_bytes()==(dst/'world.studio').read_bytes()))
 print('Preserved',area,'at',dst,flush=True)
(N/'review/AREA_PROJECT_MIGRATION.json').write_text(json.dumps(dict(passed=all(r['authored_world_identical'] for r in records),original_projects_unchanged=True,areas=records),indent=2)+'\n')
