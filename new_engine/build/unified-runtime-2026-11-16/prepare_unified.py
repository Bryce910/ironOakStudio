from pathlib import Path
import hashlib,json,shutil,os
N=Path(__file__).resolve().parent;E=N.parents[1];ROOT=E.parent
LATEST=E/'build/joust-articulated-falls-2026-11-16'
ARENA=E/'build/character-ultra4k-2026-11-15'
DEMO=E/'build/demo-final-pass-2026-11-15/demo-project'
DESIGN=E/'projects/valwood_design'
WORLD=E/'projects/valwood_current'
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def clone(src,dst):
 dst.mkdir(parents=True,exist_ok=True)
 for f in src.iterdir():
  if f.name in ('recovery','autosaves','studio-layout.ini','world.studio.workspace') or '.recovery' in f.name:continue
  target=dst/f.name
  if f.is_dir():clone(f.resolve(),target)
  elif not target.exists():
   # Authoring files are independent. Large immutable source media is shared.
   if 'surfaces' not in dst.parts and f.suffix.lower() in ('.glb','.fbx','.png','.jpg','.jpeg','.exr','.hdr','.wav','.ogg','.mp3','.bin','.ktx2','.webp','.ttf','.otf','.vbc7','.bank','.height','.coverage','.vwc','.pnt','.npz','.bmp'):
    target.symlink_to(f.resolve())
   else:shutil.copy2(f,target,follow_symlinks=True)
if (N/'PREPARED.json').exists():raise SystemExit('Already prepared; do not replace artist edits')
clone(DESIGN,WORLD)
# Preserve full saved designer content, including the 580 additional apartment assets.
# Supplement missing story-only definitions without replacing authored world files.
for folder in ('opening','ai'):
 clone(DEMO/folder,WORLD/folder)
backup=N/'migration-backup';backup.mkdir(exist_ok=True)
shutil.copytree(DEMO/'saves',backup/'previous-demo-saves',symlinks=True,dirs_exist_ok=True)
shutil.copytree(DESIGN/'saves',backup/'previous-designer-saves',symlinks=True,dirs_exist_ok=True)
for f in (DEMO/'saves').iterdir():
 if f.is_file():shutil.copy2(f,WORLD/'saves'/f.name)
(WORLD/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
for mode,origin in [('arena',ARENA/'arena-project'),('joust',LATEST/'joust-project')]:clone(origin,N/(mode+'-project'))
shared=N/'shared';shared.mkdir(exist_ok=True)
shaders=shared/'shaders';shutil.copytree(ARENA/'arena-project/shaders',shaders,symlinks=False)
# Mounted-specific additions share the same material/lighting generation.
shader_differences=[]
for f in (LATEST/'joust-project/shaders').iterdir():
 if not f.is_file():continue
 dst=shaders/f.name
 if dst.exists() and sha(dst)!=sha(f):shader_differences.append(f.name)
 shutil.copy2(f,dst)
textures=shared/'character-textures';textures.symlink_to((ARENA/'character-textures').resolve(),target_is_directory=True)
settings=(ARENA/'arena-project/graphics.settings').read_text()
(shared/'graphics.settings').write_text(settings)
for project in (WORLD,N/'arena-project',N/'joust-project'):
 # Replace only newly staged resources, never published or original working projects.
 old=project/'shaders'
 if old.is_symlink():old.unlink()
 elif old.exists():shutil.rmtree(old)
 old.symlink_to(shaders,target_is_directory=True)
 old=project/'character-textures'
 if old.is_symlink() or old.is_file():old.unlink()
 elif old.exists():shutil.rmtree(old)
 old.symlink_to(textures.resolve(),target_is_directory=True)
 shutil.copy2(shared/'graphics.settings',project/'graphics.settings')
(N/'runtime').mkdir(exist_ok=True)
shutil.copy2(LATEST/'runtime-native-v1/native-bin/rpg',N/'runtime/rpg')
shutil.copy2(LATEST/'runtime-native-v1/READY.json',N/'runtime/ORIGIN_BUILD.json')
(N/'source-v1').symlink_to(LATEST/'source-v1',target_is_directory=True)
record=dict(world_project=str(WORLD),arena_project=str(N/'arena-project'),joust_project=str(N/'joust-project'),runtime=str(N/'runtime/rpg'),runtime_sha256=sha(N/'runtime/rpg'),source=str(LATEST/'source-v1'),source_receipt_sha256=sha(N/'runtime/ORIGIN_BUILD.json'),shared_shaders=str(shaders),shared_shader_overlay_differences=shader_differences,saves_preserved=True,original_projects_modified=False,windows_work_performed=False)
(N/'PREPARED.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps(record,indent=2))
