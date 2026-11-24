#!/usr/bin/env python3
"""Install an audited private-city ground revision with protected-state checks."""
from pathlib import Path
import argparse,fcntl,hashlib,json,shutil,subprocess
from build_city_countryside import ROOT


def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--catalog',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
    project=ROOT/'projects/valwood_city_showcase';out=args.output.resolve();out.mkdir(parents=True,exist_ok=True);catalog=args.catalog.resolve();data=json.loads(catalog.read_text())
    lock=(ROOT/'projects/seldonne/.valwood-instance.lock').open('a');fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    protected=[project/n for n in ['project.valwood','graphics.settings','city.scene','city.sky','showcase.views','ground/city.coverage','ground/city-ground-coverage.png']]+list((project/'shaders').glob('*.glsl'))
    protected.extend(p for p in(project/'saves').rglob('*')if p.is_file());before={str(p.relative_to(project)):digest(p)for p in protected}
    old=json.loads((project/'city_showcase.json').read_text());(out/'metadata-before.json').write_text(json.dumps(old,indent=2)+'\n')
    with(out/'install.log').open('w')as log:subprocess.run(['python3',str(ROOT/'tools/build_village_showcase_project.py'),'--catalog',str(catalog),'--project',str(project),'--geometry-only','--link-immutable'],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
    shutil.copy2(data['ground_support_revision']['repaired_field'],project/'ground/showcase.height')
    for src,dst in zip(data['countryside']['ground_files'],['ground/countryside.height','ground/countryside.coverage']):shutil.copy2(src,project/dst)
    snapshot='qa/installed-countryside-catalog.json';shutil.copy2(catalog,project/snapshot)
    for name in ['city_showcase.json','publisher_showcase.json']:
        path=project/name;new=json.loads(path.read_text())
        for k,v in old.items():
            if k not in new:new[k]=v
        new['installed_catalog_snapshot']=snapshot;new['source_ground_support_revision']=data['ground_support_revision'];new['reviewed_house_translations']=len(data['placement_repairs'])
        new['ground']['support_field_sha256']=digest(project/'ground/showcase.height')
        c=new['countryside'];c.update(source_catalog=str(catalog),source_catalog_sha256=digest(catalog),height_sha256=digest(project/'ground/countryside.height'),coverage_sha256=digest(project/'ground/countryside.coverage'),ground_envelope=data['town_ground_envelope'])
        for k in ['collision_route','collision_route_sha256','route_meters','route_sweeps','minimum_scenery_clearance_m']:c.pop(k,None)
        c['collision_validation']='Pending final installed ground/street checks'
        path.write_text(json.dumps(new,indent=2)+'\n')
    after={str(p.relative_to(project)):digest(p)for p in protected};assert before==after,'Protected presentation state changed'
    report=dict(passed=True,catalog=str(catalog),catalog_sha256=digest(catalog),protected_files=before,primary_support_hash=digest(project/'ground/showcase.height'),physics_hash=digest(project/'city.physics'))
    (out/'install-protection.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'installed':str(catalog),'protected_state_unchanged':True}))


if __name__=='__main__':main()
