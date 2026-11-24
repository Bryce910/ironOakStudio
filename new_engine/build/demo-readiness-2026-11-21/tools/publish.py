from pathlib import Path
import json,hashlib,os,subprocess,sys
N=Path(__file__).resolve().parents[1];E=N.parents[1];R=E.parent
sys.path.insert(0,str(N));from check_unified import check
check();manifest=R/'CURRENT_VALWOOD.json';old=manifest.read_bytes();d=json.loads(old)
assert Path(d['runtime_generation']).name=='aaa-merge-2026-11-21','Active generation changed; do not overwrite'
assert hashlib.sha256(Path(d['binary']).read_bytes()).hexdigest()==d['binary_sha256'],'Baseline changed'
log=(N/'review/native-ui-v3/game.log').read_text();assert 'Owner restored MID nm_city_region_camp' in log;assert '[Upscale] frames=2622' in log;assert 'Owner MID restore rolled back: nm_city_region_camp' not in log
live=Path(d['working_world']);cache=live/'models/camp_atmosphere/texture_blocks';assert not cache.exists() and not cache.is_symlink(),'Existing cache must not be overwritten'
preserved={str(p.relative_to(live)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [live/'graphics.settings',*(live/'saves').glob('*')] if p.is_file()}
(N/'review/CURRENT_VALWOOD.before.json').write_bytes(old)
cache.symlink_to(N/'presentation/camp_models/texture_blocks',target_is_directory=True)
d.update(runtime_generation=str(N),binary=str(N/'runtime/rpg'),source=str(N/'source-v1'),shared_shaders=str(N/'shared/shaders'),guide=str(N/'GUIDE.txt'),binary_sha256=json.loads((N/'READY_INPUTS.json').read_text())['binary_sha256'])
tmp=manifest.with_name('.CURRENT_VALWOOD.demo-readiness.tmp');tmp.write_text(json.dumps(d,indent=2)+'\n');assert manifest.read_bytes()==old;os.replace(tmp,manifest)
for name,digest in preserved.items():assert hashlib.sha256((live/name).read_bytes()).hexdigest()==digest,'Save/settings changed'
(N/'review/PUBLISHED.json').write_text(json.dumps(dict(generation=str(N),binary_sha256=d['binary_sha256'],native_only=True,Windows_work_performed=False,live_save_and_settings_hashes_preserved=preserved,camp_texture_cache=str(cache),prior_manifest='CURRENT_VALWOOD.before.json'),indent=2)+'\n')
print('Published',N.name,'; live saves/settings unchanged')
