from pathlib import Path
import json,hashlib,datetime,csv,statistics
N=Path(__file__).resolve().parents[1]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
binary=sha(N/'runtime/rpg');runs={}
for name,tier,budget in [('high-tier-acceptance','High',1536),('low-tier-acceptance','Low',1024)]:
 p=N/'review'/name;d=json.loads((p/'profile.world.json').read_text());l=json.loads((p/'launch.json').read_text());log=(p/'game.log').read_text()
 assert l['binary_sha256']==binary
 assert l['shader_sha256']['world_water.fs.glsl']==sha(N/'shared/shaders/world_water.fs.glsl')
 assert d['gl_error']==0 and d['asset_streaming']['failed']==0 and d['asset_streaming']['critical_pending']==0
 assert 'Texture admission failed alias=' not in log and 'GL_INVALID' not in log
 assert f'[Environment textures] Loaded {tier},' in log and f'texture admission budget {budget} MiB' in log
 assert d['asset_streaming']['texture_bytes']<=budget*1024*1024
 assert d['arrival_camp_render']['actors']==3 and d['arrival_camp_render']['failures']==0
 assert d['scene_particles']['startup_ready'] and d['scene_particles']['failures']==0
 assert d['wildlife']['load_failures']==0
 if tier=='High':
  route=json.loads((p/'route.json').read_text());assert route['complete'] and route['captures']==7 and route['failures']==0
  assert d['asset_streaming']['resident']==d['asset_streaming']['wanted']
 runs[name]=dict(binary_sha256=binary,gl_error=0,streaming=d['asset_streaming'],particles=d['scene_particles'],environment_effects=d['environment_effects'],graphics=d['graphics'],campers=d['arrival_camp_render'],birds=d['wildlife'],shader_sha256=l['shader_sha256'])
 if tier=='Low':
  rows=list(csv.DictReader((p/'profile.csv').open()))[-600:]
  runs[name]['settled_camp_samples']=len(rows)
  runs[name]['median_frame_ms']=statistics.median(float(r['frame_ms']) for r in rows)
  runs[name]['median_fps_derived']=1000/runs[name]['median_frame_ms']
result=dict(passed=True,scope='Scoped Linux rendering/settings and streaming acceptance, not AAA certification or a complete interactive story/controls rehearsal',binary_sha256=binary,recorded_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),runs=runs,latest_user_request=dict(four_effect_tiers=True,wind_driven_cooking_fire_smoke=True,scenery_texture_tiers=True),remaining=['Plain path/hill and angular bank art remain','Legacy town NPC idles and distant tree repetition remain','Full controls, swim/dive and complete story rehearsal need unlocked desktop','RX5300 performance remains below a 45/60FPS target'],aaa_quality_certified=False,Windows_work_performed=False)
(N/'review/NATIVE_ACCEPTANCE.json').write_text(json.dumps(result,indent=2)+'\n')
print('Native scoped acceptance PASS',binary)
print('Low camp median FPS:',runs['low-tier-acceptance']['median_fps_derived'])
