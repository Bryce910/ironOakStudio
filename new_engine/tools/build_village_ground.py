#!/usr/bin/env python3
"""Bake bounded material history from the authored scene's real ground sections.

Writes only a separate linear RGBA8 mask/manifest. No scene placements, terrain
geometry, collision includes or source material images are modified. --validate
rebuilds in memory and checks exact published bytes without publishing.
"""
import argparse,hashlib,json,math,sys
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from scipy.ndimage import distance_transform_edt
from build_starting_area import glb_bounds
from build_castle_village import watch_ground_rectangles

ROOT=Path(__file__).resolve().parents[1];PROJECT=ROOT/'projects/valwood_world'
SOURCE=PROJECT/'scenery/village_ground_history.json'
HERO_SOURCE=PROJECT/'scenery/village_hero_ground.json'
SCENE=PROJECT/'scenery/castle_village_manifest.json'
LAYOUT=PROJECT/'scenery/castle_village_layout.json'
OUTPUT=PROJECT/'ground/village_ground_history.png'
MANIFEST=PROJECT/'scenery/village_ground_history_manifest.json'

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def smooth(a,b,x):
    t=np.clip((x-a)/(b-a),0,1);return t*t*(3-2*t)

def section_segments(triangles,height):
    """Actual Y-plane intersections, so overhanging roofs cannot stamp soil."""
    output=[]
    for triangle in triangles:
        points=[]
        for a,b in zip(triangle,triangle[1:]+triangle[:1]):
            if min(a[1],b[1])<=height<max(a[1],b[1]):
                t=(height-a[1])/(b[1]-a[1]);point=(a[0]+t*(b[0]-a[0]),a[2]+t*(b[2]-a[2]))
                if not any(math.dist(point,p)<1e-7 for p in points):points.append(point)
        if len(points)==2 and math.dist(*points)>1e-6:output.append(points)
    return output

def watch_contact_sections(scene):
    """Union of real boot-height intersections at each fixed runtime post."""
    metadata=scene.get('village_reinforcements')
    if not metadata:return [],{}
    source=metadata['source_manifests']['watch'];path=ROOT/source['file']
    assert sha(path)==source['sha256'],'Watch inventory changed before ground authoring'
    manifest=json.loads(path.read_text());assert manifest['pose_count']==8 and len(manifest['assets'])==8
    segments=[];hashes={}
    for pose in manifest['assets']:
        model=PROJECT/'models'/pose['file'];assert sha(model)==pose['sha256']
        _,_,_,triangles=glb_bounds(model,True)
        measured=section_segments(triangles,.10);assert measured,('Missing real watch boot contact',pose['file'])
        segments+=measured;hashes[pose['file']]=pose['sha256']
    contacts=[]
    for row in watch_ground_rectangles(metadata):
        station=row['station'];c,s=math.cos(station['yaw']),math.sin(station['yaw']);world=[]
        for a,b in segments:
            world.append([[station['x']+c*x+s*z,station['z']-s*x+c*z]for x,z in [a,b]])
        contacts.append(dict(label=row['id'],alias='village_shield_knight_idle_union',type='working-contact',
            virtual_runtime_station=True,source_pose_count=8,source_height_m=.1,world_height_m=.1,
            section_segments=len(world),world_segments=world,source_pose_sha256=hashes,
            grass_clear_rectangle=row,section_bounds_local_xz=[min(p[0]for segment in world for p in segment),
                min(p[1]for segment in world for p in segment),max(p[0]for segment in world for p in segment),
                max(p[1]for segment in world for p in segment)]))
    return contacts,hashes

def segment_distance(x,z,a,b):
    dx,dz=b[0]-a[0],b[1]-a[1];t=np.clip(((x-a[0])*dx+(z-a[1])*dz)/max(dx*dx+dz*dz,1e-9),0,1)
    return np.hypot(x-a[0]-t*dx,z-a[1]-t*dz)

def authored_market_history(channels,x,z,grounded,recipe):
    """Bounded material wear, independent of terrain height or lighting.

    Features are measured in metres and filtered by the existing RGBA history
    texture.  Road-edge soil stays separate from pedestrian burnish; there is
    no new sampler, displaced curb or invented collision surface.
    """
    assert recipe['schema_version']==1
    bounds=recipe['bounds_local_xz'];xmin,zmin,xmax,zmax=bounds
    assert xmin<xmax and zmin<zmax and xmax-xmin<=50 and zmax-zmin<=20
    assert 0<recipe['border_fade_m']<=2.5
    edge=np.minimum.reduce([x-xmin,xmax-x,z-zmin,zmax-z])
    weight=smooth(0,recipe['border_fade_m'],edge)
    # Snapshot channels once so the locality contract also holds at exact
    # zero-weight pixels after floating-point math and byte quantization.
    soil,traffic,damp,sediment=[channel.copy() for channel in channels]
    footprint=np.zeros_like(x);wheel=np.zeros_like(x)
    gutter=np.zeros_like(x);debris=np.zeros_like(x)
    def stroke(feature):
        assert len(feature['points'])>=2 and .16<=feature['width']<=1.5
        assert 0<feature['strength']<=1
        assert 0<feature.get('feather',.3)<=1.5
        assert all(xmin<=a<=xmax and zmin<=b<=zmax for a,b in feature['points'])
        distance=np.minimum.reduce([segment_distance(x,z,a,b) for a,b in zip(feature['points'],feature['points'][1:])])
        return (1-smooth(feature['width'],feature['width']+feature.get('feather',.3),distance))*feature['strength']
    for feature in recipe['footways']:footprint=np.maximum(footprint,stroke(feature))
    for feature in recipe['wheel_runs']:wheel=np.maximum(wheel,stroke(feature))
    for feature in recipe['gutters']:gutter=np.maximum(gutter,stroke(feature))
    for feature in recipe['edge_deposits']:debris=np.maximum(debris,stroke(feature))
    # Frontage wear connects to the real low-height section, rather than the
    # much wider overhanging roof bounds. The endpoints are retained in proof.
    approaches=[]
    for section in grounded:
        house=section['type']=='foundation' and section['label'].startswith('demo_home_market_row_')
        shop=section['alias'].startswith('purchased_shop_')
        if not(house or shop):continue
        sx0,sz0,sx1,sz1=section['section_bounds_local_xz'];cx=(sx0+sx1)*.5
        if not xmin<cx<xmax:continue
        face_z=sz1 if (sz0+sz1)*.5<55 else sz0
        if not zmin<face_z<zmax:continue
        street_z=54.3 if face_z<55 else 55.7
        points=[[cx,face_z],[cx+.35,face_z+(street_z-face_z)*.55],[cx+.15,street_z]]
        feature=dict(points=points,width=.46,strength=.75,feather=.34)
        footprint=np.maximum(footprint,stroke(feature))
        approaches.append(dict(label=section['label'],alias=section['alias'],points=points,source_height_m=section['source_height_m']))
    # Uneven accumulation is broad enough for the 22cm field resolution. All
    # patterns are fixed in world space; no screen/time input can cause shimmer.
    breakup=.76+.16*np.sin(x*1.17+z*.33)*np.cos(z*1.41)+.08*np.sin(x*.43-z*.71)
    soil_target=np.maximum(soil,wheel*.46+debris*.26)
    soil_target=np.maximum(soil,soil_target*breakup)
    traffic_target=np.maximum(footprint,.17*(1-smooth(1.8,2.7,np.abs(z-55))))
    traffic_target*=1-wheel*.65
    damp_target=np.maximum(damp,gutter*(.80+.14*np.sin(x*.57)) +wheel*.22)
    # Short, discontinuous shoulders read as dirt in broken paving joints,
    # not two perfectly straight painted lines down an otherwise flat road.
    sediment_target=np.maximum(sediment,np.maximum(debris*breakup,gutter*.45))
    targets=[soil_target,traffic_target,damp_target,sediment_target]
    result=[np.where(weight>0,old+(new-old)*weight,old) for old,new in zip(channels,targets)]
    proof=dict(recipe_file=str(HERO_SOURCE.relative_to(PROJECT)),recipe_sha256=sha(HERO_SOURCE),
        bounds_local_xz=bounds,border_fade_m=recipe['border_fade_m'],
        frontage_approaches=approaches,feature_counts={key:len(recipe[key]) for key in ['footways','wheel_runs','gutters','edge_deposits']},
        cpu_height_changed=False,new_samplers=0,new_gpu_bytes=0)
    return result,proof

def build(config,scene,layout):
    assert config['schema_version']==1 and config['side']==2049
    xmin,zmin,xmax,zmax=config['bounds_local_xz'];side=config['side']
    assert (xmin,zmin,xmax,zmax)==(-225,-225,225,225)
    dx,dz=(xmax-xmin)/(side-1),(zmax-zmin)/(side-1)
    x,z=np.meshgrid(np.linspace(xmin,xmax,side,dtype=np.float32),np.linspace(zmin,zmax,side,dtype=np.float32))
    layers=[Image.new('L',(side,side)),Image.new('L',(side,side))];draws=[ImageDraw.Draw(im) for im in layers]
    cache={};grounded=[];source_hashes={};skipped_elevated=0;segment_count=0
    masonry={'castle_barracks','castle_quartermaster','village_keep','village_inn','village_smithy','starting_gate','starting_wall','starting_tower','purchased_gate','purchased_wall'}
    cargo={'purchased_barrel','purchased_crate','purchased_prop_sack','purchased_prop_logs','purchased_prop_barrel_small','purchased_prop_lamp','starting_table','starting_stump','starting_crate','starting_barrel','village_cart','village_well'}
    support_offsets={'starting_crate','starting_barrel','starting_bucket','starting_lantern','starting_axe','starting_stump','starting_firepit'}
    for record in scene['placements']:
        alias=record['alias'];foundation=alias in masonry or alias.startswith(('purchased_house_','village_house_'))
        # New fixed bench/horse contacts use real low-height sections, not
        # their full envelopes. Keep this addition local to the append-only
        # reinforcement layer; earlier scene material history is unchanged.
        reinforcement_contact=record['label'].startswith('village_reinforcement_')and alias in {'castle_yard_bench','village_horseman'}
        if not foundation and alias not in cargo and not alias.startswith(('purchased_shop_','village_stall_'))and not reinforcement_contact:continue
        model=scene['models'][alias];path=PROJECT/'models'/model['file']
        if alias not in cache:
            actual=sha(path);assert actual==model['sha256'],('Scene model changed; regenerate scene manifest first',alias)
            lo,hi,_,triangles=glb_bounds(path,True);cache[alias]=(lo,hi,triangles);source_hashes[alias]=actual
        lo,hi,triangles=cache[alias];scale=record['scale'];base=record.get('elevation',0)-record['burial']-(.05 if alias in support_offsets else 0)
        if base+lo[1]*scale>.12:skipped_elevated+=1;continue
        if not (xmin-20<record['x']<xmax+20 and zmin-20<record['z']<zmax+20):continue
        height=(.10-base)/scale
        if height<lo[1] or height>hi[1]:continue
        segments=section_segments(triangles,height)
        # No convenient full-mesh-bound fallback: a mesh with no real grounded
        # section cannot create a contact/deposition mark.
        if not segments:continue
        c,s=math.cos(record['yaw']),math.sin(record['yaw']);world=[]
        for a,b in segments:
            points=[]
            for lx,lz in [a,b]:
                px=record['x']+(c*lx+s*lz)*scale;pz=record['z']+(-s*lx+c*lz)*scale
                points.append(((px-xmin)/dx,(pz-zmin)/dz))
                world.append([px,pz])
            draws[0 if foundation else 1].line(points,fill=255,width=1)
        segment_count+=len(segments)
        grounded.append(dict(label=record['label'],alias=alias,type='foundation' if foundation else 'working-contact',source_height_m=height,world_height_m=.1,section_segments=len(segments),section_bounds_local_xz=[min(p[0] for p in world),min(p[1] for p in world),max(p[0] for p in world),max(p[1] for p in world)]))
    virtual_watch,watch_models=watch_contact_sections(scene)
    for contact in virtual_watch:
        for a,b in contact['world_segments']:
            draws[1].line([((p[0]-xmin)/dx,(p[1]-zmin)/dz)for p in [a,b]],fill=255,width=1)
        segment_count+=contact['section_segments']
        grounded.append({key:value for key,value in contact.items()if key!='world_segments'})
    distances=[]
    for image in layers:
        data=np.asarray(image);assert np.any(data)
        distances.append(distance_transform_edt(data==0,sampling=(dz,dx)).astype(np.float32))
    foundation=1-smooth(.04,1.32,distances[0]);working=1-smooth(.03,.82,distances[1])
    # A broad deterministic modulation is applied only to actual section
    # deposits. It cannot create global noisy patches or camera/time patterns.
    irregular=.86+.14*np.sin(x*.67+np.sin(z*.19))*np.cos(z*.51)
    activity_apron=(1-smooth(.15,2.,distances[1]))*.18
    soil=np.maximum(foundation*.67,working*.46+activity_apron)*irregular
    traffic=np.zeros_like(soil);damp=(1-smooth(.04,.55,distances[0]))*.35
    for feature in config['traffic']:
        assert len(feature['points'])>=2 and .1<=feature['width']<=1.5 and 0<feature['strength']<=1
        distance=np.minimum.reduce([segment_distance(x,z,a,b) for a,b in zip(feature['points'],feature['points'][1:])])
        traffic=np.maximum(traffic,(1-smooth(feature['width'],feature['width']+.45,distance))*feature['strength'])
    for feature in config['drainage']:
        distance=np.minimum.reduce([segment_distance(x,z,a,b) for a,b in zip(feature['points'],feature['points'][1:])])
        damp=np.maximum(damp,(1-smooth(feature['width']*.35,feature['width']+.32,distance))*feature['strength'])
    for feature in config['damp_pockets']:
        distance=np.hypot((x-feature['center'][0])/feature['radii'][0],(z-feature['center'][1])/feature['radii'][1])
        damp=np.maximum(damp,(1-smooth(.15,1,distance))*feature['strength'])
    coverage=np.zeros_like(soil)
    for road in scene.get('roads',layout['roads']):
        distance=segment_distance(x,z,road['a'],road['b']);coverage=np.maximum(coverage,1-smooth(road['half_width'],road['half_width']+1.4,distance))
    for square in scene.get('squares',layout['squares']):
        edge=np.maximum(np.abs(x-square['center'][0])-square['half'][0],np.abs(z-square['center'][1])-square['half'][1])
        coverage=np.maximum(coverage,1-smooth(0,1.2,edge))
    sediment=4*coverage*(1-coverage)*(.62+.19*np.sin(x*.41+z*.22))
    fade=smooth(0,3,np.minimum.reduce([x-xmin,xmax-x,z-zmin,zmax-z]))
    baseline=np.stack([soil,traffic,damp,sediment],axis=-1)*fade[:,:,None]
    hero_recipe=json.loads(HERO_SOURCE.read_text())
    channels,hero_proof=authored_market_history([soil,traffic,damp,sediment],x,z,grounded,hero_recipe)
    data=np.stack(channels,axis=-1)*fade[:,:,None]
    pixels=np.rint(np.clip(data,0,1)*255).astype(np.uint8)
    baseline_pixels=np.rint(np.clip(baseline,0,1)*255).astype(np.uint8)
    changed=np.any(pixels!=baseline_pixels,axis=-1)
    bx0,bz0,bx1,bz1=hero_recipe['bounds_local_xz'];outside=(x<=bx0)|(x>=bx1)|(z<=bz0)|(z>=bz1)
    assert not np.any(changed&outside),'Hero material history escaped its authored bounds'
    hero_proof.update(changed_pixels=int(changed.sum()),outside_changed_pixels=int(np.count_nonzero(changed&outside)),
        channel_changed_pixels=[int(np.count_nonzero(pixels[:,:,i]!=baseline_pixels[:,:,i])) for i in range(4)],
        baseline_pixel_sha256=hashlib.sha256(baseline_pixels.tobytes()).hexdigest())
    mip_side=side;mip_bytes=0
    while True:
        mip_bytes+=mip_side*mip_side*4
        if mip_side==1:break
        mip_side//=2
    proof=dict(schema_version=1,channels=['foundation/cargo soil','traffic burnish','damp drainage','edge sediment'],bounds_local_xz=[xmin,zmin,xmax,zmax],side=side,texel_metres=[dx,dz],gpu_bytes=int(pixels.nbytes),gpu_mip_bytes=mip_bytes,row0='minimum local Z; no image flip',scene_instances_sha256=scene['instances_sha256'],scene_manifest_sha256=sha(SCENE),source_config_sha256=sha(SOURCE),layout_sha256=sha(LAYOUT),model_sha256=source_hashes,grounded_sections=grounded,section_segment_count=segment_count,skipped_elevated_contacts=skipped_elevated,channel_nonzero=[int(np.count_nonzero(pixels[:,:,i])) for i in range(4)],channel_max=[int(pixels[:,:,i].max()) for i in range(4)],pixel_sha256=hashlib.sha256(pixels.tobytes()).hexdigest(),hero_market=hero_proof)
    proof['virtual_watch_contacts']=[{key:value for key,value in contact.items()if key!='world_segments'}for contact in virtual_watch]
    proof['watch_pose_model_sha256']=watch_models
    return pixels,proof

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--validate',action='store_true');parser.add_argument('--output-dir',type=Path,help='Stage the mask and proof here without publishing runtime assets');args=parser.parse_args()
    assert not(args.validate and args.output_dir),'Validation checks the published runtime assets'
    config=json.loads(SOURCE.read_text());scene=json.loads(SCENE.read_text());layout=json.loads(LAYOUT.read_text());pixels,proof=build(config,scene,layout)
    assert proof['gpu_bytes']==2049*2049*4 and proof['section_segment_count']>500 and all(n>10000 for n in proof['channel_nonzero'])
    assert not np.any(pixels[[0,-1]]) and not np.any(pixels[:,[0,-1]])
    if args.validate:
        actual=np.asarray(Image.open(OUTPUT).convert('RGBA'));assert np.array_equal(actual,pixels),'Published history mask is stale'
        assert json.loads(MANIFEST.read_text())==proof,'Published ground-history proof is stale'
    else:
        output=(args.output_dir/OUTPUT.name) if args.output_dir else OUTPUT
        manifest=(args.output_dir/MANIFEST.name) if args.output_dir else MANIFEST
        output.parent.mkdir(exist_ok=True,parents=True);Image.fromarray(pixels,'RGBA').save(output,optimize=True);manifest.write_text(json.dumps(proof,indent=2)+'\n')
    print(('VALID' if args.validate else 'STAGED' if args.output_dir else 'PUBLISHED'),'village ground history:',len(proof['grounded_sections']),'actual grounded sections,',proof['section_segment_count'],'segments;',round(proof['gpu_bytes']/1048576,3),'MiB RGBA8; channels',proof['channel_nonzero'],'hero changed pixels',proof['hero_market']['changed_pixels'])
if __name__=='__main__':main()
