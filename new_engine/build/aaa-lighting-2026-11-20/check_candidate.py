"""Verify the candidate runtime matches its build receipt and the shader bank differs from camp-atmosphere-2026-11-20 only by the reviewed files."""
from pathlib import Path
import hashlib, json
N = Path(__file__).resolve().parent
B = N.with_name('camp-atmosphere-2026-11-20')
CHANGED = {'world_post.fs.glsl','world_bloom_down.fs.glsl','world_bloom_up.fs.glsl','world_exposure_meter.fs.glsl','world_object_motion.vs.glsl','world_object_motion.fs.glsl','world_temporal.fs.glsl','world_foliage.fs.glsl','world_terrain.fs.glsl','world_weather_fog.fs.glsl','world_water.fs.glsl'}

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def inventory(root): return {str(p.relative_to(root)): sha(p) for p in sorted(Path(root).rglob('*')) if p.is_file()}

def check():
    receipt = json.loads((N/'runtime-native-v1/BUILD_RECEIPT.json').read_text())
    if sha(N/'runtime/rpg') != receipt['binary_sha256']: raise RuntimeError('Candidate runtime does not match its build receipt')
    old, new = inventory(B/'shared/shaders'), inventory(N/'shared/shaders')
    delta = {p for p in set(old) | set(new) if old.get(p) != new.get(p)}
    if delta != CHANGED: raise RuntimeError('Unexpected shader differences: ' + ', '.join(sorted(delta)))
    for link in [N/'world-project/shaders', N/'arena-project/shaders', N/'joust-project/shaders', *(N/'area-projects').glob('*/shaders')]:
        if link.resolve() != (N/'shared/shaders').resolve(): raise RuntimeError(f'{link} does not use the candidate shaders')

if __name__ == '__main__':
    check(); print('Candidate OK')
