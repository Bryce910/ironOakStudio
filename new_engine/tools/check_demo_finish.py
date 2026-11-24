#!/usr/bin/env python3
"""Check the finished nine-source demo, then explicitly collect native evidence.

Default runs source-only validation. --native uses saved Custom85% manual60FPS
settings and sequential jobs; --captures collects separate untimed screenshots.
No build, cook, hash scan, quality override or sample filtering occurs during
native rendering. Both local-probe and audio telemetry are required by default.
"""
import argparse
from collections import Counter
from contextlib import contextmanager
import csv
import fcntl
import json
import math
from pathlib import Path
import subprocess
import sys

import benchmark_graphics_settings as common
import check_bep_environment as bep
import check_castle_streets as streets
import check_cinematic_particles as particles
import check_tanthoon_bandit_camp as camp

ROOT=common.ROOT
SCENERY=common.PROJECT/'scenery'
MANIFEST=SCENERY/'demo_finish_manifest.json'
EVIDENCE=ROOT/'docs/performance/demo-finish'
WARMUP=120
BASELINE_RECORDS=6660
FINISHING_RECORDS=582
CASTLE_FINISHING_RECORDS=382
RAINFOREST_RECORDS=200
COMBINED_RECORDS=BASELINE_RECORDS+FINISHING_RECORDS
RAINFOREST_ALIASES={
    'rock0':6,'rock1':6,'bep_nature_fern':32,'fern1':16,'bep_nature_reed':36,
    'bep_nature_grass_broad':24,'bep_nature_grass_long':18,
    'bep_nature_bush_a':10,'bep_nature_bush_b':10,'bep_nature_palm':18,
    'bep_nature_clover':12,'showcase_sorrel':12,
}
RAINFOREST_ZONES={
    'rainforest_stone_croppings':12,'rainforest_layered_ferns':48,
    'rainforest_reed_fringe':36,'rainforest_broad_grass':42,
    'rainforest_shrub_masses':20,'rainforest_palm_under_canopy':18,
    'rainforest_leaf_mats':24,
}
CASES={
    'route':('castle-west-street','day'),
    'arcade_route':('castle-finished-arcade','day'),
    'finished_arcade':('castle-finished-arcade','day'),
    'finished_west':('castle-finished-west','day'),
    'finished_east':('castle-finished-east','day'),
    'market':('castle-market','day'),
    'tavern':('castle-tavern-interior','dusk'),
    'creek':('castle-creek-garden','day'),
    'garden':('castle-purchased-gardens','day'),
    'rainforest_pool':('rainforest-pool','day'),
    'understory':('rainforest-purchased-understory','day'),
}
EXTRA_TOOLS=('check_demo_finish.py','check_bep_environment.py','check_cinematic_particles.py',
    'check_castle_streets.py','check_tanthoon_bandit_camp.py','build_demo_finish.py',
    'import_bep_nature.py','import_bep_props.py','build_bep_environment.py','build_demo_audio.py','check_demo_audio.py',
    'demo_rainforest_finish.py','build_demo_polish.py','build_village_seated_characters.py',
    'build_village_life.py','build_wildlife_habitats.py','check_demo_polish_camp.py')
require=common.require

def check_rainforest_finish(manifest):
    """Require the actual200 runtime poses and their independent atlas proof."""
    poses=[p for p in manifest['placements']if p['zone'].startswith('rainforest_')]
    proof=manifest['rainforest_finish']
    require(len(poses)==proof['records']==RAINFOREST_RECORDS,'Expected complete200-record rainforest finish')
    aliases=dict(Counter(p['alias']for p in poses));zones=dict(Counter(p['zone']for p in poses))
    require(aliases==proof['aliases']==RAINFOREST_ALIASES,'Rainforest species inventory differs')
    require(zones==proof['zones']==RAINFOREST_ZONES,'Rainforest authored pocket inventory differs')
    require(proof['palms']==18 and all(p['id'].startswith('rf_finish_')and p['parent']is None for p in poses),
            'Rainforest finishing poses/parent contract differs')
    for key in('production_cpu_atlas','old_tree_roots_reserved',
               'inspection_eye_and_water_sightline_reserved','pool_core_untouched'):
        require(proof.get(key)is True,'Incomplete rainforest placement proof: '+key)
    require(proof.get('full_footprint_water_grid')==[5,5]and proof.get('terrain_probes',0)>=RAINFOREST_RECORDS*25,
            'Rainforest full-footprint terrain/water coverage weakened')
    for key,minimum in(('minimum_complete_trail_clearance_m',.4),('minimum_old_rigid_gap_m',0)):
        require(common.finite_number(proof.get(key))and proof[key]>=minimum,'Rainforest clearance weakened: '+key)
    protected=lambda rows:{s['file']:s['sha256']for s in rows}
    require(len(proof['protected_sources'])==7 and protected(proof['protected_sources'])==protected(manifest['protected_sources']),
            'Rainforest proof does not preserve all seven prior source hashes')
    require(proof['generator']=='tools/demo_rainforest_finish.py'and common.digest(ROOT/proof['generator'])==proof['generator_sha256'],
            'Rainforest generator changed after authoring')
    return proof

def check_arcade_route(manifest):
    """The whole new public branch must clear all seven old rigid sources."""
    proof=manifest['proof']['new_arcade_whole_route']
    for key in('all_seven_previous_rigid_sources_checked','new_rigid_source_footprints_checked',
               'actual_model_bounds','dry_full_walking_width'):
        require(proof.get(key)is True,'Incomplete whole-arcade-route proof: '+key)
    require(proof.get('public_half_width_m')==manifest['new_arcade_route_half_width_m']==1.3
            and proof.get('required_extra_clearance_m')==proof.get('base_pedestrian_guard_m')==.4,
            'Whole arcade public-width/clearance envelope weakened')
    require(proof.get('lanes')==3 and proof.get('sampled_step_m')==.25
            and proof.get('capsule_radius_m')==.37 and proof.get('outer_lane_offset_m')==.9,
            'Whole arcade three-lane capsule sampling weakened')
    require(proof.get('source_obb_segment_pairs')==10272 and proof.get('terrain_checks')==1329,
            'Whole arcade old/new rigid or dry-terrain coverage differs')
    for key in('minimum_previous_source_extra_clearance_m','minimum_new_source_extra_clearance_m'):
        require(common.finite_number(proof.get(key))and proof[key]>=.4,'Whole arcade rigid clearance weakened: '+key)
    points=manifest['new_arcade_route_local_xz']
    length=sum(math.dist(a,b)for a,b in zip(points,points[1:]))
    require(len(points)==9 and points[0]==proof.get('entry_local_xz')==[-95,36]
            and points[-1]==proof.get('exit_local_xz')==[-95,-20]
            and common.finite_number(proof.get('length_m'))and abs(proof['length_m']-length)<1.e-6
            and abs(length-108.20621301305597)<1.e-6,'Complete108m arcade entry/exit route differs')
    require(common.finite_number(proof.get('terrain_min_m'))and common.finite_number(proof.get('terrain_max_m'))
            and 0<proof['terrain_min_m']<=proof['terrain_max_m'],'Whole arcade terrain support invalid')
    return proof

def arcade_return_route(manifest):
    """Derive the exact out-and-back walk from the verified generated roads."""
    check_arcade_route(manifest)
    points=manifest['new_arcade_route_local_xz']
    roads=manifest['new_ground']['roads']
    require(len(roads)==8 and all(r['a']==a and r['b']==b and r['half_width']==1.3 and r['paving']==.8
            for r,a,b in zip(roads,points,points[1:])), 'Arcade material road/route coordinates differ')
    path=points+points[-2::-1]
    length=sum(math.dist(a,b)for a,b in zip(path,path[1:]))
    walking_frames=math.ceil(length/3.5*60)
    frames=math.ceil((walking_frames+WARMUP+120)/120)*120
    require(len(path)==17 and path[0]==path[-1]==[-95,36] and path[8]==[-95,-20]
            and abs(length-216.41242602611194)<1.e-6 and walking_frames==3710 and frames==3960,
            'Reviewed full arcade out-and-back route differs')
    return {'id':'valdoa_bakers_arcade_return','points_local_xz':path,'waypoints':len(path),
            'length_m':length,'walking_frames':walking_frames,'frames':frames}

def check_arcade_native_route(world,frames,expected):
    route=world['benchmark_route']
    require(frames==expected['frames']==3960 and route.get('active')is True
            and route.get('complete')is True and route.get('id')==expected['id'],
            'Full3960-frame arcade benchmark did not finish')
    require(route.get('waypoints_reached')==expected['waypoints']-1==16,
            'Arcade walk did not reach all17 authored points')
    require(common.finite_number(route.get('length_m'))and abs(route['length_m']-expected['length_m'])<.000001
            and abs(route.get('speed_mps',0)-3.5)<.000001 and abs(route.get('step_seconds',0)-1/60)<1.e-9,
            'Arcade route length/speed/simulation step differs')
    for key in('collision_blocked_steps','slope_blocked_steps','sliding_steps'):
        require(route.get(key)==0,'Arcade walk blocked/slid: '+key)
    require(route.get('walking_frames')==expected['walking_frames']==3710
            and expected['walking_frames']<=route.get('movement_commands',0)<=expected['walking_frames']+16,
            'Arcade walk skipped normal movement commands/frames')
    require(route.get('waiting_frames')==WARMUP and world.get('travel_loading_frames')==0,
            'Arcade walk had extra holds or skipped warmup')
    for key in('attempted_distance_m','actual_distance_m'):
        require(common.finite_number(route.get(key))and abs(route[key]-expected['length_m'])<.002,
                'Arcade walking distance differs: '+key)
    require(common.finite_number(route.get('return_error_m'))and route['return_error_m']<.001,
            'Arcade walker did not return to the actual starting capsule')
    return route

@contextmanager
def idle_lock():
    """Only metadata is inspected between jobs; every full hash needs idle GL."""
    lock=ROOT/'projects/seldonne/.valwood-instance.lock'
    require(lock.parent.is_dir(),'Prepare the shared instance-lock directory first')
    with lock.open('a')as stream:
        try:fcntl.flock(stream.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError as error:raise ValueError('Close Valwood before source validation/fingerprinting')from error
        try:yield
        finally:fcntl.flock(stream.fileno(),fcntl.LOCK_UN)

def static_check():
    # Retain the old six-source contracts and the original520 model/preference
    # hashes, then the seventh purchased source/cooker provenance unchanged.
    polish=json.loads((SCENERY/'demo_polish_manifest.json').read_text())
    old,route=bep.static_check(authored_updates=polish['authored_input_updates'])
    require(len(old['sources'])==7 and old['combined_records']==BASELINE_RECORDS,'Expected frozen seven-source baseline6660')
    manifest=json.loads(MANIFEST.read_text())
    path=ROOT/manifest['source_file'];records=camp.read_records(path)
    require(path==SCENERY/'demo_finish_instances.txt','Unexpected eighth source path')
    require(manifest.get('schema_version')==1 and manifest.get('original_sources_unchanged')is True,'Invalid finishing manifest contract')
    require(common.digest(path)==manifest['source_sha256'],'Finishing source hash differs')
    require(manifest['record_count']==manifest['records']==len(records)==FINISHING_RECORDS,'Expected complete582-record finishing layer')
    counts=Counter(r[0]for r in records)
    require(dict(counts)==manifest['aliases'],'Finishing alias inventory differs')
    require(len(manifest['placements'])==len(records),'Finishing pose inventory differs')
    require(len({p['id']for p in manifest['placements']})==FINISHING_RECORDS,'Finishing pose IDs repeat')
    for pose,row in zip(manifest['placements'],records):
        expected=[pose['world_xz'][0]/100800+.5,pose['world_xz'][1]/(100800*2818/5009)+.5,
                  pose['yaw_degrees'],pose['scale'],pose['burial_m'],pose['elevation_m']]
        require(pose['alias']==row[0] and all(abs(a-b)<1.1e-6 for a,b in zip(row[1:],expected)),
                'Finishing pose differs from actual runtime source')
    protected=manifest['protected_sources']
    require(len(protected)==7 and {Path(s['file']).name for s in protected}==set(old['sources']),
            'Finishing proof does not preserve all seven prior sources')
    combined=counts.copy()
    for source in protected:
        previous=ROOT/source['file'];name=previous.name
        require(previous==SCENERY/name and common.digest(previous)==source['sha256'],'Protected source changed: '+name)
        prior=camp.read_records(previous)
        require(len(prior)==source['records']==old['sources'][name],'Protected source inventory changed: '+name)
        combined.update(r[0]for r in prior)
    require(sum(combined.values())==manifest['combined_record_count']==COMBINED_RECORDS and max(combined.values())<=4096,
            'Finishing source/alias capacity contract differs')
    zones=dict(Counter(p['zone']for p in manifest['placements']))
    require(zones==manifest['semantic_zones']and len(zones)>=22 and sum(zones.values())==FINISHING_RECORDS,'Finished semantic zones incomplete')
    rainforest=check_rainforest_finish(manifest)
    require(sum(n for zone,n in zones.items()if not zone.startswith('rainforest_'))==CASTLE_FINISHING_RECORDS,
            'Expected complete382-record castle finishing layer')
    architecture=manifest['architecture']
    require(architecture['infill_houses']==28 and architecture['additional_market_canopies']==8,
            'Finished frontages/market canopies missing')
    require(architecture.get('source_models_reused')is True and architecture.get('new_images')==architecture.get('new_model_families')==0,
            'Finishing texture/model reuse budget differs')
    ground=manifest['new_ground'];require(ground['height_edits']is False,'Unexpected street height edits')
    require(common.digest(ROOT/ground['include'])==ground['sha256'],'Finishing ground include differs')
    proof=manifest['proof']
    for key in('actual_runtime_bounds','full_source_roof_obb_separation','production_cpu_atlas',
               'existing_routes_preserved','cultivated_fields_preserved','existing_outer_moat_preserved'):
        require(proof.get(key)is True,'Incomplete authored geometry proof: '+key)
    require(proof['minimum_public_road_clearance_m']>=.4 and proof['minimum_actor_route_clearance_m']>=.4,
            'Authored road/actor clearance weakened')
    poses={p['id']:p for p in manifest['placements']}
    for key,flag,count in(('triangle_supported_props','actual_source_triangle_probe',None),
                           ('triangle_mounted_facade_details','actual_source_wall_triangle_probe',15)):
        details=proof[key]
        require(bool(details)and(count is None or len(details)==count),'Missing measured source-triangle details: '+key)
        require(len({p['id']for p in details})==len(details)and all(p.get(flag)is True and p['id']in poses and p['parent']in poses
                and poses[p['id']]['parent']==p['parent']and p['clearance_m']>0 for p in details),
                'Unsupported source-triangle attachment: '+key)
    arcade=architecture['compact_arcade']
    require(arcade['mounted_facade_details']==15 and arcade['source_triangle_supported_counter_goods']==18,
            'Finished arcade facade/counter dressing missing')
    require(manifest['new_arcade_route_half_width_m']==arcade['public_half_width_m']==1.3
            and len(manifest['new_arcade_route_local_xz'])==9
            and sum(math.dist(a,b)for a,b in zip(manifest['new_arcade_route_local_xz'],manifest['new_arcade_route_local_xz'][1:]))>95,
            'Complete public arcade route missing')
    check_arcade_route(manifest)
    require(abs(proof['protected_castle_walk_length_m']-old['route_length_m'])<.0001,'Original585m return walk differs')
    for name,digest in proof['runtime_actor_route_sha256'].items():
        require(common.digest(ROOT/'engine'/name)==digest,'Actor source changed after finishing layout: '+name)
    require(common.digest(ROOT/'engine/world_village_life_data.inc')==proof['stationary_actor_setup_sha256'],
            'Stationary actor setup changed after authoring')
    for model in manifest['models'].values():
        for lod in model['lods']:
            require(common.digest(ROOT/lod['file'])==lod['sha256'],'Reused model changed: '+lod['file'])
    require({c['id']for c in manifest['inspection_cameras']}=={CASES[n][0]for n in('finished_arcade','finished_west','finished_east')},
            'Three finished inspection views missing')
    for shader in(ROOT/'shaders').glob('world_*.glsl'):
        actual=common.PROJECT/'shaders'/shader.name
        require(actual.is_file()and common.digest(actual)==common.digest(shader),'Actual project shader differs: '+shader.name)
    # This is validation-only. It invokes the production CPU atlas in a temp
    # directory before rendering; never cooks meshes or edits runtime inputs.
    subprocess.run([sys.executable,str(ROOT/'tools/build_demo_finish.py'),'--validate-only'],check=True)
    polish_path=common.PROJECT/'scenery/demo_polish_instances.txt'
    polish=json.loads((common.PROJECT/'scenery/demo_polish_manifest.json').read_text())
    require(common.digest(polish_path)==polish['source_sha256'] and polish['source_files']==9,'Stale polish source')
    for source,digest in polish['protected_sources'].items():
        require(common.digest(ROOT/source)==digest,'Earlier source changed after polish: '+source)
    if 'composition_revision' in polish:
        composition=json.loads((ROOT/polish['composition_revision']).read_text())
        require(polish['new_images']==0 and polish['new_model_files']==6,'Composition sign/source-image budget differs')
        require(composition['base_rows']==535 and 0<composition['added_rows']<=150 and composition['existing_rows_byte_preserved'] is True,
                'Composition changed the original authored placements or exceeded its addition budget')
        require(polish['record_count']==composition['base_rows']+composition['added_rows'] and composition['new_texture_images']==0,
                'Composition inventory or image reuse differs')
        subprocess.run([sys.executable,str(ROOT/'tools/refine_demo_composition.py'),'--validate-only'],check=True)
    else:
        require(polish['new_images']==polish['new_model_files']==0,'Polish must reuse owned cooked models/images')
        subprocess.run([sys.executable,str(ROOT/'tools/build_demo_polish.py'),'--validate-only'],check=True)
    return {**old,'sources':{**old['sources'],path.name:FINISHING_RECORDS,polish_path.name:polish['record_count']},'combined_records':COMBINED_RECORDS+polish['record_count'],
            'finishing_records':FINISHING_RECORDS,'castle_finishing_records':CASTLE_FINISHING_RECORDS,
            'finishing_aliases':dict(counts),'finishing_zones':zones,
            'finishing_geometry_proof':proof,'rainforest_finish':rainforest,'source_count':9,'polish':polish,
            'arcade_route':arcade_return_route(manifest)},route

def input_paths():
    result=set(common.input_paths())
    result.update(ROOT/'tools'/name for name in EXTRA_TOOLS)
    result.add(ROOT.parent/'Launch_Valwood_Demo.sh')
    result.add(ROOT.parent/'Launch_Valwood_Jungle_Camp.sh')
    result.add(common.PROJECT/'graphics.settings')
    result.add(common.PROJECT/'assets/map.jpg')
    for directory in(ROOT/'third_party/bc7enc16',common.PROJECT/'texture_blocks'):
        if directory.is_dir():result.update(p for p in directory.rglob('*')if p.is_file())
    for name in('cook_world_texture_blocks.cpp','world_texture_blocks_encoder.h'):
        path=ROOT/'tools'/name
        if path.is_file():result.add(path)
    for directory in(common.PROJECT/'shaders',common.PROJECT/'particles',common.PROJECT/'audio'):
        result.update(p for p in directory.rglob('*')if p.is_file())
    return sorted(result)

def metadata():
    result={}
    for path in input_paths():
        s=path.stat();result[common.label(path)]=[s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
    return result

def fingerprints():
    """Caller holds idle_lock across this complete runtime+audio fingerprint."""
    return {common.label(path):common.digest(path)for path in input_paths()}

def check_main_preparation(world,view):
    p=world.get('main_material_preparation',{})
    require(p.get('ready')is True and p.get('failures')==0,'Main material loading preparation failed')
    require(p.get('startup_calls')==p.get('calls')==1 and p.get('destination_calls')==0
            and p.get('passes')==4,'Stationary/walking case missed four startup material headings')
    require(p.get('headings_per_call')==4 and p.get('main_color_attachments')==2,
            'Preparation did not use the actual main MRT layout')
    require(p.get('loading_only')is True and p.get('walking_fences')==0
            and p.get('temporal_history_unchanged')is True,'Material preparation changed ready-play pacing/history')
    require(p.get('terrain_passes')==4 and p.get('static_model_passes')==4,
            'Main terrain/authored model layouts were not exercised')
    for name in('procedural_grass_passes','scanned_tuft_passes','near_tree_passes','pose_passes'):
        require(isinstance(p.get(name),int)and 0<=p[name]<=4,'Invalid material layout coverage: '+name)
    if view.startswith('castle-'):require(p['pose_passes']==4,'Castle startup missed actor pose layouts')
    require(common.finite_number(p.get('milliseconds'))and p['milliseconds']>0,'Missing loading preparation duration')
    return p

def check_texture_blocks(world):
    p=world.get('texture_blocks',{})
    require(p.get('enabled')is True and p.get('offline_encoder')is True,
            'Offline model texture blocks were not enabled')
    require(p.get('full_resolution')is True, 'Texture resolution differs')
    preservation_fields=('all_mip_levels_preserved','linear_data_requires_certificate',
                         'masked_color_maps_original_rgba','facade_auxiliary_linear_maps_original_rgba')
    if any(name in p for name in preservation_fields):
        require(all(p.get(name)is True for name in preservation_fields)
                and p.get('runtime_encoding')is False
                and p.get('counter_scope')=='cumulative_immediate_upload_attempts',
                'Certified linear texture policy or immediate counter scope differs')
    else:
        # Historical opaque-only reports predate certified linear admission.
        # Retain compatibility without treating that old field as evidence of
        # the current normal/ARM policy or staged texture cache counts.
        require(p.get('linear_and_masked_maps_unchanged')is True,
                'Legacy opaque texture preservation claim is missing')
    for name in('candidates','cached_textures','reference_rgba_bytes','block_bytes','cache_misses','rejected_cache','upload_failures'):
        require(isinstance(p.get(name),int)and p[name]>=0,'Invalid texture block counter: '+name)
    require(p['cached_textures']>0 and p['reference_rgba_bytes']>p['block_bytes']>0,
            'No validated compressed model textures were uploaded')
    require(p['rejected_cache']==p['upload_failures']==0,'Rejected/corrupt cache or compressed GL upload failure')
    require(p['candidates']==p['cached_textures']+p['cache_misses'],
            'Texture upload candidate accounting differs')
    require(p['block_bytes']<=p['reference_rgba_bytes']*.27,
            'Full-chain BC7 storage did not achieve its expected footprint')
    return {**p,'saved_gpu_bytes':p['reference_rgba_bytes']-p['block_bytes']}

def check_source_storage(world):
    p=world['texture_source_storage'];shared=world['wildlife']
    require(p['file_bytes']==shared['scene_cache_source_bytes']>300*1024*1024
            and p['file_entries']==shared['scene_cache_entries']>300,
            'Exact encoded source keys are not in the reclaimable file arena')
    require(p['heap_bytes']==p['trim_failures']==0 and p['loading_trims']==world['main_material_preparation']['calls']>0,
            'Source storage fallback or loading trim failure')
    require(all(p[k]is True for k in('read_only_exact_keys','fixed_budget','gpu_images_unchanged')),
            'Source storage weakened equality/budget/image contract')
    return p

def check_rainforest_atmosphere(world,view):
    p=world.get('rainforest_atmosphere',{})
    require(p.get('smooth_biome_transition')is True and p.get('quality_settings_respected')is True,
            'Missing smooth quality-aware regional atmosphere')
    for name in('weight','blend','cloud','sun_multiplier','sky_multiplier','haze','mist_weight'):
        require(common.finite_number(p.get(name)),'Invalid atmosphere value: '+name)
    for name in('fog_color','sky_zenith'):
        require(len(p.get(name,[]))==3 and all(common.finite_number(v)and v>=0 for v in p[name]),
                'Invalid regional atmosphere colour: '+name)
    if view.startswith('rainforest'):
        for name,expected in(('weight',1),('blend',1),('cloud',.96),('sun_multiplier',.36),('sky_multiplier',.74),('mist_weight',2)):
            require(abs(p[name]-expected)<1.e-5,'Deep-jungle regional weather differs: '+name)
        require(.0008<p['haze']<.001 and abs(world.get('fog_strength',0)-1.24)<1.e-5,
                'Deep jungle lacks strong aerial/ground mist')
        require(max(p['fog_color'])<.25 and max(p['sky_zenith'])<.11,
                'Canopy fog/sky radiance exceeds subdued overcast look')
    else:
        require(p['weight']==p['blend']==p['mist_weight']==0 and p['sun_multiplier']==p['sky_multiplier']==1
                and abs(p['cloud']-world['lighting']['cloud'])<1.e-6,'Exterior lighting was changed by jungle profile')
    return p

def check_probes(world,frames,required):
    p=world.get('local_probes')
    if p is None:
        require(not required,'Missing required local_probes telemetry')
        return {'present':False,'required':False}
    require(p.get('enabled')is True and p.get('ordinary_mip_reflections')is True and p.get('sh9_diffuse')is True,
            'Local reflection/SH feature disabled or contract differs')
    require(p.get('count')in(2,6,8)and p.get('ready')==p['count']and p.get('side')==128,'Incomplete128-square scene probe bank')
    require(p.get('cache_regions')in(1,2)and 0<p.get('bytes',0)<=8*1024*1024,'Probe region/cache memory budget differs')
    require(p.get('faces')==p.get('readbacks')==p['ready']*6,'Probe face/readback inventory differs')
    require(p.get('failures')==0 and p.get('walking_bakes')==0,'Probe failure or capture during walking')
    require(p.get('loading_bakes')==p.get('capture_calls')==p['cache_regions'],'Probe loading bake count differs')
    require(isinstance(p.get('bind_calls'),int)and p['bind_calls']>=frames,'Local probes were not bound during play')
    return {'present':True,'required':required,**p}

def check_audio(world,frames,run,required):
    a=world.get('demo_audio')
    if a is None:
        require(not required,'Missing required demo_audio telemetry')
        return {'present':False,'required':False}
    require(a.get('enabled')is True and a.get('ready')is True,'Demo audio failed to initialize')
    require(isinstance(a.get('driver'),str)and a['driver'],'Missing actual SDL audio driver')
    require(a.get('loaded_beds')==8 and a.get('loaded_steps')==12 and a.get('voice_capacity')==12,
            'Missing bounded ambience/footstep bank')
    require(a.get('pcm_bytes')==23221312,'Decoded PCM inventory differs')
    require(0<=a.get('active_voices',-1)<=12 and 0<=a.get('fire_sources',-1)<=40,'Audio voice/source budget exceeded')
    require(a.get('updates')==frames and a.get('update_failures')==0,'Audio missed frames or update failed')
    require(isinstance(a.get('zone_samples'),int)and 1<=a['zone_samples']<=frames,'Audio zone sampling did not run')
    require(a.get('distance_based_footsteps')is True and a.get('focus_muting')is True,'Audio cadence/focus contract differs')
    require(isinstance(a.get('steps'),int)and a['steps']>=0,'Invalid footstep count')
    require(len(a.get('surface_steps',[]))==4 and all(isinstance(v,int)and v>=0 for v in a['surface_steps'])and sum(a['surface_steps'])==a['steps'],
            'Footstep surface counters differ')
    for key in('gains','targets'):
        values=a.get(key,[])
        require(len(values)==8 and all(common.finite_number(v)and 0<=v<=1.12001 for v in values)and sum(values)<=1.12001,
                'Ambience gain budget exceeded: '+key)
    walked=a.get('walked_m');require(common.finite_number(walked)and walked>=0,'Invalid audio walking distance')
    if run in('route','arcade_route'):
        require(walked>(580 if run=='route'else 216)and abs(a['steps']-math.floor(walked/1.2))<=1,'Grounded distance-based route footstep cadence differs')
    else:require(a['steps']==0 and walked<.0001,'Stationary camera generated footsteps')
    return {'present':True,'required':required,'dummy_backend':a['driver']=='dummy','audible_device_review_performed':False,**a}

def load_timing(stem,frames):
    """Independently retain every declared post-warmup CSV interval and tail."""
    reported=camp.load_timing(stem,frames)
    require(reported.get('frames')==frames and reported.get('warmup_frames')==WARMUP,'Declared frame/warmup contract differs')
    with Path(str(stem.with_suffix('.json'))+'.csv').open(newline='')as stream:rows=list(csv.DictReader(stream))
    require(len(rows)==frames and [int(r['frame'])for r in rows]==list(range(1,frames+1)),'CSV has missing/repeated/reordered frames')
    measured=[float(r['frame_ms'])for r in rows[WARMUP:]]
    require(len(measured)==reported['samples']and all(math.isfinite(v)and v>0 for v in measured),'CSV lost/invalidated measured intervals')
    ordered=sorted(measured);n=len(ordered);low_count=math.ceil(n*.01)
    derived={'samples':n,'mean_ms':sum(measured)/n,'max_ms':ordered[-1],
        'p99_ms':ordered[math.ceil(n*.99)-1],'mean_fps':1000/(sum(measured)/n),
        'one_percent_low_fps':1000/(sum(ordered[-low_count:])/low_count),
        'frames_over_45fps_budget':sum(v>1000/45 for v in measured),
        'frames_over_30fps_budget':sum(v>1000/30 for v in measured)}
    for key in('mean_ms','max_ms','p99_ms'):
        require(abs(reported[key]-derived[key])<=.00052,'Profile summary does not match full CSV: '+key)
    for key in('mean_fps','one_percent_low_fps'):
        require(abs(reported[key]-derived[key])<=.0052,'Profile FPS does not match full CSV: '+key)
    for key in('frames_over_45fps_budget','frames_over_30fps_budget'):
        require(reported[key]==derived[key],'Profile hitch counters differ from full CSV: '+key)
    return {**reported,'verified_csv':str(stem.with_suffix('.json'))+'.csv','csv_recomputed':derived,
            'strict_45fps_pass':derived['mean_fps']>=45 and derived['one_percent_low_fps']>=45,
            'all_frames_within_45fps_budget':derived['max_ms']<=1000/45}

def main(argv=None):
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--native',action='store_true',help='Launch sequential native AMD jobs after static validation')
    cli.add_argument('--frame-times-only',action='store_true',help='Keep all frame/CPU intervals; omit detailed GPU-stage query rings, matching ordinary manual-quality play')
    cli.add_argument('--captures',action='store_true',help='Untimed fixed-time screenshots; no FPS claim')
    cli.add_argument('--runs',nargs='+',choices=tuple(CASES),help='Default full11-case suite; captures exclude both walking routes')
    cli.add_argument('--frames',type=int,help='Stationary frames; default1800 timed/180 capture; both walking routes always complete')
    cli.add_argument('--require-probes',action=argparse.BooleanOptionalAction,default=True,help='Require complete scene-probe telemetry (default enabled)')
    cli.add_argument('--require-audio',action=argparse.BooleanOptionalAction,default=True,help='Require ready audio telemetry; dummy driver allowed (default enabled)')
    cli.add_argument('--output',type=Path,default=EVIDENCE/'native-final')
    args=cli.parse_args(argv)
    require(not args.captures or args.native,'--captures requires --native')
    require(not args.frame_times_only or(args.native and not args.captures),'--frame-times-only requires native timed rendering')
    require(args.frames is None or args.frames>(0 if args.captures else WARMUP),'Insufficient stationary frame count')
    runs=args.runs or[n for n in CASES if not args.captures or n not in('route','arcade_route')]
    require(len(runs)==len(set(runs)),'Duplicate cases')
    require(not args.captures or not{'route','arcade_route'}.intersection(runs),'Full walking routes belong to timed acceptance')
    with idle_lock():
        static,route=static_check();preferences=streets.check_saved_preferences()
        if not args.native:print(json.dumps({'static_pass':True,**static},indent=2));return 0
        output=args.output.expanduser().resolve()
        require(not output.exists()or not any(output.iterdir()),'Choose a fresh evidence directory')
        for p in(ROOT/'engine',ROOT/'shaders',common.PROJECT):require(not output.is_relative_to(p),'Evidence overlaps frozen runtime inputs')
        before=metadata();frozen=fingerprints();require(metadata()==before,'Inputs changed while freezing final runtime')
    output.mkdir(parents=True,exist_ok=True);environment,removed=common.native_environment()
    if args.frame_times_only:environment['VALWOOD_QA_FRAME_TIMES_ONLY']='1'
    (output/'source-freeze.json').write_text(json.dumps({'sha256':frozen,'preferences_before':preferences,'static':static,
        'removed_environment_keys':removed,'require_probes':args.require_probes,'require_audio':args.require_audio,
        'saved_profile':'Custom85% manual60FPS,2Kshadow,halfreflection,contact/fog/clouds; no graphics overrides',
        'scope':'All engine sources including probes/audio/viewer, Makefile, executable, root+actualproject shaders, allmodels/scenery/ground/particles/audio, graphics.settings and acceptance/generator tools.'},indent=2)+'\n')
    rows=[];failures=[];monitor=common.MemoryMonitor();monitor.start()
    try:
        for name in runs:
            view,tod=CASES[name];frames=(streets.route_frames(route)if name=='route'else static['arcade_route']['frames']if name=='arcade_route'else(args.frames or(180 if args.captures else 1800)))
            stem=output/(name+'-'+view);env=environment.copy();env['VALWOOD_VILLAGE_LIGHTING']='dusk'if tod=='dusk'else'afternoon'
            if args.captures:env['VALWOOD_QA_FIXED_TIME']='1'
            command=[str(common.LAUNCHER),'--scenic',view,'--width','1600','--height','900','--frames',str(frames),'--no-hud','--capture',str(stem.with_suffix('.bmp'))]
            if not args.captures:command+=['--benchmark','--view','river'if name in('route','arcade_route')else'sweep','--warmup',str(WARMUP),'--profile',str(stem.with_suffix('.json'))]
            row={'run':name,'view':view,'frames':frames,'timed':not args.captures,'command':command}
            print(f'Native finished demo {name}: {frames}frames',flush=True)
            try:
                with stem.with_suffix('.log').open('x')as log:process=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT)
                require(process.returncode==0,f'Engine exit{process.returncode}; log retained')
                if not args.captures:
                    # Retain timing even if a separate rendering/audio contract
                    # fails afterward. A correctness failure cannot hide hitches.
                    row['timing']=load_timing(stem,frames)
                    row['strict_45fps_pass']=row['timing']['strict_45fps_pass']
                    row['all_frames_within_45fps_budget']=row['timing']['all_frames_within_45fps_budget']
                if view.startswith('castle-'):
                    row.update(streets.native_world_check(stem,frames,not args.captures,name,view,tod,static,True,validate_early=True,expected_hour_override=17.8 if tod=='dusk'else 16.65,route_contract=static['arcade_route']if name=='arcade_route'else None))
                else:row.update(particles.other_world_check(stem,frames,not args.captures,view))
                world=json.loads(Path(row['world_report']).read_text())
                require({Path(s['path']).name:s['instances']for s in world['authored_sources']}==static['sources']and world['authored_instances']==static['combined_records'],
                        'Final nine-source inventory absent from renderer')
                require(world['texture_cache_unmanaged_uploads']==0,'Texture cache duplicated unmanaged images')
                submit=world.get('early_gpu_submission',{})
                require(submit.get('enabled')is True and submit.get('frames')==frames and submit.get('waits_for_completion')is False,
                        'Early nonblocking submission contract differs')
                row.update(particles.check_lighting(world,frames,args.captures));row.update(particles.check_particles(world,frames))
                if view.startswith('rainforest'):require(world['authored_depth_first']['frames']==frames,'Rainforest missed authored-first frames')
                row['local_probes']=check_probes(world,frames,args.require_probes)
                row['demo_audio']=check_audio(world,frames,name,args.require_audio)
                if name=='arcade_route':row['arcade_route']=check_arcade_native_route(world,frames,static['arcade_route'])
                row['main_material_preparation']=check_main_preparation(world,view)
                row['texture_blocks']=check_texture_blocks(world)
                row['texture_source_storage']=check_source_storage(world)
                row['rainforest_atmosphere']=check_rainforest_atmosphere(world,view)
                require(world.get('atlas_map')==dict(loaded=False,attempts=0,visible=False,tools_visible=False,qa_events=0,lazy=True),
                        'Ordinary demo loaded or displayed the optional atlas')
                row['correctness_pass']=True
            except(OSError,ValueError,KeyError,TypeError)as error:row.update(correctness_pass=False,error=str(error))
            rows.append(row)
            print(json.dumps({k:row.get(k)for k in('run','correctness_pass','strict_45fps_pass','error')},separators=(',',':')),flush=True)
            if metadata()!=before or common.preference_state()!=preferences:failures.append('Runtime/preferences changed; subsequent cases cancelled');break
    finally:
        monitor.stop();(output/'gpu-memory.json').write_text(json.dumps(monitor.samples,indent=2)+'\n')
        try:
            with idle_lock():require(fingerprints()==frozen,'Final runtime fingerprints changed')
            require(common.preference_state()==preferences,'Saved preferences changed')
        except(OSError,ValueError)as error:failures.append(str(error))
        correct=len(rows)==len(runs)and all(r.get('correctness_pass')for r in rows)and not failures
        gate=None if args.captures else correct and all(r.get('strict_45fps_pass')for r in rows)
        (output/'results.json').write_text(json.dumps({'correctness_pass':correct,'strict_45fps_pass':gate,'untimed':args.captures,
            'all_frames_within_45fps_budget':None if args.captures else correct and all(r.get('all_frames_within_45fps_budget')for r in rows),
            'preferences_preserved':common.preference_state()==preferences,'required_features':{'probes':args.require_probes,'audio':args.require_audio},
            'static':static,'runs':rows,'failures':failures,'memory':monitor.summary(),
            'timing_scope':'All frame intervals after exactly120warmup retained and independently recomputed from completeCSV, including every hitch; average and slowest1% must each reach45FPS. Per-frame45FPS budget reported separately. No cook/build/hash work during native rendering. Audio dummy initialization is not an audible device review.'},indent=2)+'\n')
    return 0 if correct and gate is not False else 1

if __name__=='__main__':
    try:sys.exit(main())
    except(OSError,ValueError,KeyError,TypeError,subprocess.CalledProcessError)as error:raise SystemExit(str(error))from error
