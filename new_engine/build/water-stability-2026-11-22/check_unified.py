"""Water stability runtime: verify reviewed inputs and unified launcher routing."""
from pathlib import Path
import importlib.util, sys, json, hashlib
N=Path(__file__).resolve().parent
sys.dont_write_bytecode=True
REQUIRED_SHADERS=('world_foliage.fs.glsl','world_terrain.fs.glsl','world_sky.fs.glsl','world_water.fs.glsl','world_post.fs.glsl',
                  'world_bloom_down.fs.glsl','world_bloom_up.fs.glsl','world_exposure_meter.fs.glsl','world_object_motion.vs.glsl',
                  'world_object_motion.fs.glsl','world_weather_fog.fs.glsl','world_surface_library.fs.glsl','world_scene_particles.fs.glsl')
def check():
    binary=N/'runtime/rpg'
    ready=json.loads((N/'READY_INPUTS.json').read_text())
    def verify(path,expected):
        if hashlib.sha256(path.read_bytes()).hexdigest()!=expected:raise RuntimeError('Reviewed input changed: '+str(path))
    verify(binary,ready['binary_sha256'])
    for file,digest in ready['source_sha256'].items():
        path=Path(file);path=path if path.is_absolute() else N/'source-v1'/path
        verify(path,digest)
    for file,digest in ready['artifacts'].items():verify(N/file,digest)
    if not binary.resolve().is_file():raise RuntimeError('Camp runtime missing: run build_candidate.py')
    for s in REQUIRED_SHADERS:
        if not (N/'shared/shaders'/s).is_file():raise RuntimeError('Missing shader '+s)
    if not (N/'vegetation/creek_dressing_instances.txt').is_file():raise RuntimeError('Missing vegetation overlay')
    spec=importlib.util.spec_from_file_location('aaa_merge_launcher',N/'launch.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    for mode in m.MODES:
        command,env,_=m.configuration(mode)
        if command[0]!=str(N/'runtime/rpg') or env['VALWOOD_SHADER_BANK_ROOT']!=str(N/'shared/shaders'):raise RuntimeError('Mode '+mode+' not routed to this build')
    return True
if __name__=='__main__':check();print('Water stability runtime OK')
