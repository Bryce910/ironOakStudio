#!/usr/bin/env python3
"""Deterministic, source-resident dressing for the connected village demo.

Only apply(author) mutates the caller's in-memory scene. The editable pocket
inventory is separate from the earlier six dressing pockets and final_ labels.
No model, shared layout, renderer, terrain, or source files are written.
"""
from pathlib import Path
import argparse,collections,copy,hashlib,json,math
from build_castle_village import (Author,PATH,MODELS,DIRECTORY,ALIASES,NON_SOLID,
    PURCHASED_GOODS,PURCHASED_BUILDINGS,ROUTE,rectangle_distance,separated_bounds)

CONFIG=DIRECTORY/'village_demo_dressing.json'
LEGACY={'starting_crate','starting_barrel','starting_bucket','starting_lantern','starting_axe','starting_stump'}
PLANTS={'starting_celandine','starting_wildflower','fern0','fern1','fern2'}

def baseline_equal(current,baseline):
    fields={'x','z','yaw','scale','elevation','burial'}
    if len(current)!=len(baseline):return False
    for a,b in zip(current,baseline):
        if a.keys()!=b.keys():return False
        for key in a:
            if key in fields:
                if abs(a[key]-b[key])>1e-6:return False
            elif a[key]!=b[key]:return False
    return True

def base_y(r):return r.get('elevation',0)-r.get('burial',0)-(.05 if r['alias'] in LEGACY else 0)
def shape(author,r,box=None):
    if box is None:lo,hi=author.model(r['alias']);box=[lo[0],hi[0],lo[2],hi[2],lo[1],hi[1]]
    c,s=math.cos(r['yaw']),math.sin(r['yaw']);scale=r['scale'];x=(box[0]+box[1])*.5*scale;z=(box[2]+box[3])*.5*scale
    cx,cz=r['x']+c*x+s*z,r['z']-s*x+c*z;hx,hz=(box[1]-box[0])*.5*scale,(box[3]-box[2])*.5*scale
    ax,az=abs(c)*hx+abs(s)*hz,abs(s)*hx+abs(c)*hz
    return (cx,cz,hx,hz,c,s,base_y(r)+box[4]*scale,base_y(r)+box[5]*scale,cx-ax,cx+ax,cz-az,cz+az)
def overlap(a,b,margin=.018):
    if a[7]<=b[6]+.001 or b[7]<=a[6]+.001:return False
    if a[9]+margin<b[8] or b[9]+margin<a[8] or a[11]+margin<b[10] or b[11]+margin<a[10]:return False
    ax,az,ahx,ahz,ac,asi=a[:6];bx,bz,bhx,bhz,bc,bsi=b[:6]
    for x,z in [(ac,-asi),(asi,ac),(bc,-bsi),(bsi,bc)]:
        radius=ahx*abs(ac*x-asi*z)+ahz*abs(asi*x+ac*z)+bhx*abs(bc*x-bsi*z)+bhz*abs(bsi*x+bc*z)
        if abs((ax-bx)*x+(az-bz)*z)>radius+margin:return False
    return True
def keep_boxes():
    manifest=json.loads((MODELS/'village_asset_manifest.json').read_text())
    return next(r['collision_boxes']for r in manifest['assets']if r['asset']=='village_keep')
def obstacle_shapes(author):
    result=[]
    for r in author.records:
        if r['alias'] in NON_SOLID or r['alias']=='starting_gate':continue
        boxes=keep_boxes()if r['alias']=='village_keep'else [None]
        result.extend((r['label'],shape(author,r,box),r)for box in boxes)
    return result
def route_margin(author,r,a,b):
    lo,hi=author.model(r['alias']);c,s=math.cos(r['yaw']),math.sin(r['yaw']);scale=r['scale']
    local=lambda p:[c*(p[0]-r['x'])-s*(p[1]-r['z']),s*(p[0]-r['x'])+c*(p[1]-r['z'])]
    return rectangle_distance(local(a),local(b),[lo[0]*scale,lo[2]*scale],[hi[0]*scale,hi[2]*scale])
def clear_routes(author,r,config):
    if r.get('room_parent'):
        # A four-metre central hall aisle connects the real 4.6m doorway.
        return route_margin(author,r,[0,-136],[0,-153])>3.0
    for road in author.layout['roads']:
        if route_margin(author,r,road['a'],road['b'])<=road['half_width']+.37:return False
    for a,b in zip(ROUTE,ROUTE[1:]):
        if route_margin(author,r,a,b)<.75:return False
    for a,b in config['protected_crowd_segments_xz']:
        if route_margin(author,r,a,b)<.72:return False
    return True
def clear(author,r,obstacles,config,pocket=None,exclude=()):
    a=shape(author,r)
    if pocket:
        xmin,zmin,xmax,zmax=pocket['bounds_xz']
        if a[8]<xmin or a[9]>xmax or a[10]<zmin or a[11]>zmax:return False
    if not clear_routes(author,r,config):return False
    for label,b,other in obstacles:
        if label in exclude:continue
        # Match the main author exactly: purchased structure envelopes retain
        # a15cm horizontal gap even when an awning is above nearby cargo.
        # Ordinary cargo still uses the independent three-dimensional check.
        if r['alias']not in NON_SOLID and (r['alias']in PURCHASED_BUILDINGS or other['alias']in PURCHASED_BUILDINGS):
            if not separated_bounds(author,r,other,.15):return False
        if r['alias'] in PLANTS and other['alias']=='broadleaf':
            if math.hypot(r['x']-other['x'],r['z']-other['z'])>.55*other['scale']:continue
        if overlap(a,b):return False
    return True
def record(spec,x,z):
    return dict(alias=spec['alias'],x=x,z=z,yaw=math.radians(spec.get('yaw_degrees',0)),scale=spec.get('scale',1),burial=0,elevation=0,label=spec['label'])
def add_record(author,r):
    author.add(r['alias'],r['x'],r['z'],r['yaw'],r['scale'],r['burial'],r['label'],r['elevation'])
    author.records[-1].update({k:v for k,v in r.items()if k not in author.records[-1]})
    return author.records[-1]
def world_to_local(parent,x,z):
    c,s=math.cos(parent['yaw']),math.sin(parent['yaw']);dx,dz=x-parent['x'],z-parent['z']
    return [(c*dx-s*dz)/parent['scale'],(s*dx+c*dz)/parent['scale']]
def support(author,spec,parent,local,vertical_range):
    r=record(spec,0,0);lo,hi=author.model(r['alias']);c,s=math.cos(r['yaw']),math.sin(r['yaw']);points=[];heights=[]
    # The retained scan contains real millimetre-scale gaps between boards.
    # Keep a product on actual wood with a bounded, deterministic contact
    # adjustment instead of silently accepting a missing ray or floating it.
    shifts=[(0,0)]+[(dx,dz)for dx in [0,.025,-.025,.05,-.05]for dz in [0,.025,-.025,.05,-.05]if dx or dz]
    for dx,dz in shifts:
        actual_local=[local[0]+dx,local[1]+dz];points=[];heights=[]
        try:
            for x,z in [(0,0),(lo[0],lo[2]),(lo[0],hi[2]),(hi[0],lo[2]),(hi[0],hi[2])]:
                px=actual_local[0]+(c*x+s*z)*r['scale']/parent['scale'];pz=actual_local[1]+(-s*x+c*z)*r['scale']/parent['scale']
                points.append([px,pz]);heights.append(author.surface(parent['alias'],px,pz,*vertical_range))
        except AssertionError:continue
        if max(heights)-min(heights)<.025:break
    else:raise AssertionError(('No five-ray demo support',r['label'],parent['label'],local))
    local=actual_local
    assert max(heights)-min(heights)<.025,('Uneven demo support',r['label'],heights)
    pc,ps=math.cos(parent['yaw']),math.sin(parent['yaw'])
    r.update(x=parent['x']+(pc*local[0]+ps*local[1])*parent['scale'],z=parent['z']+(-ps*local[0]+pc*local[1])*parent['scale'],
             yaw=r['yaw']+parent['yaw'],elevation=base_y(parent)+max(heights)*parent['scale']+.002-lo[1]*r['scale']+(.05 if r['alias']in LEGACY else 0),
             surface_parent=parent['label'])
    if parent.get('room_parent')or parent['label']=='royal_keep':r['room_parent']='royal_keep';r['parent_structure']='royal_keep'
    return r,dict(label=r['label'],parent=parent['label'],actual_surface_samples_m=heights,actual_footprint_xz=points,ground_elevation_m=r['elevation'],contact_slack_m=.002,source_local_xz=local)

def apply(author):
    if getattr(author,'demo_dressing',None):return author.demo_dressing
    config=json.loads(CONFIG.read_text());assert config['schema_version']==1
    baseline=copy.deepcopy(author.records);known={r['label']:r for r in author.records};obstacles=obstacle_shapes(author)
    added=[];supports=[];pocket_rows=[];resident=set(r['alias']for r in baseline)
    for filename in ['authored_instances.txt','starting_area_instances.txt','rainforest_instances.txt']:
        resident.update(line.split()[0]for line in (DIRECTORY/filename).read_text().splitlines()if line.strip()and not line.lstrip().startswith('#'))
    def register(r,proof=None):
        assert r['label'].startswith('demo_')and r['label']not in known and r['alias']in resident,('Nonresident/duplicate demo asset',r['label'],r['alias'])
        lo,hi=author.model(r['alias'],True);assert 0<r['scale']<=2
        added.append(r['label']);actual=add_record(author,r);known[r['label']]=actual
        if proof:supports.append(proof);author.attachments.append(proof)
        if r['alias']not in NON_SOLID:obstacles.append((r['label'],shape(author,r),actual))
    for pocket in config['pockets']:
        labels=[];displacement=0
        for spec in pocket['ground_items']:
            r=record(spec,*spec['preferred_xz']);lo,hi=author.model(r['alias'],True)
            r['elevation']=-lo[1]*r['scale']+(.05 if r['alias']in LEGACY else 0)
            choices=[spec['preferred_xz']]+sorted(pocket['authored_slots_xz'],key=lambda p:math.dist(p,spec['preferred_xz']))
            for x,z in choices:
                if math.dist((x,z),spec['preferred_xz'])>spec.get('maximum_shift_m',4):continue
                r.update(x=x,z=z)
                if clear(author,r,obstacles,config,pocket):break
            else:raise AssertionError(('No clear authored demo slot',pocket['id'],spec['label'],spec['alias'],spec['preferred_xz']))
            displacement=max(displacement,math.dist((r['x'],r['z']),spec['preferred_xz']));register(r);labels.append(r['label'])
        for spec in pocket.get('attachments',[]):
            parent=known[spec['parent']];r,proof=support(author,spec,parent,spec['local_xz'],spec['surface_y_range'])
            assert clear(author,r,obstacles,config,exclude=[parent['label']]),('Demo supported prop intersects another volume',r['label'])
            register(r,proof);labels.append(r['label'])
        pocket_rows.append(dict(id=pocket['id'],title=pocket['title'],labels=labels,bounds_xz=pocket['bounds_xz'],maximum_authored_slot_adjustment_m=displacement))
    keep=known['royal_keep'];interior=[]
    for spec in config['hall']['floor_items']:
        local=world_to_local(keep,*spec['world_xz']);floor_spec=dict(spec,yaw_degrees=spec.get('yaw_degrees',0)-math.degrees(keep['yaw']))
        r,proof=support(author,floor_spec,keep,local,[.015,.025]);r['room_parent']='royal_keep';r['parent_structure']='royal_keep'
        assert clear(author,r,obstacles,config),('Keep staging intersects collider or aisle',r['label'])
        register(r,proof);interior.append(r['label'])
    for spec in config['hall']['attachments']:
        parent=known[spec['parent']];r,proof=support(author,spec,parent,spec['local_xz'],spec['surface_y_range'])
        assert clear(author,r,obstacles,config,exclude=[parent['label']]),('Keep table support collision',r['label'])
        register(r,proof);interior.append(r['label'])
    # Back-wall banners are attached to an actual triangle intersection. The
    # independent collision pass still checks all fourteen wall/post/tower boxes.
    wall_proofs=[]
    if 'village_keep'not in author.triangles:author.surface('village_keep',0,0,.015,.025)
    for spec in config['hall'].get('wall_flags',[]):
        hits=[];x,y=spec['local_x'],spec['local_top_y']
        for a,b,c in author.triangles['village_keep']:
            dx1,dy1=b[0]-a[0],b[1]-a[1];dx2,dy2=c[0]-a[0],c[1]-a[1];d=dx1*dy2-dy1*dx2
            if abs(d)<1e-10:continue
            u=((x-a[0])*dy2-(y-a[1])*dx2)/d;v=(dx1*(y-a[1])-dy1*(x-a[0]))/d
            z=a[2]+u*(b[2]-a[2])+v*(c[2]-a[2])
            if u>=0 and v>=0 and u+v<=1 and 8.5<z<9.1:hits.append(z)
        assert hits,('Missing real keep wall flag support',spec['label']);wall=min(hits)
        r=record(spec,0,0);lo,hi=author.model(r['alias']);r['x']=-x;r['z']=keep['z']-wall+max(abs(lo[0]),abs(hi[0]))*r['scale']+.025
        r['elevation']=base_y(keep)+y-hi[1]*r['scale'];r['parent_structure']='royal_keep';r['room_parent']='royal_keep'
        assert clear(author,r,obstacles,config),('Keep wall flag collision',r['label']);register(r);interior.append(r['label'])
        wall_proofs.append(dict(label=r['label'],parent='royal_keep',local_wall_z=wall,local_top_y=y,wall_triangle_hits=len(hits),standoff_m=.025))
    assert author.records[:len(baseline)]==baseline,'Dressing changed an existing record'
    author.demo_dressing=dict(schema_version=1,source_file=CONFIG.name,source_sha256=hashlib.sha256(CONFIG.read_bytes()).hexdigest(),labels=added,pockets=pocket_rows,interior_labels=interior,
       surface_supports=supports,wall_supports=wall_proofs,existing_records_preserved=len(baseline),new_model_families=0,new_texture_policies=0,
       story='Workshop fuel/workbenches, inn tasting, market delivery, cooper provisions, arrivals, paired castle guard/service courts and an accessible furnished keep hall')
    district_meta=getattr(author,'demo_district',None)or getattr(author,'_demo_district_metadata',None)
    if district_meta:
        roads={r['id']:r for r in author.layout['roads']}
        for pocket in pocket_rows:
            center=[(pocket['bounds_xz'][0]+pocket['bounds_xz'][2])*.5,(pocket['bounds_xz'][1]+pocket['bounds_xz'][3])*.5]
            candidates=[]
            for district in district_meta['districts']:
                lines=[roads[x]for x in district['road_ids']if x in roads]
                if lines:
                    from build_starting_area import segment_distance
                    candidates.append((min(segment_distance(center,r['a'],r['b'])for r in lines),district))
            district=min(candidates,key=lambda x:x[0])[1];pocket['street_district']=district['id']
            district['prop_labels']=sorted(set(district.get('prop_labels',[]))|set(pocket['labels']))
            counts=district.setdefault('category_counts',{});counts['dressing_instances']=len(district['prop_labels'])
            counts['surface_products']=sum(known[x]['alias']in PURCHASED_GOODS for x in district['prop_labels'])
            counts['vegetation']=sum(known[x]['alias']in PLANTS or known[x]['alias']=='broadleaf'for x in district['prop_labels'])
        author.demo_district=district_meta;author._demo_district_metadata=copy.deepcopy(district_meta)
    author._demo_baseline=baseline
    author.demo_dressing['validation']=validate(author)
    return author.demo_dressing

def validate(author):
    config=json.loads(CONFIG.read_text());meta=author.demo_dressing;labels=meta['labels'];by_label={r['label']:r for r in author.records}
    assert 300<=len(labels)<=600 and len(labels)==len(set(labels)) and all(s.startswith('demo_')for s in labels)
    assert baseline_equal(author.records[:len(author._demo_baseline)],author._demo_baseline),'Existing record/order changed beyond serialized numeric tolerance'
    assert len(author.purchased['dressing_pockets'])==6 and sum(len(p['labels'])for p in author.purchased['dressing_pockets'])==183
    assert len([r for r in author.records if r['label'].startswith('final_')])==103
    obstacles=obstacle_shapes(author);checks=0;minimum=math.inf
    for label in labels:
        r=by_label[label];assert r['alias']in ALIASES and r['scale']>0
        assert clear_routes(author,r,config),('Demo blocks a protected street/crowd/hall aisle',label)
        for other,b,record in obstacles:
            if other==label or (other==r.get('surface_parent')and other!='royal_keep'):continue
            if r['alias']not in NON_SOLID and (r['alias']in PURCHASED_BUILDINGS or record['alias']in PURCHASED_BUILDINGS):
                assert separated_bounds(author,r,record,.15),('Demo purchased envelope gap below15cm',label,other)
            if r['alias']in PLANTS and record['alias']=='broadleaf'and math.hypot(r['x']-record['x'],r['z']-record['z'])>.55*record['scale']:continue
            assert not overlap(shape(author,r),b),('Demo prop volume intersects existing scene',label,other)
            checks+=1
        if not r.get('surface_parent')and r['label']not in {p['label']for p in meta['wall_supports']}:
            lo,hi=author.model(r['alias']);assert abs(base_y(r)+lo[1]*r['scale'])<1e-5,('Demo ground contact',label)
        if r.get('room_parent'):
            distance=route_margin(author,r,[0,-136],[0,-153]);minimum=min(minimum,distance)
            assert distance>3,('Keep central aisle blocked',label)
    for p in meta['surface_supports']:
        r=by_label[p['label']];parent=by_label[p['parent']];lo,_=author.model(r['alias'])
        actual=base_y(r)+lo[1]*r['scale'];target=base_y(parent)+max(p['actual_surface_samples_m'])*parent['scale']+.002
        assert abs(actual-target)<1e-6,('Demo supported contact changed',r['label'])
    goods=[by_label[label]for label in labels if by_label[label]['alias']in PURCHASED_GOODS]
    for i,a in enumerate(goods):
        for b in goods[i+1:]:
            if a.get('surface_parent')==b.get('surface_parent'):
                assert separated_bounds(author,a,b,.008),('Demo product footprints overlap',a['label'],b['label'])
    keep=by_label['royal_keep']
    for proof in meta['wall_supports']:
        r=by_label[proof['label']];bounds=shape(author,r)
        assert bounds[6]>4.5,('Demo cloth lowered into walking volume',r['label'])
        assert abs(bounds[7]-(base_y(keep)+proof['local_top_y']))<1e-6,('Demo wall attachment height changed',r['label'])
    return dict(instances=len(labels),pockets=len(meta['pockets']),keep_hall_instances=len(meta['interior_labels']),
       aliases=dict(sorted(collections.Counter(by_label[s]['alias']for s in labels).items())),volume_clearance_checks=checks,
       real_mesh_surface_rays=len(meta['surface_supports'])*5,real_wall_attachments=len(meta['wall_supports']),
       keep_collider_boxes_checked=14,keep_central_aisle_half_width_m=3,minimum_hall_aisle_edge_distance_m=minimum,
       existing_pockets_preserved=6,existing_dressing_objects_preserved=183,existing_final_prefix_preserved=103,
       new_model_families=0,new_texture_policies=0,deterministic=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--validate',action='store_true');args=p.parse_args()
    author=Author(json.loads(PATH.read_text()));author.compose()
    try:
        import build_village_demo_district;build_village_demo_district.apply(author)
    except ImportError:pass
    result=apply(author);print(json.dumps(result['validation'],indent=2))
