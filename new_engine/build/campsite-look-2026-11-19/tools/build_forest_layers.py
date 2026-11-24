#!/usr/bin/env python3
"""Stage owned forest-floor layers, never modifying LIVE or world.studio.

Actual rendered terrain support is sampled through the independently compiled
production sampler.  Ground-paint ideas are proposals for the root-owned merge.
"""
from pathlib import Path
import argparse, collections, ctypes, hashlib, importlib.util, json, math, re, shlex, struct
import numpy as np

N = Path(__file__).resolve().parents[1]
ROOT = N.parents[1]
P = N / 'world-project'
OUT = N / 'review/forest-layout'
W, D = 100800., 100800. * 2818. / 5009.
CAMP = np.array([-11988., 5172.])
CREEK = np.array([[-11990.,5140.],[-12010.,5148.],[-12018.,5162.],
                  [-12018.,5180.],[-12022.,5193.],[-12028.,5202.]])
BUILTINS = {'fern0':('woodland_fern_0',0), 'broadleaf':('cinematic_broadleaf_near',3),
            'pine_young':('cinematic_pine_young_near',3), 'deadwood':('cinematic_deadwood',1),
 'pine':('cinematic_pine_near',3),'fir':('cinematic_fir_near',3),
 'fern1':('woodland_fern_1',0),'fern2':('woodland_fern_2',0),
 'rock0':('cinematic_rock_0',1),'rock1':('cinematic_rock_1',1),'rock2':('cinematic_rock_2',1),
 'showcase_roots':('showcase_roots',1),'showcase_stump':('showcase_stump',1),
 'starting_stump':('starting_stump',2),'showcase_shrub':('showcase_shrub',0),
 'showcase_shrub_b':('showcase_shrub_b',0),'showcase_sorrel':('showcase_sorrel',0)}
TREES = {'nm_city_region_oak','nm_city_region_beech','pine','fir','broadleaf','pine_young'}

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def distance(point, a, b):
    v=b-a
    t=np.clip(float(np.dot(point-a,v))/max(float(np.dot(v,v)),1e-12),0,1)
    return float(np.linalg.norm(point-a-t*v))

def decode(q):
    return dict(x=(float(q[1])-.5)*W,z=(float(q[2])-.5)*D,y=float(q[13]),
                yaw_deg=float(q[3]),scale=float(q[4]),pitch_deg=float(q[8]),
                roll_deg=float(q[9]),sx=float(q[10]),sy=float(q[11]),sz=float(q[12]))

def native_scale(p):
    # Runtime requires the authoring base scale>=.15 independently of the
    # effective axis lengths. Transfer its uniform factor without changing shape.
    if p['scale']<.15:
        ratio=p['scale']/.15
        for axis in ['sx','sy','sz']:p[axis]*=ratio
        p['scale']=.15

def validate_native_rows(report):
    """Validate the complete source grammar/budgets, not a looser art schema."""
    counts=collections.Counter();total=0;maximum_base=0;maximum_tail=0
    source=(N/'source-v1/engine/world_scenery.cpp').read_text()
    native_aliases={m[0]:m[2]=='true'for m in re.findall(r'\{"([^"]+)",\s*"([^"]+)",\s*(true|false),',source)}
    for line in (P/'world.studio').read_text().splitlines():
        q=shlex.split(line)
        if q and q[0]=='asset':native_aliases[q[1]]=q[5]=='2'
    for file in sorted((P/'scenery').glob('*.txt')):
        assert file.stat().st_size<=4*1024*1024
        for line_no,line in enumerate(file.read_text().splitlines(),1):
            raw=line.split('#',1)[0];q=raw.split()
            assert len(line)<=320,(file,line_no,'320byte record')
            if not q:
                assert len(line)<=256
                continue
            assert len(q)>=6 and q[0]in native_aliases,(file,line_no,'known alias')
            suffix=q[6:q.index('wind')]if 'wind'in q else q[6:]
            assert len(suffix)in [0,1,2,7,8],(file,line_no,'numeric suffix')
            if 'wind'in q:
                at=raw.index('wind');base=at-1;tail=len(line)-base
                assert base<=256 and tail<=64 and len(q)==q.index('wind')+3
                assert all(math.isfinite(float(v))and 0<=float(v)<=2 for v in q[-2:])
            else:base=len(line);tail=0;assert base<=256
            maximum_base=max(maximum_base,base);maximum_tail=max(maximum_tail,tail)
            numeric=[float(v)for v in q[1:6]+suffix];assert all(math.isfinite(v)for v in numeric)
            u,v,yaw,scale,burial=numeric[:5]
            assert 0<=u<=1 and 0<=v<=1 and -360<=yaw<=360 and .15<=scale<=4 and burial>=0,(file,line_no,'base limits')
            assert scale<=(2.5 if q[0]=='broadleaf'else 2 if native_aliases[q[0]]else 4),(file,line_no,'alias scale cap')
            if len(suffix)>=1:assert float(suffix[0])>=0
            if len(suffix)>=2:assert -128<=float(suffix[1])<=128
            if len(suffix)>=7:
                pitch,roll,sx,sy,sz=map(float,suffix[2:7])
                assert abs(pitch)<=360 and abs(roll)<=360 and all(.1<=x<=8 for x in [sx,sy,sz]),(file,line_no,'full transform')
            if len(suffix)==8:assert -2000<=float(suffix[7])<=12000
            counts[q[0]]+=1;total+=1
    assert total<=16384 and max(counts.values())<=4096
    placements=report['placements'];lines=(P/'scenery/authored_instances.txt').read_text().splitlines()
    assert all(lines[p['line']-1]==p['row']for p in placements)
    for item in placements:
        q=item['row'].split();assert float(q[5])==0 and float(q[6])==0,'New burial/elevation must be zero (within every native alias cap)'
        assert float(q[13])>.1,'New explicit pivot must be above native minimum finite support'
        assert len(q[:q.index('wind')]if 'wind'in q else q)==14,'New placement requires exact8-number transform suffix'
    return dict(passed=True,total_records=total,added_records_checked=len(placements),max_alias_records=max(counts.values()),
        record_limit=16384,alias_limit=4096,maximum_legacy_bytes=maximum_base,maximum_wind_tail_bytes=maximum_tail,
        numeric_suffix_allowed=[0,1,2,7,8],source_sha256=sha(N/'source-v1/engine/world_scenery.cpp'),
        transform_source_sha256=sha(N/'source-v1/engine/world_transform_math.h'),
        limitation='Static authoring grammar/range audit; terrain/water and actual native load are reviewed separately.')

def fix_native_scale():
    report=json.loads((OUT/'ROW_PATCH.json').read_text());target=P/'scenery/authored_instances.txt';lines=target.read_text().splitlines();changes=[]
    for item in report['placements']:
        i=item['line']-1;assert lines[i]==item['row'];q=lines[i].split();old=float(q[4])
        if old>=.15:continue
        before=lines[i];oldaxes=np.array([float(q[k])for k in [10,11,12]])
        newaxes=oldaxes*old/.15;q[4]='0.15000000'
        for k,v in zip([10,11,12],newaxes):q[k]=f'{v:.12f}'
        after=' '.join(q);actual=np.array([float(q[k])for k in [10,11,12]])*.15
        diff=float(np.abs(actual-oldaxes*old).max());assert diff<1e-12
        floatdiff=float(np.abs(np.float32(actual)-np.float32(oldaxes)*np.float32(old)).max())
        lines[i]=after;item['row']=after
        changes.append(dict(line=i+1,alias=item['alias'],before=before,after=after,max_effective_axis_difference=diff,max_float32_axis_difference=floatdiff))
    assert changes
    target.write_text('\n'.join(lines)+'\n');report['native_base_scale_reparameterization']=dict(count=len(changes),changes=changes,
        purpose='Meet native base-scale .15 lower bound while preserving effective XYZ scale/world geometry.',
        max_effective_axis_difference=max(c['max_effective_axis_difference']for c in changes),
        max_float32_axis_difference=max(c['max_float32_axis_difference']for c in changes))
    report['native_authoring_validation']=validate_native_rows(report);report['output_sha256']=sha(target);report['tool_sha256']=sha(Path(__file__))
    report['changed_scenery_files']['scenery/authored_instances.txt']['after_sha256']=sha(target)
    report['final_surface_regrounded']=False
    (OUT/'ROW_PATCH.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(normalized_rows=len(changes),max_axis_difference=report['native_base_scale_reparameterization']['max_effective_axis_difference'],validation=report['native_authoring_validation'])))

def sampler():
    bank=ROOT/'build/camp-road-polish-2026-11-19'
    for name in ['world_surface_authoring.cpp','world_surface_field.cpp']:
        assert (bank/'source-v1/engine'/name).read_bytes()==(N/'source-v1/engine'/name).read_bytes()
    path=bank/'review/route-budget/sample-height.so'
    lib=ctypes.CDLL(str(path));lib.load.argtypes=[ctypes.c_char_p,ctypes.c_char_p];lib.load.restype=ctypes.c_int
    lib.sample_many.argtypes=[ctypes.c_uint,ctypes.POINTER(ctypes.c_double),ctypes.POINTER(ctypes.c_float),ctypes.c_int]
    lib.sample_many.restype=ctypes.c_int;lib.error.restype=ctypes.c_char_p
    loaded_studio_sha=sha(P/'world.studio')
    assert lib.load(str(P/'ground/showcase.height').encode(),str(P/'world.studio').encode())==1,lib.error()
    assert loaded_studio_sha==sha(P/'world.studio'),'Studio changed while loading actual surface'
    def ground(x,z):
        points=np.ascontiguousarray([[x,z]],dtype=np.float64);values=np.empty(1,dtype=np.float32)
        if lib.sample_many(1,points.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),values.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),1)!=1:return None
        return float(values[0])
    ground.loaded_studio_sha=loaded_studio_sha
    return ground,path

def paint_proposal():
    """Recompose twelve existing local masks; preserve every distant paint."""
    report=json.loads((OUT/'ROW_PATCH.json').read_text())
    old=json.loads((ROOT/'build/camp-road-polish-2026-11-19/review/scenery/ROW_PATCH.json').read_text())
    studio=(P/'world.studio').read_text().splitlines()
    donors=[]
    for item in old['changed_studio_rows']:
        if item.get('kind')!='shade_litter_composition':continue
        before=item['after']
        assert studio.count(before)==1,'Original local donor unexpectedly changed'
        q=json.loads(before[9:]).split();point=np.array([float(q[1]),float(q[2])])
        donors.append((float(np.linalg.norm(point-CAMP)),before,point))
    donors=[d for d in sorted(donors,key=lambda t:t[0])if d[0]<110]
    assert len(donors)==10
    for record in ['paint -11996.6 5178.6 4.8 0.94 7 0','paint -11987.8 5192 5.2 0.94 7 0']:
        before='surfaces '+json.dumps(record);assert studio.count(before)==1
        q=record.split();point=np.array([float(q[1]),float(q[2])]);donors.append((float(np.linalg.norm(point-CAMP)),before,point))
    anchors=[[-11977,5161],[-11974,5172],[-11978,5185],[-11983,5193],
             [-11964,5158],[-11958,5177],[-11974,5206],[-11970,5225],
             [-11954,5238],[-11962,5252],[-11930,5257],[-11937,5265]]
    candidates=[c for c in report['clusters']if c['accepted']>=12 and
                np.linalg.norm(np.asarray(c['center_xz'])-CAMP)<120]
    selected=[];ground,helper=sampler();changes=[]
    for i,(anchor,donor)in enumerate(zip(anchors,donors)):
        available=[c for c in candidates if c['name']not in {s['name']for s in selected} and
                   all(np.linalg.norm(np.asarray(c['center_xz'])-np.asarray(s['center_xz']))>4 for s in selected)]
        c=min(available,key=lambda c:np.linalg.norm(np.asarray(c['center_xz'])-anchor));selected.append(c)
        x,z=c['center_xz'];radius=4.8+.35*(i%4);amount=.84 if i%3 else .72;layer=1 if i%4 else 7
        after='surfaces '+json.dumps(f'paint {x:.7f} {z:.7f} {radius:.2f} {amount:.2f} {layer} 0')
        changes.append(dict(before=donor[1],after=after,source_donor_distance_from_camp_m=donor[0],
            cluster=c['name'],center_xyz=[x,ground(x,z),z],purpose='world-tiled PBR beneath rooted fern/grass masses'))
    broad='surfaces '+json.dumps('paint -11977.789967299999 5215.8378071699999 48 0.78 7 0')
    assert studio.count(broad)==1
    changes.append(dict(before=broad,after='surfaces '+json.dumps('paint -11977.789967299999 5215.8378071699999 48 0.45 7 0'),purpose='Reduce blanket brown circle while retaining soft woodland base'))
    layer_before='surfaces '+json.dumps('layer "Arrival detailed woodland litter" 2 3 4 0.76 0.65 0.47 2.4 0.98 0 1 0')
    assert studio.count(layer_before)==1
    layer_after='surfaces '+json.dumps('layer "Arrival detailed woodland litter" 2 3 4 0.65 0.62 0.51 2.4 0.98 0 1 0')
    result=dict(schema='valwood.local-forest-ground-patch.v1',world_studio_modified=False,
        tool_sha256=sha(Path(__file__)),world_studio_snapshot_sha256=sha(P/'world.studio'),
        required_changes=changes,optional_tint_change=dict(before=layer_before,after=layer_after,
            rationale='Mute orange bias; red factor decreases14.5%, blue increases8.5%; retain existing exact images/roughness/normal/UV scale.'),
        changed_existing_local_paints=12,broad_base_amount_change=[.78,.45],paint_count_after=256,
        added_paints=0,added_projections=0,farther_priorpass_masks_preserved=11,priorpass_local_donors=10,additional_existing_camp_donors=2,
        ground_sampler_sha256=sha(helper),
        rendering_reason='Paint retains metre-based world UVs. Current projections use normalized UVs and would magnify the existing2m PBR scan over large boxes; nearest8 are uploaded, not64.',
        shader_and_source_modified=False)
    (OUT/'GROUND_PAINT_PROPOSAL.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(changed_local=12,furthest_donor_m=max(t[0]for t in donors),proposed_records=len(changes),world_studio_modified=False)))

def shoreline_stage():
    """Dress the *dry* shoulders after the root-owned creek/paint merge.

    This is the only plant class allowed inside the earlier conservative15m
    clearing. No solid, rock, shrub or tree is added to the bank corridor.
    """
    assert N.name=='scenery-forest-2026-11-19' and not (N/'PUBLISHED.json').exists()
    target=P/'scenery/authored_instances.txt';before=target.read_bytes()
    report=json.loads((OUT/'ROW_PATCH.json').read_text())
    assert not report.get('shoreline_instances'),'Never stack shoreline dressing'
    mesh_path=ROOT/'docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py'
    spec=importlib.util.spec_from_file_location('actual_mesh',mesh_path);mesh=importlib.util.module_from_spec(spec);spec.loader.exec_module(mesh)
    ground,helper=sampler();studio=(P/'world.studio').read_text().splitlines()
    aliases={q[1]:q for line in studio if (q:=shlex.split(line)) and q[0]=='asset'}
    ribbon=[shlex.split(json.loads(line[9:]))for line in studio if line.startswith('surfaces ') and json.loads(line[9:]).startswith('ribbon "West woodland creek"')]
    assert len(ribbon)==1;r=ribbon[0];assert r[:2]==['ribbon','West woodland creek']
    assert r[2]=='1' and float(r[4])==10 and float(r[5])==7 and r[7]=='1'
    controls=np.array([float(v)for v in r[9:]]).reshape(-1,3);assert len(controls)==int(r[8])
    assert np.max(np.abs(controls[:6][:,[0,2]]-CREEK))<1e-8,'Unexpected creek head layout'
    # Exact2m Catmull-Rom sampling formula from production bake_ribbon. Ground
    # itself is always evaluated by the compiled production sampler above.
    samples=[controls[0]];head_end=0
    for i in range(len(controls)-1):
        a,b,c,d=controls[max(0,i-1)],controls[i],controls[i+1],controls[min(i+2,len(controls)-1)]
        count=max(1,math.ceil(np.linalg.norm((c-b)[[0,2]])/2))
        for k in range(1,count+1):
            t=k/count;q=.5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)
            if np.linalg.norm((q-samples[-1])[[0,2]])>=.01:samples.append(q)
        if i==4:head_end=len(samples)-1
    samples=np.array(samples)
    def nearest(point):
        a=samples[:-1];v=samples[1:]-a;dxz=v[:,[0,2]]
        t=np.clip(np.sum((point-a[:,[0,2]])*dxz,axis=1)/np.sum(dxz*dxz,axis=1),0,1)
        q=a+v*t[:,None];d=np.linalg.norm(q[:,[0,2]]-point,axis=1);i=int(np.argmin(d))
        return float(d[i]),float(q[i,1])
    routes=json.loads((P/'opening/first-journey.routes.json').read_text())['paths'];segments=[]
    for points in routes.values():
        a=np.array(points)[:,[0,2]];segments.extend(zip(a[:-1],a[1:]))
    traffic=[]
    for line in (P/'opening/road-traffic.settings').read_text().splitlines():
        q=line.split()
        if q and q[0]=='point':traffic.append(np.array([float(q[1]),float(q[3])]))
    segments+=list(zip(traffic[:-1],traffic[1:]))
    actors=np.array([[-11990.15,5171.35],[-11985.8,5171.35],[-11987.9,5168.8]])
    protected=[(CAMP,4),(np.array([-11988.,5180.]),2.7),(np.array([-11933.,5312.]),8.5),
        (np.array([-11930.5,5320.]),4.6),(np.array([-11968.,5582.]),5.5)]
    positions=[]
    for file in (P/'scenery').glob('*.txt'):
        for line in file.read_text().splitlines():
            q=line.split()
            if len(q)<14 or q[0].startswith('#'):continue
            try:p=decode(q);positions.append(np.array([p['x'],p['z']]))
            except ValueError:pass
    points=np.array(positions);rng=np.random.default_rng(2026111911);new=[];assets={};skips=collections.Counter()
    species=['opening_verge_grass_long','opening_verge_grass_broad','bep_nature_clover',
        'opening_verge_grass_long','fern0','bep_nature_grass_long','opening_verge_grass_broad',
        'bep_nature_clover','bep_nature_grass_broad','opening_verge_grass_long']
    for attempt in range(6000):
        if len(new)>=280:break
        # Short irregular masses separated by visible bank gaps, both shores.
        pocket=attempt%20;si=min(head_end-2,2+(pocket//2)*max(1,(head_end-5)//9));side=-1 if pocket%2 else 1
        tangent=(samples[min(si+1,head_end)]-samples[max(0,si-1)])[[0,2]];tangent/=np.linalg.norm(tangent)
        right=np.array([tangent[1],-tangent[0]])
        point=samples[si,[0,2]]+tangent*rng.uniform(-2.8,2.8)+right*side*rng.uniform(7.3,11.6)
        if any(np.linalg.norm(point-c)<radius for c,radius in protected)or np.linalg.norm(actors-point,axis=1).min()<3.5:skips['protected']+=1;continue
        if len(points)and np.linalg.norm(points-point,axis=1).min()<.32:skips['existing_spacing']+=1;continue
        if new and min(np.linalg.norm(point-n['point'])for n in new)<.36:skips['new_spacing']+=1;continue
        alias=species[len(new)%len(species)]
        if alias not in assets:
            stem=BUILTINS[alias][0]if alias in BUILTINS else aliases[alias][2]+('_near'if aliases[alias][3]=='2'else'')
            path=P/'models'/(stem+'.glb')
            if not path.exists():path=P/'models'/(aliases[alias][2]+'.glb')
            g=mesh.geometry(path)
            raw=path.read_bytes();sz=struct.unpack_from('<I',raw,12)[0];doc=json.loads(raw[20:20+sz])
            assets[alias]=(g,dict(path=str(path),sha256=sha(path),vertices=len(g),collision=0,span_m=np.ptp(g,axis=0).tolist(),
                triangles=sum(doc['accessors'][p['indices']]['count']//3 for m in doc['meshes']for p in m['primitives'])))
        g,_=assets[alias];p=dict(x=point[0],z=point[1],yaw_deg=float(rng.uniform(-180,180)),pitch_deg=0.,roll_deg=0.,scale=1.,sx=1.,sy=1.,sz=1.)
        if alias.startswith('opening_verge_grass'):p.update(scale=float(rng.uniform(1.45,2.15)),sy=float(rng.uniform(1.15,1.45)))
        elif alias=='bep_nature_clover':p.update(scale=float(rng.uniform(.21,.32)),sy=.78)
        elif alias=='fern0':p.update(scale=float(rng.uniform(.75,1.1)))
        else:p.update(scale=float(rng.uniform(.32,.48)),sy=float(rng.uniform(1.05,1.30)))
        native_scale(p)
        mesh.align_plant(p,ground);height=mesh.support(p,g,ground,.025)
        v=g*np.array([p['sx'],p['sy'],p['sz']])*p['scale'];v=v@mesh.rotation(p['yaw_deg'],p['pitch_deg'],p['roll_deg']).T
        radius=float(np.linalg.norm(v[:,[0,2]],axis=1).max());center_distance,water_y=nearest(point)
        # Full horizontal mesh envelope remains a metre beyond the wet width.
        creek_clear=center_distance-radius
        if not 7<=center_distance<=12 or creek_clear<6:skips['wet_mesh_envelope']+=1;continue
        support_ground=ground(*point)
        if support_ground<water_y+.10:skips['root_below_water']+=1;continue
        route_clear=min(distance(point,a,b)for a,b in segments)-radius
        if route_clear<3.2:skips['actual_mesh_route_clearance']+=1;continue
        q=[alias,f'{p["x"]/W+.5:.12f}',f'{p["z"]/D+.5:.12f}',f'{p["yaw_deg"]:.8f}',f'{p["scale"]:.8f}','0','0','0',f'{p["pitch_deg"]:.8f}',f'{p["roll_deg"]:.8f}',f'{p["sx"]:.8f}',f'{p["sy"]:.8f}',f'{p["sz"]:.8f}',f'{p["y"]:.9f}','wind',f'{rng.uniform(.88,1.12):.5f}',f'{rng.uniform(.85,1.15):.5f}']
        new.append(dict(alias=alias,row=' '.join(q),cluster=f'creek_bank_{pocket:02d}',kind='shoreline_plant',
            point=point,x=p['x'],z=p['z'],y=p['y'],height_m=height,low_mesh_radius_m=radius,
            route_clearance_m=route_clear,creek_clearance_m=creek_clear,embed_m=.025,
            shoreline_class_exemption=True,water_y=water_y,root_ground_above_water_m=support_ground-water_y))
    assert len(new)>=200,len(new)
    allplacements=report['placements']+[{k:v for k,v in item.items()if k!='point'}for item in new]
    allassets=report['assets']|{a:data for a,(_,data)in assets.items()}
    path=np.array(routes['town'])[:,[0,2]];length=np.linalg.norm(np.diff(path,axis=0),axis=1);along=np.r_[0,np.cumsum(length)]
    probes=[CAMP]
    for s in np.arange(0,along[-1],5):
        i=min(len(length)-1,max(0,int(np.searchsorted(along,s,side='right')-1)));probes.append(path[i]+(path[i+1]-path[i])*(s-along[i])/length[i])
    peak=max(sum(allassets[p['alias']]['triangles']for p in allplacements if np.linalg.norm(np.array([p['x'],p['z']])-point)<150)for point in probes)
    assert peak<1300000,peak
    assert ground.loaded_studio_sha==sha(P/'world.studio'),'Concurrent studio edit'
    assert target.read_bytes()==before,'Concurrent scenery edit'
    rows=before.decode().splitlines();start=len(rows)+2
    rows+=['# Forest20261119 shoreline: dry low plants only; no collision; final carved banks.']+[n['row']for n in new]
    for i,n in enumerate(new):
        n.pop('point');n.update(file='scenery/authored_instances.txt',line=start+i)
    target.write_text('\n'.join(rows)+'\n');report['placements']+=new
    report['shoreline_instances']=len(new);report['added_instances']=len(report['placements'])
    report['shoreline_placement_rules']=dict(center_distance_m=[7,12],low_mesh_centerline_clearance_m=6,
        minimum_ground_above_water_m=.1,solid_or_tree_added=False,curve_sampling='Exact production2m Catmull-Rom bake formula; final height via production sampler')
    report['shoreline_skipped']=dict(skips);report['assets']=allassets
    report['species_counts']=dict(collections.Counter(p['alias']for p in report['placements']))
    report['conservative_peak_added_triangles_within150m']=peak
    report['instanced_triangles_total']=sum(allassets[p['alias']]['triangles']for p in report['placements'])
    report['output_sha256']=sha(target);report['tool_sha256']=sha(Path(__file__))
    report['changed_scenery_files']['scenery/authored_instances.txt']['after_sha256']=sha(target)
    report['shoreline_surface_studio_sha256']=ground.loaded_studio_sha
    report['minimum_route_clearance_m']=min(p['route_clearance_m']for p in report['placements'])
    report['minimum_creek_clearance_m']=min(p['creek_clearance_m']for p in report['placements'])
    report['native_authoring_validation']=validate_native_rows(report)
    (OUT/'ROW_PATCH.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(shoreline_added=len(new),peak_added_triangles=peak,minimum_dry_mesh_clearance_m=min(n['creek_clearance_m']for n in new),minimum_ground_above_water_m=min(n['root_ground_above_water_m']for n in new))))

def restore_canopy_stage():
    """V4: restore dry-root crowns and compose visible camp foreground masses.

    Call only after the parent confirms all native views have exited. The15m
    blanket used in v3 was visually wrong for mature trees: wide crowns safely
    overhang water, while only their basal trunk/root envelope needs dry ground.
    """
    assert N.name=='scenery-forest-2026-11-19' and not (N/'PUBLISHED.json').exists()
    target=P/'scenery/authored_instances.txt';before=target.read_bytes()
    report=json.loads((OUT/'ROW_PATCH.json').read_text());assert not report.get('canopy_restoration_v4')
    archive=OUT/'before-canopy-restoration-v4';archive.mkdir(exist_ok=False)
    for file in (P/'scenery').glob('*.txt'):(archive/file.name).write_bytes(file.read_bytes())
    (archive/'ROW_PATCH.json').write_bytes((OUT/'ROW_PATCH.json').read_bytes())
    (archive/'build_forest_layers.py').write_bytes(Path(__file__).read_bytes())
    mesh_path=ROOT/'docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py'
    spec=importlib.util.spec_from_file_location('actual_mesh',mesh_path);mesh=importlib.util.module_from_spec(spec);spec.loader.exec_module(mesh)
    ground,helper=sampler();studio=(P/'world.studio').read_text().splitlines()
    aliases={q[1]:q for line in studio if (q:=shlex.split(line)) and q[0]=='asset'}
    r=next(shlex.split(json.loads(line[9:]))for line in studio if line.startswith('surfaces ')and json.loads(line[9:]).startswith('ribbon "West woodland creek"'))
    controls=np.array([float(v)for v in r[9:]]).reshape(-1,3);samples=[controls[0]]
    for i in range(len(controls)-1):
        a,b,c,d=controls[max(0,i-1)],controls[i],controls[i+1],controls[min(i+2,len(controls)-1)]
        count=max(1,math.ceil(np.linalg.norm((c-b)[[0,2]])/2))
        for k in range(1,count+1):
            t=k/count;q=.5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)
            if np.linalg.norm((q-samples[-1])[[0,2]])>=.01:samples.append(q)
    samples=np.array(samples)
    def creek(point):
        a=samples[:-1];v=samples[1:]-a;dxz=v[:,[0,2]]
        t=np.clip(np.sum((point-a[:,[0,2]])*dxz,axis=1)/np.sum(dxz*dxz,axis=1),0,1)
        q=a+v*t[:,None];d=np.linalg.norm(q[:,[0,2]]-point,axis=1);i=int(np.argmin(d))
        return float(d[i]),float(q[i,1])
    routes=json.loads((P/'opening/first-journey.routes.json').read_text())['paths'];segments=[]
    for points in routes.values():
        a=np.array(points)[:,[0,2]];segments.extend(zip(a[:-1],a[1:]))
    traffic=[]
    for line in (P/'opening/road-traffic.settings').read_text().splitlines():
        q=line.split()
        if q and q[0]=='point':traffic.append(np.array([float(q[1]),float(q[3])]))
    segments+=list(zip(traffic[:-1],traffic[1:]))
    actors=np.array([[-11990.15,5171.35],[-11985.8,5171.35],[-11987.9,5168.8]])
    geometry={};assets={};existing=[];new=[];rejected=[];skips=collections.Counter()
    for file in (P/'scenery').glob('*.txt'):
        for line in file.read_text().splitlines():
            q=line.split()
            if len(q)>=14 and not q[0].startswith('#'):
                p=decode(q)
                if q[0]in TREES:existing.append(np.array([p['x'],p['z']]))
    def asset(alias):
        if alias not in geometry:
            if alias in BUILTINS:stem=BUILTINS[alias][0]
            else:stem=aliases[alias][2]+('_near'if aliases[alias][3]=='2'else'')
            path=P/'models'/(stem+'.glb')
            if not path.exists():path=P/'models'/(aliases[alias][2]+'.glb')
            g=mesh.geometry(path);raw=path.read_bytes();sz=struct.unpack_from('<I',raw,12)[0];doc=json.loads(raw[20:20+sz])
            geometry[alias]=g;assets[alias]=dict(path=str(path),sha256=sha(path),vertices=len(g),
                triangles=sum(doc['accessors'][p['indices']]['count']//3 for m in doc['meshes']for p in m['primitives']),
                collision=3 if alias in TREES else 0,span_m=np.ptp(g,axis=0).tolist())
        return geometry[alias]
    rng=np.random.default_rng(2026111914)
    def add(alias,p,label,kind,restored=None):
        point=np.array([p['x'],p['z']]);g=asset(alias);tree=kind in ['restored_canopy','mature_canopy']
        native_scale(p)
        if not tree:mesh.align_plant(p,ground)
        embed=.07 if tree else .025
        height=mesh.support(p,g,ground,embed)
        v=(g*np.array([p['sx'],p['sy'],p['sz']])*p['scale'])@mesh.rotation(p['yaw_deg'],p['pitch_deg'],p['roll_deg']).T
        # Basal actual geometry, not the high crown, controls tree bank safety.
        low=v[v[:,1]<=v[:,1].min()+1.5]if tree else v
        radius=float(np.linalg.norm(low[:,[0,2]],axis=1).max())
        # Bank crowns may overhang water; road users still require the full
        # mounted-height envelope, independently from the smaller basal band.
        road_low=v[v[:,1]+p['y']-ground(*point)<=4.2]if tree else v
        road_radius=float(np.linalg.norm(road_low[:,[0,2]],axis=1).max())
        center_distance,water_y=creek(point);clear=center_distance-radius
        route_clear=min(distance(point,a,b)for a,b in segments)-road_radius
        if clear<(6.8 if tree else 6):skips['dry_root_creek']+=1;return False
        if route_clear<(3.2 if tree else 1):skips['route_margin']+=1;return False
        if ground(*point)<water_y+.18:skips['root_water_height']+=1;return False
        if tree:
            if np.linalg.norm(point-CAMP)<7:skips['camp_tree']+=1;return False
            if kind=='mature_canopy'and existing and min(np.linalg.norm(point-t)for t in existing)<5:skips['tree_spacing']+=1;return False
        else:
            if np.linalg.norm(point-CAMP)<1.8+radius or np.linalg.norm(point-np.array([-11988,5180]))<1.5+radius:skips['fire_landing']+=1;return False
            if np.linalg.norm(actors-point,axis=1).min()<1.1+radius:skips['actor']+=1;return False
            if -11991.2-radius<point[0]<-11984.8+radius and 5163.8-radius<point[1]<5170.5+radius:skips['tent']+=1;return False
            # Short talk/approach sightline remains open; low herbs can flank it.
            if distance(point,np.array([-11988.,5180.]),np.array([-11988.,5171.]))<.75+radius:skips['talk_approach']+=1;return False
            foreground_points=[np.array([n['x'],n['z']])for n in new if n['kind']=='camp_foreground']
            if foreground_points and min(np.linalg.norm(point-q)for q in foreground_points)<.3:skips['plant_spacing']+=1;return False
        q=[alias,f'{p["x"]/W+.5:.12f}',f'{p["z"]/D+.5:.12f}',f'{p["yaw_deg"]:.8f}',f'{p["scale"]:.8f}','0','0','0',f'{p["pitch_deg"]:.8f}',f'{p["roll_deg"]:.8f}',f'{p["sx"]:.12f}',f'{p["sy"]:.12f}',f'{p["sz"]:.12f}',f'{p["y"]:.9f}']
        if not tree:q+=['wind',f'{rng.uniform(.88,1.12):.5f}',f'{rng.uniform(.85,1.15):.5f}']
        item=dict(alias=alias,row=' '.join(q),cluster=label,kind=kind,x=p['x'],z=p['z'],y=p['y'],height_m=height,
            low_mesh_radius_m=road_radius,basal_root_radius_m=radius,route_clearance_m=route_clear,creek_clearance_m=clear,embed_m=embed,
            creek_rule='basal_root_1.5m'if tree else'low_plant_dry_bank',water_y=water_y,root_ground_above_water_m=ground(*point)-water_y)
        if restored:item['restored_original']=restored
        new.append(item)
        if tree:existing.append(point)
        return True
    # Recover exact owned mature trees wherever actual basal roots stay dry.
    for file,items in report['removals'].items():
        for item in items:
            if item['alias']not in ['nm_city_region_oak','nm_city_region_beech']:continue
            p=decode(item['before'].split())
            if not add(item['alias'],p,'restored_owned_canopy','restored_canopy',dict(file=file,line=item['line'],before=item['before'])):
                rejected.append(dict(file=file,line=item['line'],alias=item['alias'],x=p['x'],z=p['z']))
    restored_count=len(new)
    canopy=[[-12039,5121],[-12025,5124],[-12012,5119],[-11998,5118],[-11981,5117],[-11965,5124],[-11950,5119],[-11936,5127],
        [-12042,5146],[-12030,5135],[-12004,5128],[-11970,5144],[-11951,5146],[-11935,5157],[-11942,5136],[-11983,5124],
        [-12038,5112],[-12023,5111],[-11968,5112],[-11951,5133],[-11935,5113],[-11958,5155],[-11975,5133],[-11942,5149],
        [-12032,5154],[-11963,5134],[-11940,5120],[-12006,5109],[-11989,5108],[-11951,5108]]
    for i,point in enumerate(canopy):
        if len(new)-restored_count>=18:break
        alias='nm_city_region_beech'if i%3==0 else'nm_city_region_oak'
        scale=float(rng.uniform(.48,.57)if alias.endswith('beech')else rng.uniform(.94,1.13))
        p=dict(x=point[0],z=point[1],yaw_deg=float(rng.uniform(-180,180)),pitch_deg=0.,roll_deg=0.,scale=scale,sx=1.,sy=1.,sz=1.)
        add(alias,p,f'camp_backdrop_crown_{i:02d}','mature_canopy')
    canopy_count=len(new)-restored_count
    assert canopy_count>=12,canopy_count
    foreground=[[-11996,5186],[-11979,5185],[-11998,5178],[-11978,5177],[-11997,5169],[-11978,5166],[-11994,5159],[-11981,5160]]
    species=['opening_verge_grass_long','opening_verge_grass_broad','opening_verge_grass_long','bep_nature_clover','opening_verge_grass_long','fern0','opening_verge_grass_broad','opening_verge_grass_long']
    for i,center in enumerate(foreground):
        accepted=0
        for attempt in range(180):
            if accepted>=25:break
            angle=attempt*2.399963+float(rng.uniform(-.2,.2));radius=float(rng.uniform(.3,2.5))
            point=np.asarray(center)+[math.cos(angle)*radius,math.sin(angle)*radius*.7];alias=species[accepted%len(species)]
            p=dict(x=point[0],z=point[1],yaw_deg=float(rng.uniform(-180,180)),pitch_deg=0.,roll_deg=0.,scale=1.,sx=1.,sy=1.,sz=1.)
            if alias=='fern0':p.update(scale=float(rng.uniform(.72,1.1)))
            elif alias=='bep_nature_clover':p.update(scale=float(rng.uniform(.2,.32)),sy=.8)
            else:p.update(scale=float(rng.uniform(1.45,2.15)),sy=float(rng.uniform(1.1,1.4)))
            accepted+=int(add(alias,p,f'camp_foreground_{i:02d}','camp_foreground'))
    foreground_count=len(new)-restored_count-canopy_count
    assert foreground_count>=150,foreground_count
    allplacements=report['placements']+new;allassets=report['assets']|assets
    path=np.array(routes['town'])[:,[0,2]];length=np.linalg.norm(np.diff(path,axis=0),axis=1);along=np.r_[0,np.cumsum(length)];probes=[CAMP]
    for s in np.arange(0,along[-1],5):
        i=min(len(length)-1,max(0,int(np.searchsorted(along,s,side='right')-1)));probes.append(path[i]+(path[i+1]-path[i])*(s-along[i])/length[i])
    peak_new=max(sum(allassets[p['alias']]['triangles']for p in allplacements if p['kind']!='restored_canopy'and np.linalg.norm(np.array([p['x'],p['z']])-point)<150)for point in probes)
    peak_with_restored=max(sum(allassets[p['alias']]['triangles']for p in allplacements if np.linalg.norm(np.array([p['x'],p['z']])-point)<150)for point in probes)
    assert peak_new<1500000,(peak_new,peak_with_restored)
    assert ground.loaded_studio_sha==sha(P/'world.studio')and target.read_bytes()==before
    lines=before.decode().splitlines();start=len(lines)+2
    lines+=['# Forest20261119 v4: restored dry-root canopy and composed camp foreground.']+[p['row']for p in new]
    for i,item in enumerate(new):item.update(file='scenery/authored_instances.txt',line=start+i)
    target.write_text('\n'.join(lines)+'\n');report['placements']+=new;report['added_instances']=len(report['placements']);report['assets']=allassets
    report['species_counts']=dict(collections.Counter(p['alias']for p in report['placements']))
    report['canopy_restoration_v4']=dict(restored_instances=restored_count,new_mature_canopy=canopy_count,foreground_instances=foreground_count,
        rejected_restorations=rejected,tree_basal_band_m=1.5,tree_minimum_dry_root_clearance_m=6.8,
        tree_route_margin_m=3.2,tree_route_height_envelope_m=4.2,foreground_full_mesh_route_margin_m=1,fire_clearance_m=1.8,actor_clearance_m=1.1,landing_clearance_m=1.5,
        net_removed_original_instances=report['removed_instances']-restored_count,skips=dict(skips),surface_studio_sha256=ground.loaded_studio_sha)
    report['conservative_peak_added_triangles_within150m']=peak_new
    report['conservative_peak_new_plus_restored_triangles_within150m']=peak_with_restored
    report['instanced_triangles_total']=sum(allassets[p['alias']]['triangles']for p in report['placements'])
    report['minimum_route_clearance_m']=min(p['route_clearance_m']for p in report['placements'])
    report['minimum_creek_clearance_m']=min(p['creek_clearance_m']for p in report['placements'])
    report['output_sha256']=sha(target);report['tool_sha256']=sha(Path(__file__))
    report['changed_scenery_files']['scenery/authored_instances.txt']['after_sha256']=sha(target)
    report['final_surface_regrounded']=True;report['final_surface_studio_sha256']=ground.loaded_studio_sha
    report['native_authoring_validation']=validate_native_rows(report)
    (OUT/'ROW_PATCH.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(restored=restored_count,new_canopy=canopy_count,foreground=foreground_count,peak_new=peak_new,peak_with_restored=peak_with_restored)))

def relocate_foreground_stage():
    """V5: move exactly the200 existing foreground plants into actual view.

    Camera-fixed first-person source-view composition; retains alias, uniform
    and axis scales, yaw and wind, so no geometry/texture/count cost is added.
    """
    assert N.name=='scenery-forest-2026-11-19' and not (N/'PUBLISHED.json').exists()
    target=P/'scenery/authored_instances.txt';before=target.read_bytes();lines=before.decode().splitlines()
    report=json.loads((OUT/'ROW_PATCH.json').read_text());assert not report.get('foreground_relocation_v5')
    archive=OUT/'before-foreground-relocation-v5'
    assert (archive/'authored_instances.txt').read_bytes()==before and (archive/'ROW_PATCH.json').read_bytes()==(OUT/'ROW_PATCH.json').read_bytes()
    mesh_path=ROOT/'docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py'
    spec=importlib.util.spec_from_file_location('actual_mesh',mesh_path);mesh=importlib.util.module_from_spec(spec);spec.loader.exec_module(mesh)
    ground,helper=sampler();routes=json.loads((P/'opening/first-journey.routes.json').read_text())['paths'];segments=[]
    for points in routes.values():
        a=np.array(points)[:,[0,2]];segments.extend(zip(a[:-1],a[1:]))
    traffic=[]
    for line in (P/'opening/road-traffic.settings').read_text().splitlines():
        q=line.split()
        if q and q[0]=='point':traffic.append(np.array([float(q[1]),float(q[3])]))
    segments+=list(zip(traffic[:-1],traffic[1:]))
    studio=(P/'world.studio').read_text().splitlines()
    r=next(shlex.split(json.loads(line[9:]))for line in studio if line.startswith('surfaces ')and json.loads(line[9:]).startswith('ribbon "West woodland creek"'))
    controls=np.array([float(v)for v in r[9:]]).reshape(-1,3);samples=[controls[0]]
    for i in range(len(controls)-1):
        a,b,c,d=controls[max(0,i-1)],controls[i],controls[i+1],controls[min(i+2,len(controls)-1)]
        count=max(1,math.ceil(np.linalg.norm((c-b)[[0,2]])/2))
        for k in range(1,count+1):
            t=k/count;q=.5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)
            if np.linalg.norm((q-samples[-1])[[0,2]])>=.01:samples.append(q)
    samples=np.array(samples)
    def creek(point):
        a=samples[:-1];v=samples[1:]-a;dxz=v[:,[0,2]]
        t=np.clip(np.sum((point-a[:,[0,2]])*dxz,axis=1)/np.sum(dxz*dxz,axis=1),0,1)
        q=a+v*t[:,None];d=np.linalg.norm(q[:,[0,2]]-point,axis=1);i=int(np.argmin(d));return float(d[i]),float(q[i,1])
    # Exact matched native source-view camera, not merely an XZ radius/count.
    eye=np.array([-11982.,13.5500001907,5180.]);yaw=math.atan2(-6,8);pitch=math.atan2(12.4-eye[1],10.)
    forward=np.array([math.sin(yaw)*math.cos(pitch),math.sin(pitch),-math.cos(yaw)*math.cos(pitch)])
    right=np.array([math.cos(yaw),0,math.sin(yaw)]);up=np.cross(right,forward)
    tan_v=math.tan(math.pi/6);aspect=16/9
    def frustum(v):
        delta=v-eye;depth=delta@forward;safe=np.maximum(depth,1e-8);x=(delta@right)/(safe*tan_v*aspect);y=(delta@up)/(safe*tan_v)
        mask=(depth>.12)&(abs(x)<.97)&(abs(y)<.97)
        return dict(fraction=float(mask.mean()),median_ndc=[float(np.median(x)),float(np.median(y))],
            min_depth_m=float(depth.min()),max_depth_m=float(depth.max()),ndc_bounds=[float(x.min()),float(y.min()),float(x.max()),float(y.max())])
    actors=np.array([[-11990.15,5171.35],[-11985.8,5171.35],[-11987.9,5168.8]])
    # Pairs of visible flanking crescents; centre remains a readable worn path.
    anchors=[[-11982.48,5176.36],[-11990.78,5178.46],[-11982.84,5174.38],[-11991.36,5177.02],
             [-11983.24,5172.18],[-11992.48,5175.36],[-11983.56,5169.92],[-11993.46,5174.22]]
    rng=np.random.default_rng(2026111915);geometry={};actual={};selected=[];changes=[];skips=collections.Counter()
    # The independent GLB audit recorded the exact native all-primitive Y
    # normalization; these four assets shift0m except fern's5.69nanometres.
    oldaudit_path=OUT/'INDEPENDENT_REVIEW.json';oldaudit=json.loads(oldaudit_path.read_text());normal={a['alias']:a['runtime_y_normalization_m']for a in oldaudit['actual_geometry']}
    foreground=[p for p in report['placements']if p['kind']=='camp_foreground'];assert len(foreground)==200
    for index,item in enumerate(foreground):
        q=item['row'].split();alias=item['alias'];old=decode(q);assert lines[item['line']-1]==item['row']
        if alias not in geometry:
            path=Path(report['assets'][alias]['path']);assert sha(path)==report['assets'][alias]['sha256']
            geometry[alias]=mesh.geometry(path);actual[alias]=geometry[alias].copy();actual[alias][:,1]-=normal[alias]
        g=geometry[alias];render=actual[alias];oldv=(render*np.array([old['sx'],old['sy'],old['sz']])*old['scale'])@mesh.rotation(old['yaw_deg'],old['pitch_deg'],old['roll_deg']).T+np.array([old['x'],old['y'],old['z']]);oldview=frustum(oldv)
        anchor=index%8;chosen=None
        for attempt in range(1200):
            a=(anchor+(attempt//180)*2)%8;radius=1.55 if a%2 else 1.25
            point=np.array(anchors[a])+rng.uniform([-radius,-radius*.70],[radius,radius*.70])
            if selected and min(np.linalg.norm(point-p)for p in selected)<.27:skips['spacing']+=1;continue
            p=old.copy();p.update(x=point[0],z=point[1]);mesh.align_plant(p,ground);height=mesh.support(p,g,ground,item['embed_m'])
            rot=mesh.rotation(p['yaw_deg'],p['pitch_deg'],p['roll_deg']);v=(render*np.array([p['sx'],p['sy'],p['sz']])*p['scale'])@rot.T
            mesh_radius=float(np.linalg.norm(v[:,[0,2]],axis=1).max());route_clear=min(distance(point,a,b)for a,b in segments)-mesh_radius
            if route_clear<1:skips['route']+=1;continue
            if np.linalg.norm(point-CAMP)<1.8+mesh_radius or np.linalg.norm(point-np.array([-11988,5180]))<1.5+mesh_radius:skips['fire_landing']+=1;continue
            if np.linalg.norm(actors-point,axis=1).min()<1.1+mesh_radius:skips['actor']+=1;continue
            if -11991.2-mesh_radius<point[0]<-11984.8+mesh_radius and 5163.8-mesh_radius<point[1]<5170.5+mesh_radius:skips['tent']+=1;continue
            if distance(point,np.array([-11988.,5180.]),np.array([-11988.,5171.]))<.75+mesh_radius:skips['talk']+=1;continue
            center_distance,water_y=creek(point);creek_clear=center_distance-mesh_radius
            if creek_clear<6 or ground(*point)<water_y+.18:skips['wet']+=1;continue
            view=frustum(v+np.array([p['x'],p['y'],p['z']]))
            if view['fraction']<.9 or not -.92<view['median_ndc'][0]<.92 or not -.90<view['median_ndc'][1]<.1:skips['not_framed']+=1;continue
            chosen=(p,height,mesh_radius,route_clear,creek_clear,water_y,view,a);break
        assert chosen is not None,(index,alias,dict(skips))
        p,height,mesh_radius,route_clear,creek_clear,water_y,view,a=chosen
        before_row=item['row'];q[1]=f'{p["x"]/W+.5:.12f}';q[2]=f'{p["z"]/D+.5:.12f}'
        q[8]=f'{p["pitch_deg"]:.8f}';q[9]=f'{p["roll_deg"]:.8f}';q[13]=f'{p["y"]:.9f}'
        after_row=' '.join(q);lines[item['line']-1]=after_row
        item.update(row=after_row,cluster=f'camera_visible_crescent_{a}',x=p['x'],z=p['z'],y=p['y'],height_m=height,
            low_mesh_radius_m=mesh_radius,basal_root_radius_m=mesh_radius,route_clearance_m=route_clear,
            creek_clearance_m=creek_clear,water_y=water_y,root_ground_above_water_m=ground(*point)-water_y,
            camp_eye_frustum=view)
        selected.append(np.array([p['x'],p['z']]))
        changes.append(dict(line=item['line'],alias=alias,before=before_row,after=after_row,before_view=oldview,after_view=view))
    assert ground.loaded_studio_sha==sha(P/'world.studio')and target.read_bytes()==before
    # Verify unchanged lines, alias, shape, wind and all canopy records exactly.
    oldlines=before.decode().splitlines();changed={c['line']-1 for c in changes}
    assert len(changed)==200 and all(a==b for i,(a,b)in enumerate(zip(oldlines,lines))if i not in changed)
    for c in changes:
        a,b=c['before'].split(),c['after'].split();assert all(a[k]==b[k]for k in [0,3,4,5,6,7,10,11,12,14,15,16])
    # Relocation preserves total geometry, while rechecking the actual near
    # residency window rather than assuming a moved row has the same draw cost.
    path=np.array(routes['town'])[:,[0,2]];length=np.linalg.norm(np.diff(path,axis=0),axis=1);along=np.r_[0,np.cumsum(length)];probes=[CAMP]
    for s in np.arange(0,along[-1],5):
        i=min(len(length)-1,max(0,int(np.searchsorted(along,s,side='right')-1)));probes.append(path[i]+(path[i+1]-path[i])*(s-along[i])/length[i])
    peak_new=max(sum(report['assets'][p['alias']]['triangles']for p in report['placements']if p['kind']!='restored_canopy'and np.linalg.norm(np.array([p['x'],p['z']])-point)<150)for point in probes)
    peak_with_restored=max(sum(report['assets'][p['alias']]['triangles']for p in report['placements']if np.linalg.norm(np.array([p['x'],p['z']])-point)<150)for point in probes)
    assert peak_new<1500000
    report['conservative_peak_added_triangles_within150m']=peak_new
    report['conservative_peak_new_plus_restored_triangles_within150m']=peak_with_restored
    report['minimum_route_clearance_m']=min(p['route_clearance_m']for p in report['placements'])
    report['minimum_creek_clearance_m']=min(p['creek_clearance_m']for p in report['placements'])
    target.write_text('\n'.join(lines)+'\n');report['output_sha256']=sha(target);report['tool_sha256']=sha(Path(__file__))
    report['changed_scenery_files']['scenery/authored_instances.txt']['after_sha256']=sha(target)
    report['final_surface_regrounded']=True;report['final_surface_studio_sha256']=ground.loaded_studio_sha
    report['native_authoring_validation']=validate_native_rows(report)
    report['foreground_relocation_v5']=dict(moved_rows=200,added_rows=0,removed_rows=0,canopy_or_other_rows_changed=False,
        shape_alias_yaw_wind_preserved=True,camera_eye=eye.tolist(),camera_yaw=yaw,camera_pitch=pitch,fov_vertical=math.pi/3,aspect=aspect,
        anchors_xz=anchors,minimum_visible_vertex_fraction=min(c['after_view']['fraction']for c in changes),
        before_rows_at_least90percent_in_frustum=sum(c['before_view']['fraction']>=.9 for c in changes),
        after_rows_at_least90percent_in_frustum=sum(c['after_view']['fraction']>=.9 for c in changes),
        minimum_actual_route_clearance_m=min(p['route_clearance_m']for p in foreground),
        production_surface_sha256=ground.loaded_studio_sha,asset_normalization_receipt_sha256=sha(oldaudit_path),
        camera_view_receipt_sha256=sha(N/'review/forest-views/forest-v4/candidate/camp-eye/EXECUTION.json'),changes=changes,skips=dict(skips),
        limitation='CPU static visible-mesh frustum composition; actual native appearance and dynamic wind still require new captures.')
    (OUT/'ROW_PATCH.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report['foreground_relocation_v5'].items()if k in ['moved_rows','minimum_visible_vertex_fraction','before_rows_at_least90percent_in_frustum','after_rows_at_least90percent_in_frustum','minimum_actual_route_clearance_m']}))

def main(reground=False):
    assert N.name=='scenery-forest-2026-11-19' and not (N/'PUBLISHED.json').exists()
    target=P/'scenery/authored_instances.txt'
    assert target.is_file() and not target.is_symlink()
    OUT.mkdir(parents=True,exist_ok=True)
    mesh_path=ROOT/'docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py'
    spec=importlib.util.spec_from_file_location('actual_mesh',mesh_path);mesh=importlib.util.module_from_spec(spec);spec.loader.exec_module(mesh)
    ground,helper=sampler()
    studio=(P/'world.studio').read_text().splitlines()
    aliases={q[1]:q for line in studio if (q:=shlex.split(line)) and q[0]=='asset'}
    geometry,budget={},{}
    def asset(alias):
        if alias in geometry:return geometry[alias]
        if alias in BUILTINS:stem,collision=BUILTINS[alias]
        else:
            q=aliases[alias];stem=q[2]+('_near' if q[3]=='2' else '');collision=int(q[4])
        path=P/'models'/(stem+'.glb')
        if not path.exists():path=P/'models'/(aliases[alias][2]+'.glb')
        g=mesh.geometry(path);raw=path.read_bytes();size=struct.unpack_from('<I',raw,12)[0];doc=json.loads(raw[20:20+size])
        geometry[alias]=g
        budget[alias]=dict(path=str(path),sha256=sha(path),vertices=len(g),
            triangles=sum(doc['accessors'][p['indices']]['count']//3 for m in doc['meshes']for p in m['primitives']),
            collision=collision,span_m=np.ptp(g,axis=0).tolist())
        return g

    if reground:
        report=json.loads((OUT/'ROW_PATCH.json').read_text());lines=target.read_text().splitlines()
        for item in report['placements']:
            i=item['line']-1;assert lines[i]==item['row'],'Unexpected changed new row'
            pose=decode(lines[i].split());g=asset(item['alias'])
            if item['kind']not in ['restored_canopy','mature_canopy']:mesh.align_plant(pose,ground)
            mesh.support(pose,g,ground,item['embed_m']);q=lines[i].split()
            q[8],q[9],q[13]=f'{pose["pitch_deg"]:.8f}',f'{pose["roll_deg"]:.8f}',f'{pose["y"]:.9f}'
            lines[i]=' '.join(q);item['row']=lines[i];item['y']=pose['y']
        assert ground.loaded_studio_sha==sha(P/'world.studio'),'Concurrent studio edit invalidated sampled ground'
        target.write_text('\n'.join(lines)+'\n');report['final_surface_regrounded']=True
        report['output_sha256']=sha(target);report['final_surface_studio_sha256']=ground.loaded_studio_sha
        report['tool_sha256']=sha(Path(__file__))
        report['changed_scenery_files']['scenery/authored_instances.txt']['after_sha256']=sha(target)
        report['limitations']=[s for s in report['limitations']if 'run --reground'not in s]
        report['native_authoring_validation']=validate_native_rows(report)
        (OUT/'ROW_PATCH.json').write_text(json.dumps(report,indent=2)+'\n');print('regrounded',len(report['placements']));return

    assert not (OUT/'ROW_PATCH.json').exists(),'Never stack this pass'
    before=target.read_bytes();source_hashes={str(p):sha(p)for p in (P/'scenery').glob('*.txt')}
    routes=json.loads((P/'opening/first-journey.routes.json').read_text())['paths'];segments=[]
    for points in routes.values():
        a=np.array(points)[:,[0,2]];segments.extend(zip(a[:-1],a[1:]))
    traffic=[]
    for line in (P/'opening/road-traffic.settings').read_text().splitlines():
        q=line.split()
        if q and q[0]=='point':traffic.append(np.array([float(q[1]),float(q[3])]))
    segments+=list(zip(traffic[:-1],traffic[1:]))
    actors=np.array([[-11990.15,5171.35],[-11985.8,5171.35],[-11987.9,5168.8]])
    protected=[(CAMP,4.0),(np.array([-11988.,5180.]),2.7),(np.array([-11933.,5312.]),8.5),
               (np.array([-11930.5,5320.]),4.6),(np.array([-11968.,5582.]),5.5)]
    existing=[];existing_trees=[];counts=collections.Counter();removals=collections.defaultdict(list);original_files={}
    def nature(alias):
        return alias in BUILTINS or alias in TREES or alias.startswith('bep_nature_') or alias in ['opening_verge_grass_long','opening_verge_grass_broad','opening_road_roots','opening_road_stone']
    for file in (P/'scenery').glob('*.txt'):
        original_files[str(file.relative_to(P))]=file.read_bytes()
        for line_no,line in enumerate(file.read_text().splitlines(),1):
            q=line.split()
            if len(q)<14 or q[0].startswith('#'):continue
            try:pose=decode(q)
            except ValueError:continue
            point=np.array([pose['x'],pose['z']])
            creek_center=min(distance(point,a,b)for a,b in zip(CREEK[:-1],CREEK[1:]))
            if nature(q[0]) and creek_center<30:
                g=asset(q[0]);v=g*np.array([pose['sx'],pose['sy'],pose['sz']])*pose['scale'];v=v@mesh.rotation(pose['yaw_deg'],pose['pitch_deg'],pose['roll_deg']).T
                low=v[v[:,1]<=v[:,1].min()+4.2]if q[0]in TREES else v
                edge=creek_center-float(np.linalg.norm(low[:,[0,2]],axis=1).max())
                if edge<15:
                    removals[str(file.relative_to(P))].append(dict(line=line_no,alias=q[0],before=line,x=pose['x'],z=pose['z'],low_mesh_creek_clearance_m=edge));continue
            counts[q[0]]+=1;existing.append((q[0],point))
            if q[0] in TREES and np.linalg.norm(point-CAMP)<220:existing_trees.append(point)
    rng=np.random.default_rng(2026111907);placements=[];skips=collections.Counter();new_points=[]
    def add(alias,point,label,kind='plant'):
        point=np.asarray(point,dtype=float)
        if any(np.linalg.norm(point-c)<r for c,r in protected):skips['interaction_area']+=1;return False
        if np.linalg.norm(actors-point,axis=1).min()<3.5:skips['actor']+=1;return False
        if any(np.linalg.norm(point-p)<(.19 if 'grass' in alias else .38)for _,p in new_points):skips['new_spacing']+=1;return False
        g=asset(alias);p=dict(x=point[0],z=point[1],yaw_deg=float(rng.uniform(-180,180)),pitch_deg=0.,roll_deg=0.,scale=1.,sx=1.,sy=1.,sz=1.)
        if alias.startswith('opening_verge_grass'):
            p.update(scale=float(rng.uniform(1.15,1.95)),sy=float(rng.uniform(1.15,1.55)))
        elif alias in ['bep_nature_grass_long','bep_nature_grass_broad']:
            p.update(scale=float(rng.uniform(.36,.64)),sy=float(rng.uniform(1.05,1.4)))
        elif alias=='bep_nature_clover':p.update(scale=float(rng.uniform(.19,.37)),sy=float(rng.uniform(.65,.90)))
        elif alias=='bep_nature_fern':p.update(scale=float(rng.uniform(.40,.65)),sy=float(rng.uniform(.88,1.12)))
        elif alias=='fern0':p.update(scale=float(rng.uniform(.76,1.3)))
        elif alias.startswith('bep_nature_bush'):p.update(scale=float(rng.uniform(.11,.16)),sx=float(rng.uniform(.72,.96)),sy=float(rng.uniform(.55,.82)),sz=float(rng.uniform(.72,.96)))
        elif alias=='nm_city_region_oak':p.update(scale=float(rng.uniform(.37,.57)))
        elif alias=='nm_city_region_beech':p.update(scale=float(rng.uniform(.22,.34)))
        elif alias=='broadleaf':p.update(scale=float(rng.uniform(.65,1.0)))
        elif alias=='pine_young':p.update(scale=float(rng.uniform(1.05,1.55)))
        elif alias=='deadwood':p.update(scale=float(rng.uniform(.55,.85)),sy=.7)
        else:p.update(scale=float(rng.uniform(.22,.36)),sy=.48)
        native_scale(p)
        if kind!='tree':mesh.align_plant(p,ground)
        if abs(p['pitch_deg'])>30 or abs(p['roll_deg'])>30:skips['steep']+=1;return False
        embed=.018 if 'grass' in alias else .025 if kind=='plant' else .06
        height=mesh.support(p,g,ground,embed)
        transformed=g*np.array([p['sx'],p['sy'],p['sz']])*p['scale'];transformed=transformed@mesh.rotation(p['yaw_deg'],p['pitch_deg'],p['roll_deg']).T
        # Mounted travel needs clear low branches, not an exclusion circle for
        # high canopy leaves. Existing trunk-collision aliases remain intact.
        low=transformed[transformed[:,1]+p['y']-ground(*point)<4.2] if kind=='tree' else transformed
        radius=float(np.linalg.norm(low[:,[0,2]],axis=1).max())
        route_clear=min(distance(point,a,b)for a,b in segments)-radius
        creek_clear=min(distance(point,a,b)for a,b in zip(CREEK[:-1],CREEK[1:]))-(max(.3*p['scale'],.18)if kind=='tree'else radius)
        if route_clear<(3.6 if budget[alias]['collision'] else 3.2):skips['actual_low_mesh_route_clearance']+=1;return False
        if creek_clear<15:skips['creek']+=1;return False
        if kind=='tree' and existing_trees and min(np.linalg.norm(point-t)for t in existing_trees)<4.6:skips['existing_tree_spacing']+=1;return False
        if kind=='plant' and existing_trees and min(np.linalg.norm(point-t)for t in existing_trees)<.75:skips['trunk']+=1;return False
        q=[alias,f'{p["x"]/W+.5:.12f}',f'{p["z"]/D+.5:.12f}',f'{p["yaw_deg"]:.8f}',f'{p["scale"]:.8f}','0','0','0',f'{p["pitch_deg"]:.8f}',f'{p["roll_deg"]:.8f}',f'{p["sx"]:.8f}',f'{p["sy"]:.8f}',f'{p["sz"]:.8f}',f'{p["y"]:.9f}']
        if kind=='plant':q+=['wind',f'{rng.uniform(.88,1.12):.5f}',f'{rng.uniform(.85,1.15):.5f}']
        row=' '.join(q);assert len(' '.join(q[:14]))<=256 and len(row)<=320
        placements.append(dict(alias=alias,row=row,cluster=label,kind=kind,x=p['x'],z=p['z'],y=p['y'],height_m=height,
            low_mesh_radius_m=radius,route_clearance_m=route_clear,creek_clearance_m=creek_clear,embed_m=embed))
        new_points.append((alias,point));return True

    roots=[]
    for t in sorted(existing_trees,key=lambda t:float(np.linalg.norm(t-CAMP))):
        if np.linalg.norm(t-CAMP)>86 or min(distance(t,a,b)for a,b in zip(CREEK[:-1],CREEK[1:]))<17:continue
        if roots and min(np.linalg.norm(t-r)for r in roots)<3.5:continue
        roots.append(t)
        if len(roots)==58:break
    clusters=[dict(name=f'root_floor_{i:02d}',center=t,radius=3.3,count=20)for i,t in enumerate(roots)]
    for i,p in enumerate([[-11975,5161],[-11974,5171],[-11977,5185],[-11982,5194],[-11968,5195],[-11962,5158],[-11958,5177],[-11996,5191]]):
        clusters.append(dict(name=f'camp_lush_mass_{i}',center=np.array(p),radius=3.7,count=54))
    path=np.array(routes['town'])[:,[0,2]];length=np.linalg.norm(np.diff(path,axis=0),axis=1);along=np.r_[0,np.cumsum(length)]
    def road(s):
        i=min(len(length)-1,max(0,int(np.searchsorted(along,s,side='right')-1)));v=(path[i+1]-path[i])/length[i]
        return path[i]+v*(s-along[i]),np.array([v[1],-v[0]])
    for i,s in enumerate([35,48,62,78,94,112,130,148,169,194,232,274,318,362,398]):
        center,right=road(s);side=-1 if i%3 else 1
        clusters.append(dict(name=f'road_understory_{i:02d}',center=center+right*side*(9.5+1.1*(i%3)),radius=3.8 if s<180 else 3.0,count=38 if s<180 else 25))
    species=['opening_verge_grass_long','opening_verge_grass_broad','fern0','opening_verge_grass_long',
             'bep_nature_clover','bep_nature_grass_long','bep_nature_fern','opening_verge_grass_broad',
             'bep_nature_clover','fern0','opening_verge_grass_long','bep_nature_grass_broad']
    for cluster in clusters:
        count=0
        for k in range(cluster['count']*7):
            if count>=cluster['count']:break
            angle=k*2.399963229728653+rng.uniform(-.2,.2);radius=cluster['radius']*math.sqrt(rng.uniform(.10,1))
            point=cluster['center']+np.array([math.cos(angle)*radius,math.sin(angle)*radius*.78])
            count+=int(add(species[count%len(species)],point,cluster['name']))
        cluster['accepted']=count
        if count>=12:
            add('bep_nature_bush_a'if len(cluster['name'])%2 else'bep_nature_bush_b',cluster['center']+[1.3,-1.1],cluster['name'])
    # A few genuine younger silhouettes fill the clear trunk-to-canopy gap.
    young=[[-11962,5143],[-11949,5164],[-11964,5189],[-11991,5209],[-11966,5224],[-11930,5227],
           [-11928,5250],[-11960,5261],[-11956,5290],[-11915,5305],[-11909,5340],[-11961,5355],
           [-11975,5410],[-11924,5440],[-11986,5485],[-11943,5510]]
    for i,point in enumerate(young):add('broadleaf'if i%3==0 else'pine_young'if i%3==1 else'nm_city_region_oak',point,f'young_woodland_{i}', 'tree')
    for i,point in enumerate([[-11955,5121],[-11929,5150],[-11934,5197],[-11905,5235],[-11974,5250],[-11907,5288],[-11964,5332],[-11910,5366]]):
        add('nm_city_region_beech'if i%3 else'nm_city_region_oak',point,f'background_closure_{i}','tree')
    for i,t in enumerate(roots[::6]):add('opening_road_roots',t+[1.1,.8],f'root_flare_{i}','detail')
    for i,point in enumerate([[-11963,5151],[-11971,5218],[-11924,5270],[-11960,5361]]):add('deadwood',point,f'fallen_branch_{i}','detail')
    assert len(placements)>=1400,len(placements)
    added=collections.Counter(p['alias']for p in placements)
    assert sum(counts.values())+len(placements)<16384
    assert all(counts[a]+c<=4096 for a,c in added.items())
    peak=[]
    for point in [CAMP]+[road(float(s))[0]for s in np.arange(0,along[-1],5)]:
        active=[p for p in placements if np.linalg.norm(np.array([p['x'],p['z']])-point)<150]
        peak.append(sum(budget[p['alias']]['triangles']for p in active))
    assert max(peak)<1250000,max(peak)
    assert target.read_bytes()==before,'Concurrent scenery edit'
    original=[line for i,line in enumerate(before.decode().splitlines(),1)if i not in {r['line']for r in removals.get('scenery/authored_instances.txt',[])}];start=len(original)+2
    lines=original+['# Forest20261119: dense rooted woodland layers; creek/traffic protected.']+[p['row']for p in placements]
    (OUT/'authored_instances.before.txt').write_bytes(before);target.write_text('\n'.join(lines)+'\n')
    changed_files={}
    for name,removed in removals.items():
        path=P/name
        if path!=target:
            assert path.read_bytes()==original_files[name] and not path.is_symlink()
            kept=[line for i,line in enumerate(original_files[name].decode().splitlines(),1)if i not in {r['line']for r in removed}]
            (OUT/(Path(name).name+'.before.txt')).write_bytes(original_files[name]);path.write_text('\n'.join(kept)+'\n')
        changed_files[name]=dict(before_sha256=hashlib.sha256(original_files[name]).hexdigest(),after_sha256=sha(path),removed_rows=len(removed))
    for i,p in enumerate(placements):p['file']='scenery/authored_instances.txt';p['line']=start+i
    report=dict(schema='valwood.forest-layer-layout.v1',staged=True,gpu_used=False,production_live_modified=False,
        tool_sha256=sha(Path(__file__)),source_scenery_sha256=source_hashes,source_studio_sha256=sha(P/'world.studio'),
        output_sha256=sha(target),added_instances=len(placements),removed_instances=sum(len(a)for a in removals.values()),removals=dict(removals),changed_scenery_files=changed_files,species_counts=dict(added),placements=placements,
        clusters=[dict(name=c['name'],center_xz=c['center'].tolist(),radius_m=c['radius'],accepted=c['accepted'])for c in clusters],
        assets=budget,conservative_peak_added_triangles_within150m=max(peak),instanced_triangles_total=sum(budget[a]['triangles']*c for a,c in added.items()),
        minimum_route_clearance_m=min(p['route_clearance_m']for p in placements),minimum_creek_clearance_m=min(p['creek_clearance_m']for p in placements),
        creek_polyline_xz=CREEK.tolist(),added_unique_asset_files=0,added_texture_files=0,skipped_candidates=dict(skips),
        ground_sampler_path=str(helper),ground_sampler_sha256=sha(helper),grounding_source_sha256=sha(mesh_path),height_sha256=sha(P/'ground/showcase.height'),
        final_surface_regrounded=False,limitations=['Actual surface-grounded at staging; run --reground after root creek/paint merge.','No new textures or mesh files; runtime residency/frame cost still requires native tests.','High tree canopies may overhang; all geometry below4.2m and solid tree roots preserve route clearance.'])
    (OUT/'ROW_PATCH.json').write_text(json.dumps(report,indent=2)+'\n')
    suggestions=[]
    for i,t in enumerate(roots[::2]):
        suggestions.append(dict(purpose='irregular rooted woodland litter under new floor layers',center_xz=t.tolist(),
            radius_m=round(4.6+.45*(i%4),2),amount=.82 if i%3 else .68,layer=1 if i%4 else 7,
            proposed_record=f'paint {t[0]:.7f} {t[1]:.7f} {4.6+.45*(i%4):.2f} {(.82 if i%3 else .68):.2f} {1 if i%4 else 7} 0'))
    (OUT/'GROUND_PAINT_PROPOSAL.json').write_text(json.dumps(dict(world_studio_not_modified=True,
        budget_note='256 paints/16 ribbons already full; root must replace selected old masks, not append.',
        broad_mask_to_recompose='paint -11977.789967299999 5215.8378071699999 48 0.78 7 0',suggestions=suggestions),indent=2)+'\n')
    print(json.dumps({k:report[k]for k in ['added_instances','species_counts','conservative_peak_added_triangles_within150m','minimum_route_clearance_m','minimum_creek_clearance_m']},indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--reground',action='store_true');p.add_argument('--paint-proposal',action='store_true');p.add_argument('--shoreline',action='store_true');p.add_argument('--fix-native-scale',action='store_true');p.add_argument('--restore-canopy',action='store_true');p.add_argument('--relocate-foreground',action='store_true');a=p.parse_args()
    if a.paint_proposal:paint_proposal()
    elif a.shoreline:shoreline_stage()
    elif a.fix_native_scale:fix_native_scale()
    elif a.restore_canopy:restore_canopy_stage()
    elif a.relocate_foreground:relocate_foreground_stage()
    else:main(a.reground)
