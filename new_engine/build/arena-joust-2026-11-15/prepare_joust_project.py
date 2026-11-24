from pathlib import Path
import shutil,json
n=Path(__file__).resolve().parent;b=n.parent/'arena-closeup-finish-2026-11-15';src=b/'arena-project';out=n/'joust-project'
assert not out.exists(),'Do not overwrite a joust project'
out.mkdir()
for p in src.iterdir():
 if p.name in ['saves','world.studio.workspace','studio-layout.ini']:continue
 dest=out/p.name
 if p.is_symlink():dest.symlink_to(p.resolve(),target_is_directory=p.is_dir())
 elif p.is_dir():shutil.copytree(p,dest)
 else:shutil.copy2(p,dest)
(out/'project.valwood').write_text((out/'project.valwood').read_text().replace('name=Valwood | Modular Knight Test','name=Valwood | Jousting Practice'))
(out/'saves').mkdir()
(out/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
(out/'joust.enabled').write_text('VALWOOD_JOUST_ARENA 1\n')
(out/'world.studio').write_text('VALWOOD_STUDIO 14\nlook 0.18 0.95 0.90 1.0 15.2 100 0.38 0 0.25 0.08\nstructure_ids 3\nstructure 1 1 "" 3 21 3 0.35 1.1 0 0 0 0 0 2 -1810.5 64.06 -10561 -1810.5 64.06 -10439\nstructure 2 1 "" 3 21 3 0.35 1.1 0 0 0 0 0 2 -1789.5 64.06 -10561 -1789.5 64.06 -10439\nentities "VALWOOD_ENTITIES 4 1 0"\nsurfaces "VALWOOD_SURFACES 2"\nenvironment environments/arena-overcast-puresky.hdr 0 0.40 1 1\n')
(out/'playtest.route').write_text('VALWOOD_PLAYTEST 1\nname "Valwood Jousting Grounds"\nspawn -1801.7 65.81 -10540 3.14159265359 -0.06\ncount 1\npath "Jousting list" -1800 64 -10500 2 checkpoint\n')
# Keep existing woodland outside the new list, removing any obstruction in its full footprint.
kept=removed=0
for p in (out/'scenery').glob('*instances.txt'):
 lines=[]
 for line in p.read_text().splitlines():
  tok=line.split()
  if len(tok)>3 and not line.startswith('#'):
   try:x=(float(tok[1])-.5)*100800+1800;z=(float(tok[2])-.5)*56700+10500
   except ValueError:lines.append(line);continue
   if abs(x)<23 and abs(z)<63:removed+=1;continue
   kept+=1
  lines.append(line)
 # Source parser requires at least one authored instance per file.
 if not any(l.strip()and not l.startswith('#')for l in lines):lines.append('purchased_crate 0.4823214285714286 0.3153439153439153 0 0.75 0 0 0 0 0 1 1 1 64')
 p.write_text('\n'.join(lines)+'\n')
(out/'joust-assets').symlink_to(n/'joust-assets',target_is_directory=True)
(n/'PROJECT_ORIGIN.json').write_text(json.dumps({'baseline_project':str(src),'separate_saves':True,'new_list_clearance_x':23,'new_list_clearance_z':63,'removed_instances_in_list':removed,'retained_background_instances':kept,'old_structures_removed':True,'windows_work_performed':False},indent=2)+'\n')
print('Prepared separate joust project:',kept,'background objects;',removed,'old objects cleared from list.')
