#!/usr/bin/env python3
"""CPU verification of real civilian/armored appearance composition and FP subsets."""
from pathlib import Path
import hashlib,json,subprocess,time
N=Path(__file__).resolve().parents[1]
OUT=N/'review/hero-compose'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 source=N/'source-v1'
 obj=N/'runtime-native-v1/native-objects/engine/world_animation.cpp.o'
 build=['/usr/bin/x86_64-linux-gnu-g++-13','-std=c++17','-O1','-I',str(source),str(OUT/'verify.cpp'),str(source/'engine/world_animation.cpp'),'-o',str(OUT/'verify')]
 result=subprocess.run(build,capture_output=True,text=True)
 if result.returncode:raise RuntimeError(result.stderr)
 eq=N/'assets/story_hero/rigs'
 assets=[N/'assets/story_hero'/outfit/name for outfit in ['civilian','armored'] for name in ['body.glb','appearance.cfg']]
 eq=N/'assets/story_hero/rigs'
 assets += [eq/(name+'.glb') for name in ['sword','sword_shield','great_axe','staff']] + [eq/'HERO_ASSET.json',eq/'loadouts.manifest']
 before={str(p):sha(p) for p in assets}
 args=[str(OUT/'verify'),*[str(N/'assets/story_hero'/outfit/'appearance.cfg') for outfit in ['civilian','armored']],*[str(eq/(name+'.glb')) for name in ['sword','sword_shield','great_axe','staff']]]
 start=time.monotonic();result=subprocess.run(args,capture_output=True,text=True)
 (OUT/'RAW.json').write_text(result.stdout);(OUT/'STDERR.txt').write_text(result.stderr)
 after={str(p):sha(p) for p in assets}
 evidence=[OUT/'verify.cpp',source/'engine/world_knight_appearance.h',source/'engine/world_skeletal_hero_viewmodel.h',source/'engine/world_skeletal_hero_sleeves.h',source/'engine/world_animation.cpp',source/'engine/world_model_hair.h',source/'engine/world_model_skin.h']
 record={'exit_code':result.returncode,'passed':result.returncode==0 and before==after,'assets_unchanged_during_check':before==after,'elapsed_seconds':time.monotonic()-start,'asset_sha256':before,'command':args,'build_command':build,'binary_sha256':sha(OUT/'verify'),'source_sha256':{str(p):sha(p) for p in evidence}}
 (OUT/'EXECUTION.json').write_text(json.dumps(record,indent=2)+'\n')
 if not record['passed']:raise RuntimeError(result.stderr)
 raw=json.loads(result.stdout);print(json.dumps({'passed':True,'cases':len(raw['cases']),'assertions':raw['assertions'],'deformation_samples':raw['deformation_samples'],'asset_sha256':before},indent=2))
if __name__=='__main__':main()
