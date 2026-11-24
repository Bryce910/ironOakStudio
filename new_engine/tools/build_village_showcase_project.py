#!/usr/bin/env python3
"""Install the purchased city in its own project; keep original game saves intact.

Consumes the publisher-source catalog, preserving source pivots, material/paint
variants and source actor transforms. No source asset is modified. All authored
scene state stays private; only read-only base resources are symlinked.
"""
from pathlib import Path
import argparse, datetime, hashlib, json, math, re, shutil, struct
import numpy as np
from import_world_asset import cook

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'projects/valwood_visual'
DEFAULT=ROOT/'projects/valwood_city_showcase'
ORIGIN=np.array([(0.487-.5)*100800,24.1,(0.320-.5)*100800*2818/5009])
FILES=['authored','starting_area','rainforest','castle_village','tanthoon_bandit_camp','castle_streets','bep_environment','demo_finish','demo_polish']

def asset_alias(index,name):
    return 'nm_city_'+str(index)+'_'+''.join(c.lower()if c.isalnum()else'_'for c in name)[:70]

def geometry_aliases(project,assets,incremental):
    # Keep the existing immutable revisions/cache paths when new source families
    # sort before the old ones. New families append; old indices never shift.
    names=[]
    if incremental:
        metadata=json.loads((project/'city_showcase.json').read_text())
        previous=json.loads(Path(metadata['active_geometry_catalog']).read_text())
        names=sorted(a['name']for a in previous['assets'])
        studio_aliases={line.split()[1]for line in (project/'world.studio').read_text().splitlines()if line.startswith('asset ')}
        # A previous incremental update records its stable mapping explicitly.
        prior=metadata.get('asset_aliases',{name:asset_alias(i,name)for i,name in enumerate(names)})
        if not set(prior.values()).issubset(studio_aliases):raise ValueError('Installed source aliases do not match previous catalog')
    else:prior={}
    result={name:prior[name]for name in assets if name in prior}
    next_index=max([int(match.group(1))for alias in prior.values()if(match:=re.fullmatch(r'nm_city_(\d+)_.+',alias))]+[-1])+1
    for name in sorted(set(assets)-set(result)):
        requested=assets[name].get('native',{}).get('alias')
        if requested is not None:
            if not isinstance(requested,str)or not re.fullmatch(r'[A-Za-z0-9_-]{1,95}',requested):raise ValueError('Invalid native asset alias: '+name)
            result[name]=requested
        else:result[name]=asset_alias(next_index,name);next_index+=1
    if len(set(result.values()))!=len(result):raise ValueError('Source assets choose duplicate native aliases')
    return result

def asset_definition(alias,stem,asset):
    native=asset.get('native',{})
    if not isinstance(native,dict):raise ValueError('Native asset metadata must be an object')
    vegetation=native.get('vegetation',0);collision=native.get('collision',0)
    distance=native.get('distance',2000);split=native.get('split',35)
    if type(vegetation)is not int or vegetation not in [0,1,2]or type(collision)is not int or collision not in [0,1,2,3,4]:raise ValueError('Invalid native vegetation/collision flags')
    if not isinstance(distance,(int,float))or not isinstance(split,(int,float))or not math.isfinite(distance)or not math.isfinite(split)or not 30<=distance<=2000 or not 10<=split<=500 or split+4>=distance:raise ValueError('Invalid native LOD distance/split')
    return f'asset {alias} {stem} 2 {collision} {vegetation} {distance:.12g} {split:.12g} 1'

def replace_asset_definitions(text,definitions):
    lines=text.splitlines()
    if not lines or lines[0]!='VALWOOD_STUDIO 7':raise ValueError('Expected installed city studio version7')
    found=False;result=[]
    for line in lines:
        if line.startswith('asset '):
            if not found:result.extend(definitions);found=True
        else:result.append(line)
    if not found:raise ValueError('Installed city has no source asset definitions')
    return '\n'.join(result)+'\n'

def backup_geometry_state(project):
    target=project/'qa/geometry-backups'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    target.mkdir(parents=True)
    files=['world.studio','project.valwood','city.scene','city.sky','showcase.views','city.physics','city_showcase.json','publisher_showcase.json','ground/showcase.height','ground/city.coverage','ground/city-ground-coverage.png','ground/countryside.height','ground/countryside.coverage']
    files.extend('scenery/'+name+'_instances.txt'for name in FILES)
    files.extend(str(p.relative_to(project))for p in (project/'shaders').glob('*.glsl'))
    files.extend(str(p.relative_to(project))for p in (project/'qa').glob('*.route'))
    files.extend(str(p.relative_to(project))for p in (project/'qa').glob('*cameras*.json'))
    for name in files:
        source=project/name
        if source.is_file():
            destination=target/name;destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,destination)
    return target

def matrix(value):
    result=np.asarray(value,dtype=float).reshape((4,4),order='F')
    if not np.isfinite(result).all() or np.max(np.abs(result[3]-[0,0,0,1]))>1e-7:
        raise ValueError('Non-affine source placement')
    return result

def decompose(m):
    """Native T*Yaw(Y)*Pitch(X)*Roll(Z)*Scale, with explicit reconstruction."""
    axes=m[:3,:3];scale=np.linalg.norm(axes,axis=0)
    if np.any(scale<1e-8):raise ValueError('Collapsed source placement')
    r=axes/scale
    if np.linalg.det(r)<0:raise ValueError('Mirrored source placement must be cooked into its own mesh')
    if np.max(np.abs(r.T@r-np.eye(3)))>1e-5:raise ValueError('Source hierarchy shear must be baked, not silently approximated')
    pitch=math.asin(np.clip(-r[1,2],-1,1))
    if abs(math.cos(pitch))>1e-8:yaw,roll=math.atan2(r[0,2],r[2,2]),math.atan2(r[1,0],r[1,1])
    else:yaw,roll=math.atan2(-r[2,0],r[0,0]),0
    # Validate exactly the decimal uniform value that the native parser reads.
    # cbrt(.15**3) can round just below .15 even though the serialized value is
    # the valid boundary. Axis multipliers retain the original source scale.
    uniform=float(format(float(np.cbrt(np.prod(scale))),'.12g'));extra=scale/uniform
    if not .15<=uniform<=4 or np.any(extra<.1) or np.any(extra>8):raise ValueError('Source TRS exceeds native placement scale bounds0.15..4')
    return math.degrees(yaw),math.degrees(pitch),math.degrees(roll),uniform,extra

def preflight_scenery_scales(project):
    """Check installed row scales against the actual native parser contract.

    Model preparation cannot catch invalid placement rows. Keep this separate
    integration check over the files the game will open, including tree2 and
    broadleaf2.5 maxima rather than the generic static-prop maximum4.
    """
    vegetation={row.split()[1]:int(row.split()[5])for row in (project/'world.studio').read_text().splitlines()if row.startswith('asset ')}
    rows=0;errors=[]
    for name in FILES:
        path=project/'scenery'/f'{name}_instances.txt'
        for line_number,line in enumerate(path.read_text().splitlines(),1):
            fields=line.split('#',1)[0].split()
            if not fields:continue
            rows+=1;alias=fields[0]
            if len(fields)<6 or alias not in vegetation:raise ValueError(f'{path}:{line_number}: undeclared/malformed source placement')
            scale=float(fields[4]);limit=2.5 if alias=='broadleaf'else(2 if vegetation[alias]==2 else 4)
            if not math.isfinite(scale)or not .15<=scale<=limit:errors.append(f'{path}:{line_number}: {alias} scale{scale:.12g} outside0.15..{limit}')
    if errors:raise ValueError('Native scenery scale preflight rejected '+str(len(errors))+' rows: '+'; '.join(errors[:8]))
    return dict(rows=rows,minimum_uniform_scale=.15,tree_maximum_uniform_scale=2,static_maximum_uniform_scale=4,failures=0)

def skeleton(project):
    project.mkdir(parents=True,exist_ok=True)
    for name in ['models','ground','scenery','saves','shaders','qa','texture_blocks']:
        target=project/name
        if target.is_symlink():raise ValueError('Project-owned path must not be shared: '+name)
        target.mkdir(exist_ok=True)
    for name in ['audio','fonts','particles','assets','ai']:
        target=project/name;source=BASE/name
        if source.exists() and not target.exists():target.symlink_to(source.resolve(),target_is_directory=True)
    for source in (BASE/'models').iterdir():
        target=project/'models'/source.name
        if source.name!='imports' and not target.exists():target.symlink_to(source.resolve(),target_is_directory=source.is_dir())
    (project/'models/imports').mkdir(exist_ok=True)
    for source in (BASE/'ground').iterdir():
        target=project/'ground'/source.name
        if source.is_file() and not target.exists():target.symlink_to(source.resolve())
    for source in (BASE/'shaders').iterdir():
        target=project/'shaders'/source.name
        if not target.exists():
            if source.is_dir():shutil.copytree(source,target)
            else:shutil.copy2(source,target)
    for candidate in [ROOT/'assets/purchased/medieval_village/source-windows/shader-candidate',ROOT/'assets/purchased/medieval_village/city-materials/shaders']:
        for name in ['world_foliage.vs.glsl','world_foliage.fs.glsl']:
            if (candidate/name).is_file():shutil.copy2(candidate/name,project/'shaders'/name)
    if not (project/'graphics.settings').exists():shutil.copy2(BASE/'graphics.settings',project/'graphics.settings')
    # An empty explicit source list suppresses unrelated old-town fire sockets.
    (project/'showcase.flames').write_text('VALWOOD_SOURCE_FLAMES 1\n')
    for name in FILES:(project/'scenery'/f'{name}_instances.txt').write_text('# Publisher source city; private scene.\n')
    for name in ['stronghold.json','milestone_standard.json','jungle_expansion.json']:
        if (project/name).exists():raise ValueError('Incompatible inherited project marker: '+name)

def glb_geometry(path):
    raw=Path(path).read_bytes();size,kind=struct.unpack_from('<II',raw,12)
    if kind!=0x4e4f534a:raise ValueError('GLB JSON required')
    doc=json.loads(raw[20:20+size]);offset=20+size
    n,kind=struct.unpack_from('<II',raw,offset);binary=memoryview(raw)[offset+8:offset+8+n]
    def array(index):
        a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
        dtype={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']]
        width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
        if 'byteStride' in v:raise ValueError('Ground accessor must be tightly packed')
        return np.frombuffer(binary,dtype,count=a['count']*width,offset=v.get('byteOffset',0)+a.get('byteOffset',0)).reshape(-1,width)
    vertices=[];triangles=[];base=0
    for mesh in doc.get('meshes',[]):
        for p in mesh['primitives']:
            if p.get('mode',4)!=4:continue
            v=array(p['attributes']['POSITION']).astype(float);t=array(p['indices']).reshape(-1,3).astype(np.int64)
            vertices.append(v);triangles.append(t+base);base+=len(v)
    return np.concatenate(vertices),np.concatenate(triangles)

def raster_height(vertices,triangles,side=1025):
    """Highest actual source triangle; OOB cells filled only for scalar support."""
    lo=vertices[:,[0,2]].min(axis=0);hi=vertices[:,[0,2]].max(axis=0)
    spacing=(hi-lo)/(side-1);values=np.full((side,side),-np.inf,float);covered=np.zeros((side,side),bool)
    for tri in vertices[triangles]:
        q=(tri[:,[0,2]]-lo)/spacing
        mn=np.maximum(0,np.floor(q.min(axis=0)).astype(int));mx=np.minimum(side-1,np.ceil(q.max(axis=0)).astype(int))
        a,b,c=q;den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(den)<1e-12 or np.any(mx<mn):continue
        yy,xx=np.mgrid[mn[1]:mx[1]+1,mn[0]:mx[0]+1]
        w=((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/den
        u=((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/den
        v=1-w-u;inside=(w>=-1e-7)&(u>=-1e-7)&(v>=-1e-7)
        h=w*tri[0,1]+u*tri[1,1]+v*tri[2,1]
        target=values[mn[1]:mx[1]+1,mn[0]:mx[0]+1]
        np.maximum(target,np.where(inside,h,-np.inf),out=target)
        covered[mn[1]:mx[1]+1,mn[0]:mx[0]+1]|=inside
    if not covered.any():raise ValueError('Ground has no upward projected triangle coverage')
    from scipy.ndimage import distance_transform_edt
    nearest=distance_transform_edt(~covered,return_distances=False,return_indices=True)
    values[~covered]=values[tuple(nearest[:,~covered])]
    return values,covered,lo,spacing

def source_ground(project,asset,placements):
    vertices,triangles=glb_geometry(asset['files'][0]);all_v=[];all_t=[];base=0
    for p in placements:
        m=matrix(p['matrix_native']);offset=np.asarray(asset['pivot_offset_native'],float)
        m[:3,3]+=m[:3,:3]@offset+ORIGIN
        v=vertices@m[:3,:3].T+m[:3,3];all_v.append(v);all_t.append(triangles+base);base+=len(v)
    v=np.concatenate(all_v);t=np.concatenate(all_t)
    return ground_fields(project,v,t)

def ground_fields(project,v,t):
    heights,mask,lo,step=raster_height(v,t)
    (project/'ground/showcase.height').write_bytes(struct.pack('<8sIIdddd',b'VWHGT1\0\0',heights.shape[1],heights.shape[0],*lo,*step)+np.ascontiguousarray(heights-.05,dtype='<f4').tobytes())
    # The alpha mask preserves actual projected source coverage, including any
    # authored cut-outs; the shader generator only uses the solid inner extent.
    from PIL import Image
    Image.fromarray((mask*255).astype('uint8')).save(project/'ground/city-ground-coverage.png')
    (project/'ground/city.coverage').write_bytes(struct.pack('<8sIIdddd',b'VWHGT1\0\0',mask.shape[1],mask.shape[0],*lo,*step)+np.ascontiguousarray(mask,dtype='<f4').tobytes())
    shader=project/'shaders/world_terrain.fs.glsl';text=shader.read_text()
    if '// SOURCE_CITY_GROUND_MASK' not in text:
        text=text.replace('uniform bool uForestShelterEnabled,uForestGroundReady;', '// SOURCE_CITY_GROUND_MASK: exact projected source mesh coverage.\nuniform sampler2DArray uCityGroundCoverage;\nuniform vec4 uCityGroundBounds;\nuniform bool uCityGroundMaskReady=false;\nuniform bool uForestShelterEnabled,uForestGroundReady;',1)
        text=text.replace('    float distance_to_eye = length(vRelative);', '    vec2 city_uv=(vPos.xz-uCityGroundBounds.xy)*uCityGroundBounds.zw;\n    if(uCityGroundMaskReady && all(greaterThanEqual(city_uv,vec2(0))) && all(lessThanEqual(city_uv,vec2(1))) && texture(uCityGroundCoverage,vec3((city_uv*(vec2(textureSize(uCityGroundCoverage,0).xy)-1.)+.5)/vec2(textureSize(uCityGroundCoverage,0).xy),0)).r>.5)discard;\n    float distance_to_eye = length(vRelative);',1)
        shader.write_text(text)
    return dict(bounds=[float(lo[0]),float(lo[1]),float(lo[0]+step[0]*(mask.shape[1]-1)),float(lo[1]+step[1]*(mask.shape[0]-1))],grid=list(heights.shape),coverage=float(mask.mean()),height_range=[float(heights.min()),float(heights.max())])

def physics(project,assets,placements):
    # Group exact UCX triangles into bounded spatial mesh bodies. This keeps
    # arch openings rather than substituting an enclosing house/gate AABB.
    groups={};tri_count=vert_count=0
    for p in placements:
        asset=assets[p['asset']]
        if asset['name']=='SM_Ground_Mesh':continue
        collision=asset.get('collision')
        if isinstance(collision,str):collision=json.loads(Path(collision).read_text())
        if not collision:continue
        v=np.asarray(collision['vertices'],float);t=np.asarray(collision['triangles'],np.int64)
        m=matrix(p['matrix_native']);m[:3,3]+=m[:3,:3]@np.asarray(asset['pivot_offset_native'],float)+ORIGIN
        # Publisher UCX and visual GLB share the same grounded local datum.
        v=v@m[:3,:3].T+m[:3,3]
        key=tuple(np.floor(m[[0,2],3]/32).astype(int));g=groups.setdefault(key,[[],[],0])
        g[0].append(v);g[1].append(t+g[2]);g[2]+=len(v);tri_count+=len(t);vert_count+=len(v)
    if vert_count>65536 or tri_count>131072 or len(groups)>512:raise ValueError(f'Source UCX exceeds native collision budget: {vert_count} vertices/{tri_count} triangles/{len(groups)} bodies')
    rows=['VALWOOD_PHYSICS 1','gravity 0 -9.81 0']+[f'layer {i} 255' for i in range(8)]
    for i,(key,g) in enumerate(sorted(groups.items()),1):
        v=np.concatenate(g[0]);t=np.concatenate(g[1]);anchor=v.mean(axis=0);v-=anchor
        rows.append(f'body {i} "Source UCX {key[0]} {key[1]}" 3 0 0 0 '+ ' '.join(format(x,'.12g')for x in anchor)+' 0 0 0 1 .5 .5 .5 .6 .05 1000 0 1 0 0 0 0 0 0 1')
        rows.extend(f'vertex {i} '+ ' '.join(format(x,'.12g')for x in point)for point in v)
        rows.extend(f'triangle {i} '+ ' '.join(str(int(x))for x in tri)for tri in t)
    if groups:(project/'city.physics').write_text('\n'.join(rows)+'\n')
    else:(project/'city.physics').unlink(missing_ok=True)
    return dict(bodies=len(groups),vertices=vert_count,triangles=tri_count,method='exact source UCX, not visual AABBs',walking_route_verified=False)

def solar():
    p,y=map(math.radians,[-32.754127502441406,115.20697784423828]);sun=np.array([-math.cos(p)*math.cos(y),-math.sin(p),math.cos(p)*math.sin(y)])
    # Solve native normalized arc elevation on its afternoon branch.
    lo,hi=12.,18.
    for _ in range(80):
        h=(lo+hi)/2;a=(h-6)/12*math.pi;n=np.array([.65*math.cos(a),math.sin(a),.35]);e=n[1]/np.linalg.norm(n)
        if e>sun[1]:lo=h
        else:hi=h
    h=(lo+hi)/2;a=(h-6)/12*math.pi
    bearing=math.degrees(math.atan2(sun[2],sun[0])-math.atan2(.35,.65*math.cos(a)))
    return h,(bearing+180)%360-180

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--catalog',type=Path,default=ROOT/'assets/purchased/medieval_village/publisher-source/catalog.json');parser.add_argument('--project',type=Path,default=DEFAULT);parser.add_argument('--prepare-only',action='store_true');parser.add_argument('--link-immutable',action='store_true',help='Hardlink frozen source GLBs into managed revisions on the same filesystem');parser.add_argument('--geometry-only',action='store_true',help='Update installed source assets/placements/ground/UCX with backup; preserve shaders, look, sky, spawn, cameras and routes');args=parser.parse_args()
    project=args.project.resolve()
    if args.geometry_only:
        if args.prepare_only:parser.error('--geometry-only cannot be combined with --prepare-only')
        if not (project/'city_showcase.json').is_file():raise ValueError('Geometry-only update requires an installed city project')
        for name in ['models/imports','ground','scenery','shaders','qa']:
            if not (project/name).is_dir()or(project/name).is_symlink():raise ValueError('Geometry-only target must own '+name)
    else:skeleton(project)
    if args.prepare_only:print('Prepared private city folders:',project);return
    data=json.loads(args.catalog.read_text());assets={a['name']:a for a in data['assets']};placements=data['placements']
    used={p['asset']for p in placements}
    if not used.issubset(assets):raise ValueError('Source placement references an undeclared asset')
    if not 1<=len(assets)<=128:raise ValueError('Source city requires 1..128 asset definitions')
    aliases=geometry_aliases(project,assets,args.geometry_only)
    # Reject source placement errors before copying any model revisions or
    # replacing scene files; source GLB validation alone cannot detect these.
    for p in placements:
        _,_,_,scale,_=decompose(matrix(p['matrix_native']))
        alias=aliases[p['asset']];vegetation=assets[p['asset']].get('native',{}).get('vegetation',0)
        limit=2.5 if alias=='broadleaf'else(2 if vegetation==2 else 4)
        if scale>limit:raise ValueError(f'{p["asset"]} scale{scale:.12g} exceeds native limit{limit}')
    backup=backup_geometry_state(project)if args.geometry_only else None
    stems={};definitions=[];records=[]
    for index,name in enumerate(sorted(assets)):
        a=assets[name];files=[Path(p)for p in a['files']]
        if len(files)!=2 or not all(p.is_file()for p in files):raise ValueError('Missing source near/MID pair: '+name)
        alias=aliases[name]
        stem=cook(project,alias,(files[0],files[1],None),1.,immutable_links=args.link_immutable)
        definitions.append(asset_definition(alias,stem,a));stems[name]=alias
    for p in placements:
        a=assets[p['asset']];m=matrix(p['matrix_native']);m[:3,3]+=m[:3,:3]@np.asarray(a['pivot_offset_native'],float)+ORIGIN
        yaw,pitch,roll,scale,extra=decompose(m);x,y,z=m[:3,3];u,v=x/100800+.5,z/(100800*2818/5009)+.5
        if not 0<u<1 or not 0<v<1 or y<.1:raise ValueError('Source placement outside native dry world bounds')
        records.append(f'{stems[p["asset"]]} {u:.17g} {v:.17g} {yaw:.12g} {scale:.12g} 0 0 0 {pitch:.12g} {roll:.12g} '+ ' '.join(format(float(s),'.12g')for s in extra)+f' {y:.12g}')
    if len(records)>8192:raise ValueError('Source city exceeds native instance budget')
    for i,name in enumerate(FILES):(project/'scenery'/f'{name}_instances.txt').write_text('# Exact publisher city transforms.\n'+'\n'.join(records[i::len(FILES)])+'\n')
    ground_name=data['ground_asset'];ground=source_ground(project,assets[ground_name],[p for p in placements if p['asset']==ground_name]);collision=physics(project,assets,placements)
    views=[]
    for index,c in enumerate(data.get('cameras',[]),1):
        xyz=np.asarray(c['position_native'])+ORIGIN
        name='city-town' if index==1 else f'city-camera-{index:02d}'
        views.append(name+' '+' '.join(format(float(x),'.12g')for x in [*xyz,c['yaw_radians'],c['pitch_radians'],c.get('fov_y_radians',math.radians(60))]))
    if not args.geometry_only:(project/'showcase.views').write_text('\n'.join(views)+'\n')
    walk_camera=data.get('cameras',[])[-1] if data.get('cameras') else {}
    spawn=np.asarray(data.get('spawn_native',[12,0,-5]))+ORIGIN;spawn_yaw=float(data.get('spawn_yaw_radians',walk_camera.get('yaw_radians',0)))
    hour,bearing=solar();sun_strength=2.;source_sun_gain=4/(1.62*(1-.72*.1)*sun_strength)
    if args.geometry_only:
        studio=project/'world.studio';studio.write_text(replace_asset_definitions(studio.read_text(),definitions))
    else:
        (project/'world.studio').write_text('VALWOOD_STUDIO 7\n'+f'look 0 {sun_strength:.12g} 1 1 {hour:.12g} {bearing:.12g} .1 0 .3 .08\n'+'\n'.join(definitions)+'\nentities "VALWOOD_ENTITIES 4 1 0"\nsurfaces "VALWOOD_SURFACES 2"\n')
        (project/'project.valwood').write_text('name=Legends of Valwood | Publisher Medieval City\nterrain=valwood_atlas\n'+f'spawn={spawn[0]:.12g} {spawn[2]:.12g} {spawn_yaw:.12g}\ncamera=0 0 7\nday_fraction=0.6\n')
    # Source warm sun is sRGB FColor 255,188,142 decoded into linear RGB.
    decode=lambda n:(n/255/12.92 if n/255<=.04045 else((n/255+.055)/1.055)**2.4)
    if not args.geometry_only:(project/'city.scene').write_text('VALWOOD_CITY_LOOK 1\nsun '+' '.join(format(decode(n)*source_sun_gain,'.12g')for n in [255,188,142])+'\nsky .19 .245 .325\nground .066 .070 .047\nfog .363 .430 .505\nzenith .04 .116 .245\nhaze .00015\n')
    metadata=dict(version=1,name='Publisher Medieval Village',active_geometry_catalog=str(args.catalog.resolve()),catalog_sha256=hashlib.sha256(args.catalog.read_bytes()).hexdigest(),origin=ORIGIN.tolist(),assets=len(assets),placed_assets=len(used),instances=len(records),source_geometry_and_paint=True,ground=ground,collision=collision,saves='Private city saves; original main project unchanged',gameplay='Authored environment review and walking; old main quest coordinates are not relocated')
    metadata['asset_aliases']=aliases
    if backup:metadata['previous_geometry_backup']=str(backup)
    for name in ['publisher_showcase.json','city_showcase.json']:(project/name).write_text(json.dumps(metadata,indent=2)+'\n')
    preflight_scenery_scales(project)
    print(json.dumps(metadata,indent=2))
if __name__=='__main__':main()
