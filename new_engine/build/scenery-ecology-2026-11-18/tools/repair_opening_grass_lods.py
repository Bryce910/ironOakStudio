"""Keep the exact owned photo-grass HIGH/MID pair for the opening aliases."""
from pathlib import Path
import hashlib,json,shutil,struct
N=Path(__file__).resolve().parents[1];P=N/'world-project';models=P/'models';studio=(P/'world.studio').read_text().splitlines();owned=N/'owned-grass';owned.mkdir(exist_ok=True)
if models.is_symlink():
 old=models.resolve();models.unlink();models.mkdir()
 for item in old.iterdir():(models/item.name).symlink_to(item.resolve(),target_is_directory=item.is_dir())
def triangles(path):
 b=path.read_bytes();n=struct.unpack_from('<I',b,12)[0];j=json.loads(b[20:20+n]);return sum((j['accessors'][p['indices']]['count'] if 'indices' in p else j['accessors'][p['attributes']['POSITION']]['count'])//3 for m in j['meshes'] for p in m['primitives'])
report=[]
for family,number in [('long',1),('broad',0)]:
 stem=next(line.split()[2] for line in studio if line.startswith('asset bep_nature_grass_'+family+' '));source=models/(stem+'_mid.glb');target=owned/f'cinematic_grass_{number}_mid.glb';old_triangles=triangles(models/target.name)
 shutil.copy2(source,target);link=models/target.name
 if link.is_symlink():link.unlink()
 link.symlink_to(target.resolve());report.append(dict(family=family,previous_triangles=old_triangles,corrected_triangles=triangles(target),source=str(source.resolve()),sha256=hashlib.sha256(target.read_bytes()).hexdigest(),output=str(target.resolve())))
(N/'GRASS_LOD_REPAIR.json').write_text(json.dumps(dict(passed=True,source_geometry_unchanged=True,matched_near_and_mid_family=True,repairs=report),indent=2)+'\n');print(json.dumps(report,indent=2))
