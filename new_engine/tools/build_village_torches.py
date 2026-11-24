#!/usr/bin/env python3
"""Source-measured purchased torches, real bracket contacts and fixed emitters.

apply(author) appends only this layer. The CLI stages proof and an independent
VillageTorchEmitter include; the root builder decides when to publish it. All
sockets come from the retained purchased-asset manifest. No flame geometry,
source textures, renderer or existing scenery is changed here.
"""
from pathlib import Path
import argparse,copy,hashlib,json,math,sys,tempfile
import numpy as np
from build_castle_village import Author,PATH,ROOT,MODELS,ALIASES,ORIGIN,ROUTE
from build_starting_area import Atlas,glb_bounds
from build_village_demo_dressing import shape,overlap,base_y,route_margin,baseline_equal
from build_village_hero_grove import source_routes
from build_village_market_hero import triangle_box_intersects
MANIFEST=MODELS/'purchased_torches_manifest.json'
OUTPUT=ROOT/'docs/performance/village-torches'
ASSET_ALIASES={f'purchased_torch_{v}':f'purchased_torch_{v}_near'for v in ['pole','wall','brazier']}
ALIASES.update(ASSET_ALIASES)
HALL=[-12,-154,12,-135.6,23.8,31.2]
OUTDOOR=[-280,-280,280,280,0,1000]
# Measured side-wall pieces, not abstract rectangles replacing the source.
COURT_WALLS=[('west_training','castle_west_10',1),('west_armory','castle_west_8',1),
              ('east_monument','castle_east_10',-1),('east_armory','castle_east_8',-1)]
STREET_WALLS=[('hero_north_a','demo_home_market_row_north_1',-1),
 ('hero_north_b','demo_home_market_row_north_2',-1),
 ('hero_south_a','demo_home_market_row_south_0',-1),
 ('hero_south_b','demo_home_market_row_south_1',1),
 ('merchants_west','home_2_0',1),('merchants_east','lane_east_bakers',-1)]


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory():
    data=json.loads(MANIFEST.read_text());assert data['schema_version']==1
    assets={row['alias']:row for row in data['assets']if row['lod']=='near'}
    assert set(assets)=={'hand','cup','pole','wall','brazier'}
    for row in data['assets']:
        assert sha(MODELS/row['file'])==row['sha256'],'Purchased torch source changed'
    return data,assets


def register(author):
    owner=sys.modules[type(author).__module__];owner.ALIASES.update(ASSET_ALIASES)
    ALIASES.update(ASSET_ALIASES)


def triangles(author,alias):
    if alias not in author.triangles:author.triangles[alias]=glb_bounds(MODELS/(ALIASES[alias]+'.glb'),True)[3]
    return np.asarray(author.triangles[alias],float)


def ray_x(author,alias,y,z,sign,interior=False):
    t=triangles(author,alias);a=t[:,0];b=t[:,1]-a;c=t[:,2]-a
    denominator=b[:,1]*c[:,2]-b[:,2]*c[:,1];good=np.abs(denominator)>1e-10
    inverse=np.divide(1.,denominator,out=np.zeros_like(denominator),where=good)
    u=((y-a[:,1])*c[:,2]-(z-a[:,2])*c[:,1])*inverse
    v=(b[:,1]*(z-a[:,2])-b[:,2]*(y-a[:,1]))*inverse
    x=a[:,0]+u*b[:,0]+v*c[:,0]
    hits=x[good&(u>=-.0001)&(v>=-.0001)&(u+v<=1.0001)&(x*sign>0)]
    assert len(hits),'No actual source wall at bracket sample'
    return float((min if (sign>0)==interior else max)(hits))


def plate_samples():
    _,_,vertices,_=glb_bounds(MODELS/'purchased_torch_wall_near.glb',True)
    plate=[p for p in vertices if abs(p[2])<.003]
    assert len(plate)>=100
    # These are actual source backplate vertices, not invented anchor offsets.
    points=[]
    for y in [.014,.835,1.646,2.469]:
        for x in[-.12,0,.12]:
            p=min(plate,key=lambda p:(p[0]-x)**2+(p[1]-y)**2)
            if p not in points:points.append(p)
    return points


def mount(author,parent,label,sign,station=0,interior=False):
    scale=.60;c,s=math.cos(parent['yaw']),math.sin(parent['yaw']);points=plate_samples()
    # Keep the real fuel head at2.7–3.1m, looking for a flat portion of the
    # actual façade rather than silently spanning an open door/window.
    for elevation in[1.6,1.7,1.5,1.8]:
        for offset in[0,-.6,.6,-1.2,1.2,-1.8,1.8,-2.4,2.4]:
            localz=station+offset;surface=[];contacts=[]
            normal_sign=-sign if interior else sign
            try:
                for x,y,_ in points:
                    py=(elevation-base_y(parent)+y*scale)/parent['scale']
                    pz=localz-normal_sign*x*scale/parent['scale']
                    px=ray_x(author,parent['alias'],py,pz,sign,interior)
                    surface.append(px);contacts.append([px,py,pz])
            except AssertionError:continue
            # The signed normal points out of a house, into the receiving
            # hall, or into the courtyard from its curtain wall.
            normal_sign=-sign if interior else sign
            wallx=(min if normal_sign<0 else max)(surface)
            spread=(max(surface)-min(surface))*parent['scale']
            if spread>.028:continue
            xlocal=wallx+normal_sign*.002/parent['scale']
            x=parent['x']+(c*xlocal+s*localz)*parent['scale']
            z=parent['z']+(-s*xlocal+c*localz)*parent['scale']
            nx,nz=normal_sign*c,-normal_sign*s;yaw=math.atan2(nx,nz)
            record=dict(alias='purchased_torch_wall',x=x,z=z,yaw=yaw,scale=scale,burial=0,
                elevation=elevation,label='village_torch_'+label,parent_structure=parent['label'])
            proof=dict(label=record['label'],parent=parent['label'],interior=interior,
                source_side_sign=sign,source_local_z=localz,source_contacts=contacts,
                plate_vertices_local=points,source_contact_spread_m=spread,
                contact_slack_m=.002,normal_xz=[nx,nz],source_mount_x=wallx)
            if body_clear(author,record,parent['label']):return record,proof
    raise AssertionError(('No flat, clear real wall mount',label,parent['label']))


def body_clear(author,r,parent=None):
    a=shape(author,r)
    for other in author.records:
        if other['label']==parent:continue
        if overlap(a,shape(author,other),.012):return False
    return True


def emit(record,asset,normal=(0,0),zone='outdoor'):
    c,s=math.cos(record['yaw']),math.sin(record['yaw']);x,y,z=asset['flame_socket_local_m'];scale=record['scale']
    position=[record['x']+(c*x+s*z)*scale,24+record['elevation']-record['burial']+y*scale,
              record['z']+(-s*x+c*z)*scale]
    kind=asset['alias'];height,width={'wall':(.42,.13),'pole':(.55,.16),'brazier':(.65,.32)}[kind]
    seed=int(hashlib.sha256(record['label'].encode()).hexdigest()[:8],16)
    nx,nz=normal
    # Fade plane lies12cm behind the supporting wall so its front receives
    # the full bounded light. Ground lights use an always-positive plane.
    d=-nx*record['x']-nz*record['z']+.12 if nx or nz else 1
    return dict(label=record['label'],variant=kind,position_local_xyz=position,height=height,width=width,intensity=45,
        seed=seed,normal_xyz=[nx,0,nz],plane_d_local=d,clip_bounds=HALL if zone=='hall'else OUTDOOR,
        zone=zone,envelope_min_y=position[1],envelope_max_y=position[1]+2.4*height,
        envelope_horizontal_radius=1.8*width+.04)


def same_emitter(actual,expected):
    """Allow only the sub-micrometre error of the 12-digit scene text."""
    if isinstance(expected,dict):
        return isinstance(actual,dict) and actual.keys()==expected.keys() and all(same_emitter(actual[k],v)for k,v in expected.items())
    if isinstance(expected,(list,tuple)):
        return isinstance(actual,(list,tuple)) and len(actual)==len(expected) and all(same_emitter(a,b)for a,b in zip(actual,expected))
    if isinstance(expected,float) or isinstance(actual,float):
        return isinstance(actual,(int,float)) and isinstance(expected,(int,float)) and math.isclose(actual,expected,rel_tol=0,abs_tol=1e-6)
    return actual==expected


def include(metadata):
    lines=['/* Generated purchased torch fuel-base emitters. XZ village-local, Y absolute. */',
           '/* VillageTorchEmitter is defined by the consumer; fixed17-field POD. */',
           'static constexpr VillageTorchEmitter village_torch_emitters[]={']
    for e in metadata['emitters']:
        values=[*e['position_local_xyz'],e['height'],e['width'],e['intensity'],e['seed'],*e['normal_xyz'],e['plane_d_local'],*e['clip_bounds']]
        def value(v,i):
            if i==6:return str(v)+'u'
            number=format(v,'.12g')
            if i>=3:
                if '.'not in number and 'e'not in number:number+='.0'
                number+='f'
            return number
        lines.append('    {'+','.join(value(v,i)for i,v in enumerate(values))+'}, // '+e['label'])
    return '\n'.join(lines+['};',''])


def apply(author):
    if hasattr(author,'_village_torches_metadata'):return copy.deepcopy(author._village_torches_metadata)
    register(author);data,assets=inventory();before=copy.deepcopy(author.records);records={r['label']:r for r in before}
    assert not any(r['label'].startswith('village_torch_')for r in before)
    mounts=[];emitters=[];ground=[]
    def add(record,proof=None,zone='outdoor'):
        author.add(record['alias'],record['x'],record['z'],record['yaw'],record['scale'],record['burial'],record['label'],record['elevation'])
        if 'parent_structure'in record:author.records[-1]['parent_structure']=record['parent_structure']
        normal=proof['normal_xz']if proof else(0,0)
        emitters.append(emit(record,assets[record['alias'].removeprefix('purchased_torch_')],normal,zone))
        if proof:mounts.append(proof)
        else:ground.append(record['label'])
    # The bridge bowl/rails stay inside±7.5m; all these feet are beyond±8.7.
    for label,x,z in[('outer_west',-9,224),('outer_east',9,224),('gate_west',-9,198),('gate_east',9,198),
                      ('market_west',-30,35),('market_east',30,35)]:
        record=dict(alias='purchased_torch_pole',x=x,z=z,yaw=0,scale=.65,burial=.003,elevation=0,label='village_torch_'+label)
        assert body_clear(author,record),('Ground torch source overlap',label)
        add(record)
    for label,x,z in[('court_brazier_west',-58,-91),('court_brazier_east',52.3,-88)]:
        record=dict(alias='purchased_torch_brazier',x=x,z=z,yaw=0,scale=.70,burial=.003,elevation=0,label='village_torch_'+label)
        assert body_clear(author,record),('Brazier source overlap',label)
        add(record)
    for label,parent,sign in COURT_WALLS+STREET_WALLS:
        record,proof=mount(author,records[parent],label,sign)
        add(record,proof)
    keep=records['royal_keep']
    for side in[-1,1]:
        for station in[-6.5,6.5]:
            record,proof=mount(author,keep,f'hall_{"west"if side<0 else"east"}_{"north"if station<0 else"south"}',side,station,True)
            add(record,proof,'hall')
    labels=[r['label']for r in author.records[len(before):]]
    metadata=dict(schema_version=1,title='Purchased medieval torch lighting layer',recipe_file=str(Path(__file__).relative_to(ROOT)),
        recipe_sha256=sha(Path(__file__)),labels=labels,existing_records_preserved=len(before),
        baseline_records_sha256=hashlib.sha256(json.dumps(before,sort_keys=True,separators=(',',':')).encode()).hexdigest(),
        asset_manifest=str(MANIFEST.relative_to(ROOT)),asset_manifest_sha256=sha(MANIFEST),provenance_file=data['provenance'],
        provenance_sha256=sha(ROOT/data['provenance']),aliases=ASSET_ALIASES,
        model_sha256={row['file']:row['sha256']for row in data['assets']if row['alias']in['pole','wall','brazier']},
        wall_mounts=mounts,ground_labels=ground,emitters=emitters,maximum_near_lights=8,
        generated_include='engine/world_village_torches.inc',flame_envelope='Ybase..base+2.4*height; horizontal radius1.8*width+.04; source-fixed seed',
        lighting_policy='Fixed source sockets, bounded smooth flicker, wall plane and hall AABB clipping; no point-light cubemap passes')
    author._village_torches_baseline=before;author._village_torches_metadata=metadata
    return copy.deepcopy(metadata)


def flame_clearance(author,emitter):
    x,y,z=emitter['position_local_xyz'];radius=emitter['envelope_horizontal_radius'];low=[x-radius,y-24,z-radius];high=[x+radius,emitter['envelope_max_y']-24,z+radius]
    checks=0
    for other in author.records:
        if other['label']==emitter['label']:continue
        box=shape(author,other)
        if box[9]<low[0]or box[8]>high[0]or box[11]<low[2]or box[10]>high[2]or box[7]<low[1]or box[6]>high[1]:continue
        t=triangles(author,other['alias']);c,s=math.cos(other['yaw']),math.sin(other['yaw']);scale=other['scale'];w=t.copy()
        w[:,:,0]=other['x']+(c*t[:,:,0]+s*t[:,:,2])*scale
        w[:,:,1]=base_y(other)+t[:,:,1]*scale
        w[:,:,2]=other['z']+(-s*t[:,:,0]+c*t[:,:,2])*scale
        select=np.all(w.max(axis=1)>=low,axis=1)&np.all(w.min(axis=1)<=high,axis=1)
        for tri in w[select]:
            checks+=1
            assert not triangle_box_intersects(tri,low,high),('Flame/spark envelope intersects actual source',emitter['label'],other['label'])
    return checks


def validate(author):
    metadata=author._village_torches_metadata;before=author._village_torches_baseline
    assert baseline_equal(author.records[:len(before)],before),'Existing castle changed'
    assert len(metadata['labels'])==22 and len(set(metadata['labels']))==22
    assert len(metadata['ground_labels'])==8 and len(metadata['wall_mounts'])==14
    data,assets=inventory();known={r['label']:r for r in author.records}
    assert len(known)==len(author.records)
    proof_by_label={p['label']:p for p in metadata['wall_mounts']}
    parent_by_label={label:p['parent']for label,p in proof_by_label.items()}
    routes=[*zip(ROUTE,ROUTE[1:]),*zip(author.layout['demo_route_xz'],author.layout['demo_route_xz'][1:])]
    from build_castle_expansion import WALK
    routes+=list(zip(WALK,WALK[1:]))
    people=source_routes('world_village_population.cpp');pets=source_routes('world_village_pets.cpp')
    source_checks=0;flame_checks=0;route_checks=0
    for e in metadata['emitters']:
        record=known[e['label']];parent=parent_by_label.get(e['label'])
        original=author.records;author.records=[r for r in original if r['label']!=e['label']]
        try:assert body_clear(author,record,parent),('Torch body source collision',e['label'])
        finally:author.records=original
        assert record['alias']=='purchased_torch_'+e['variant']
        proof=proof_by_label.get(e['label'])
        normal=proof['normal_xz']if proof else(0,0)
        zone='hall'if proof and proof['parent']=='royal_keep'else'outdoor'
        calculated=emit(record,assets[e['variant']],normal,zone)
        assert same_emitter(e,calculated),('Emitter no longer matches measured source fuel',e['label'])
        assert 0<e['height']<=1.2 and .04<=e['width']<=.65 and e['intensity']==45
        for a,b in routes:
            # Brackets at head height retain actual bounds-based capsule
            # separation; high bowls do not excuse low protruding hardware.
            if shape(author,record)[6]<1.85:assert route_margin(author,record,a,b)>.52,(e['label'],'Public route capsule')
            route_checks+=1
        radius=e['envelope_horizontal_radius']
        if e['position_local_xyz'][1]<24+2.55:
            for a,b in people+pets:
                from build_starting_area import segment_distance
                assert segment_distance([e['position_local_xyz'][0],e['position_local_xyz'][2]],a,b)>radius+1.5,('Low flame near a living route',e['label'])
            from build_village_market_lively import TRADERS,OLD_STATIONS,PET_STATIONS
            fixed=TRADERS+OLD_STATIONS+[p for _,p in PET_STATIONS]+[[side*x,-z-j*5]for side in[-1,1]for x,z in[(35,72),(30,64)]for j in range(4)]
            for point in fixed:
                assert math.dist([e['position_local_xyz'][0],e['position_local_xyz'][2]],point)>radius+1.5,('Low flame near stationary head',e['label'])
            from build_castle_expansion import TRAINING_STATIONS
            for station in TRAINING_STATIONS:
                assert math.dist([e['position_local_xyz'][0],e['position_local_xyz'][2]],station['position_xz'])>radius+station['reserved_radius_m']+1.,('Low flame near training sweep',e['label'])
        flame_checks+=flame_clearance(author,e);source_checks+=len(author.records)-1-(1 if parent else 0)
    for proof in metadata['wall_mounts']:
        parent=known[proof['parent']];record=known[proof['label']]
        pc,ps=math.cos(parent['yaw']),math.sin(parent['yaw'])
        tc,ts=math.cos(record['yaw']),math.sin(record['yaw'])
        sign=-proof['source_side_sign']if proof['interior']else proof['source_side_sign']
        nx,nz=sign*pc,-sign*ps
        assert math.hypot(nx-proof['normal_xz'][0],nz-proof['normal_xz'][1])<1e-8
        assert math.hypot(ts-nx,tc-nz)<1e-8 and record['scale']==.6
        for point,plate in zip(proof['source_contacts'],proof['plate_vertices_local']):
            actual=ray_x(author,parent['alias'],point[1],point[2],proof['source_side_sign'],proof['interior'])
            assert abs(actual-point[0])<1e-6 and proof['source_contact_spread_m']<=.028
            support=np.asarray([parent['x']+(pc*point[0]+ps*point[2])*parent['scale'],
                base_y(parent)+point[1]*parent['scale'],parent['z']+(-ps*point[0]+pc*point[2])*parent['scale']])
            bracket=np.asarray([record['x']+(tc*plate[0]+ts*plate[2])*record['scale'],
                base_y(record)+plate[1]*record['scale'],record['z']+(-ts*plate[0]+tc*plate[2])*record['scale']])
            delta=bracket-support;gap=delta[0]*nx+delta[2]*nz
            assert .0019<=gap<=.0301 and np.linalg.norm(delta-np.asarray([nx,0,nz])*gap)<1e-6, ('Backplate detached from measured source contact',record['label'])
        assert record['parent_structure']==parent['label']
    return dict(instances=22,wall_torches=14,pole_torches=6,braziers=2,maximum_near_lights=8,
        existing_records_preserved=len(before),source_volume_checks=source_checks,actual_wall_contact_samples=sum(len(p['source_contacts'])for p in metadata['wall_mounts']),
        actual_flame_source_triangle_checks=flame_checks,route_capsule_checks=route_checks,all_emitters_source_socket_exact=True,
        flame_envelope_includes_sparks=True,wall_clip_planes=True,hall_clip_aabb=HALL,source_manifest_sha256=sha(MANIFEST))


def test(author):
    result=validate(author);before=copy.deepcopy(author.records);m=apply(author);assert apply(author)==m and before==author.records
    bad=copy.copy(author);bad._village_torches_metadata=copy.deepcopy(m);bad._village_torches_metadata['emitters'][0]['position_local_xyz'][1]+=.1
    try:validate(bad)
    except AssertionError:pass
    else:raise AssertionError('Detached flame socket accepted')
    rays=0
    with tempfile.TemporaryDirectory(prefix='valwood-torch-support-')as tmp:
        atlas=Atlas(tmp)
        for label in m['ground_labels']:
            r=next(r for r in author.records if r['label']==label);s=shape(author,r)
            for x,z in[(r['x'],r['z']),(s[8],s[10]),(s[8],s[11]),(s[9],s[10]),(s[9],s[11])]:
                assert abs(atlas.sample(ORIGIN[0]+x,ORIGIN[1]+z).height-24)<.01,(label,'Torch footing not on actual dry ground');rays+=1
    result.update(idempotent=True,detached_flame_negative_control=True,actual_ground_support_rays=rays)
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--stage',action='store_true');p.add_argument('--test',action='store_true');args=p.parse_args();assert args.stage or args.test
    author=Author(json.loads(PATH.read_text()));author.compose();metadata=apply(author);proof=test(author)if args.test else validate(author)
    if args.stage:
        OUTPUT.mkdir(parents=True,exist_ok=True)
        (OUTPUT/'placements-stage.json').write_text(json.dumps(dict(metadata=metadata,validation=proof,placements=[r for r in author.records if r['label']in metadata['labels']]),indent=2)+'\n')
        (OUTPUT/'world_village_torches.inc').write_text(include(metadata))
    print(json.dumps(proof,indent=2))
if __name__=='__main__':main()
