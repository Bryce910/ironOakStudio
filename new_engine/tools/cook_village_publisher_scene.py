#!/usr/bin/env python3
"""Cook the owned publisher layout, exact FBX meshes and component paint.

CPU/data only. Sources stay immutable; output is an isolated asset candidate.
All placements use the source pivot, with a recorded native loader Y correction.
Small dressing is spatially combined to respect the 128-definition native limit.
"""
from pathlib import Path
import argparse,collections,copy,hashlib,json,math,re,struct,sys
import numpy as np
from scipy.spatial import cKDTree
from build_cinematic_diversity import Writer

ROOT=Path(__file__).resolve().parents[1]
PRIVATE=ROOT.parent/'.local-tools/village-source'
OUTPUT=ROOT/'assets/purchased/medieval_village/publisher-source'
C=np.array([[1.,0,0],[0,0,1],[0,-1,0]])
CELL_SIZE=12.0
BASE_ALIASES={
 'SM_Boundary_Gate_01':'SM_Castle_Boundary_Gate_01','SM_Boundary_Gate_02':'SM_Castle_Boundary_Gate_02',
 'SM_Boundary_Wall_01':'SM_Castle_Boundary_Wall_01','SM_Boundary_Wall_02':'SM_Castle_Boundary_Wall_02',
 'SM_Boundary_Wall_Corner':'SM_Castle_Boundary_Wall_Corner',
 'SM_Fruit_Wicker_01':'SM_Fruit_Wicker_Basket_01','SM_Fruit_Wicker_02':'SM_Fruit_Wicker_Basket_02','SM_Mix_Debris':'Mix_Debris'}
EXCLUDED={'SM_Sky_Dome':'Source sky is delivered as the recovered HDR environment, not an opaque sphere.',
          'Plane':'Publisher fog-card planes require a fog shader; source transforms remain recorded.'}
KEEP_PREFIX=('SM_House_','SM_Boundary_','SM_Shop_','SM_King_Statue','SM_Street_Lamp','SM_Ground_Mesh',
             'SM_Wooden_Bed','SM_Wooden_Cart','SM_Wooden_Stairs_','SM_Wooden_Plough')

def native_matrix(source):
    m=np.asarray(source,dtype=np.float64).reshape(4,4).T
    n=np.eye(4);n[:3,:3]=C@m[:3,:3]@C.T;n[:3,3]=C@m[:3,3]/100
    return n

class Source:
    def __init__(self):
        self.folder=PRIVATE/'fbx-raw';self.raw=json.loads((self.folder/'meshes.json').read_text())
        self.groups=collections.defaultdict(dict);self.collision=collections.defaultdict(list)
        for row in self.raw['meshes']:
            if row['name'].startswith('UCX_'):
                self.collision[Path(row['source_file']).stem].append(row);continue
            match=re.search(r'_LOD(\d+)$',row['name']);name=re.sub(r'_LOD\d+$','',row['name']);lod=int(match[1])if match else 0
            if lod in self.groups[name]:raise ValueError(('Duplicate source family/tier',name,lod))
            self.groups[name][lod]=row
        self.aliases=dict(BASE_ALIASES);self.slot_maps={};self.lod_slot_maps={};self.alias_evidence={}
        aliases=PRIVATE/'source-aliases.json'
        if aliases.exists():
            doc=json.loads(aliases.read_text())
            self.alias_evidence=doc.get('aliases',doc)
            for key,value in doc.get('aliases',doc).items():
                self.aliases[key]=value['fbx_mesh_name']
                self.slot_maps[key]={int(k):v for k,v in value['fbx_slot_to_ue_slot'].items()}
                self.lod_slot_maps[key]={int(l):{int(k):v for k,v in slots.items()}for l,slots in value.get('fbx_lod_slot_to_ue_slot',{}).items()}
        self.registry=json.loads((PRIVATE/'material-registry.json').read_text())
        self.material_names=set(self.registry['material_instances'])|set(self.registry['master_materials'])
        self.paint=json.loads((PRIVATE/'component-paint/manifest.json').read_text())
        self.cache={};self.paint_cache={};self.binding=[];self.derived={}
    def name(self,family):return self.aliases.get(family,family)
    def row(self,family,lod):
        group=self.groups.get(self.name(family))
        if not group:raise ValueError(('No exact purchased FBX binding',family))
        return group.get(lod,group[0])
    def geometry(self,family,lod,variant=None):
        if (family,lod)in self.derived:return self.derived[family,lod]
        row=self.row(family,lod);key=(family,row['vertices'],variant)
        if key in self.cache:return self.cache[key]
        v=np.fromfile(self.folder/row['vertices'],dtype='<f4').reshape(-1,18).copy()
        if variant:
            record=self.paint['variants'][variant];data=np.load(record['path']);prefix='lod'+str(lod)+'_'
            if prefix+'rgba'not in data:prefix=''
            colors=data[prefix+'rgba'];positions=data[prefix+'position_unreal_cm'];normals=data[prefix+'normal_unreal'][:,:3]
            # A constant authored RGB override is exact independently of the
            # supplied FBX/UE reduction topology. Alpha is not source-paint RGB.
            if np.all(colors[:,:3]==colors[0,:3]):
                v[:,14:18]=colors[0]/255.
                self.binding.append(dict(family=family,lod=lod,variant=variant,corners=len(v),constant_rgb_exact=True))
                positions=None
            # Eight candidates disambiguate duplicate positions by the original
            # source normal, including UV/material splits sharing a location.
            if positions is not None and (variant,prefix)not in self.paint_cache:
                self.paint_cache[variant,prefix]=cKDTree(positions)
            if positions is not None:
                dist,ix=self.paint_cache[variant,prefix].query(v[:,:3],k=min(8,len(positions)),workers=2)
                if dist.ndim==1:dist=dist[:,None];ix=ix[:,None]
                ndot=np.einsum('nki,ni->nk',normals[ix],v[:,3:6]);score=dist+np.maximum(0,1-ndot)*.1
                choice=np.argmin(score,axis=1);bound=ix[np.arange(len(v)),choice];residual=dist[np.arange(len(v)),choice]
                max_residual=float(residual.max(initial=0));bad=int(np.count_nonzero(residual>.025))
                if bad:
                    if lod==1:
                        # The supplied UE-derived LOD1 paint and ALL FBX LOD1
                        # are different vendor reductions for two house copies.
                        # Retain exact LOD0 surface+paint for their MID instead
                        # of borrowing colors from unrelated nearby faces.
                        fallback=self.geometry(family,0,variant)
                        self.binding.append(dict(family=family,lod=lod,variant=variant,source_lod_fallback=0,
                            reason='Vendor FBX LOD1 differs from map-painted UE LOD1; original painted HIGH retained intact',
                            rejected_max_error_cm=max_residual,rejected_corners=bad))
                        self.cache[key]=fallback;return fallback
                    raise ValueError(('Paint geometry mismatch >.025cm',family,lod,max_residual,bad))
                v[:,14:18]=colors[bound]/255.
                self.binding.append(dict(family=family,lod=lod,variant=variant,corners=len(v),max_position_error_cm=max_residual,
                    min_normal_dot=float(ndot[np.arange(len(v)),choice].min(initial=1))))
        slots=self.registry['meshes'].get(family,{}).get('slots',[])
        parts=[]
        for part in row['parts']:
            effective_lod=int(re.search(r'_LOD(\d+)$',row['name'])[1])if re.search(r'_LOD(\d+)$',row['name'])else 0
            mapping=self.lod_slot_maps.get(family,{}).get(effective_lod,self.slot_maps.get(family,{}))
            slot=mapping.get(part['slot'],part['slot'])
            interface=slots[slot]['MaterialInterface']if slot<len(slots)else part['material']
            if interface not in self.material_names:
                normalized=lambda n:re.sub('[^a-z0-9]','',n.lower().replace('tileable',''))
                candidates=[name for name in self.material_names if normalized(name)==normalized(interface)]
                if len(candidates)!=1:raise ValueError(('Unknown or ambiguous source material alias',family,interface,candidates))
                interface=candidates[0]
            ix=np.fromfile(self.folder/part['indices'],dtype='<u4').reshape(-1,3)
            parts.append(dict(slot=slot,material=interface,source_material=part['material'],indices=ix))
        self.cache[key]=(v,parts,row);return self.cache[key]
    def ucx(self,family):
        row=self.row(family,0);stem=Path(row['source_file']).stem;vertices=[];triangles=[]
        for piece in self.collision.get(stem,[]):
            v=np.fromfile(self.folder/piece['vertices'],dtype='<f4').reshape(-1,18)[:,:3]@C.T/100
            for part in piece['parts']:
                ix=np.fromfile(self.folder/part['indices'],dtype='<u4').reshape(-1,3)
                triangles.extend((ix+len(vertices)).tolist())
            vertices.extend(v.tolist())
        if not vertices:return None
        unique,inverse=np.unique(np.asarray(vertices),axis=0,return_inverse=True)
        triangles=inverse[np.asarray(triangles,dtype=np.uint32)]
        return dict(vertices=unique.tolist(),triangles=triangles.tolist(),source='purchased original UCX meshes; exact-position indexed, no simplification')

def foliage(scene):
    data=Path(scene['source']).read_bytes();result=[];audit=[]
    for component in scene['native_instance_components']:
        if component['class_name']!='FoliageInstancedStaticMeshComponent':continue
        count=component['properties']['NumBuiltInstances'];offset=component['native_tail_offset'];size=component['native_tail_bytes'];tail=data[offset:offset+size]
        candidates=[]
        for k in range(min(128,len(tail)-8)):
            stride,n=struct.unpack_from('<II',tail,k)
            if stride!=64 or n!=count or k+8+count*64>len(tail):continue
            matrices=np.frombuffer(tail,dtype='<f4',count=count*16,offset=k+8).reshape(-1,4,4).transpose(0,2,1).copy()
            if np.isfinite(matrices).all()and np.all(matrices[:,3,:3]==0)and np.all(matrices[:,3,3]==1):candidates.append((k,matrices))
        if len(candidates)!=1:raise ValueError(('Ambiguous foliage matrix buffer',component['export_index'],len(candidates)))
        k,matrices=candidates[0];parent=np.asarray(component['world_matrix_unreal_cm_column_major']).reshape(4,4).T
        for index,matrix in enumerate(matrices):
            world=parent@matrix
            result.append(dict(export_index=str(component['export_index'])+':'+str(index),mesh=component['mesh'],actor_label=component['mesh']['name']+'_'+str(index),
                world_matrix_unreal_cm_column_major=world.T.flatten().tolist(),properties={},foliage=True))
        audit.append(dict(component=component['export_index'],mesh=component['mesh']['name'],instances=count,native_matrix_offset=offset+k+8,
            matrix_stride=64,source_exact=True))
    return result,audit

def recover_ropes(scene,source):
    """Apply the supplied Blueprint's Z-forward Hermite spline deformation."""
    from audit_purchased_village_scene import ScenePackage
    from village_source_registry import SourcePackage
    blueprint=PRIVATE/'source-project/Medieval_Village/Content/Medieval_Village/BluePrint/BP_Rope.uasset'
    if not blueprint.exists():return [],[]
    bp=ScenePackage(blueprint);templates={}
    for e in bp.exports:
        if bp.object(e['cls'])in{'SplineMeshComponent','StaticMeshComponent'}:
            props=bp.props(e['index'])
            if props.get('StaticMesh'):templates[e['name']]=props
    package=SourcePackage(Path(scene['source']));dynamic={}
    for e in package.exports:
        if package.object(e['cls'])=='MaterialInstanceDynamic':dynamic[package.exports.index(e)+1]=package.export_properties(e)
    result=[];audit=[]
    for record in scene['placements']:
        if record.get('mesh')or not record.get('actor_label','').startswith('rope_'):continue
        name=re.sub(r'_\d+$','',record['name'])
        template=templates.get(name)or templates.get(record['name'])
        # Unreal appends an instance suffix after the template object name.
        if template is None:
            template=next((v for k,v in templates.items()if record['name'].startswith(k+'_')),None)
        if template is None:raise ValueError(('No original rope Blueprint template',record['name']))
        r=copy.deepcopy(record);r['mesh']=template['StaticMesh'];r['properties']['OverrideMaterials']=template.get('OverrideMaterials',[])
        materials=record['properties'].get('OverrideMaterials',[]);parameters=[]
        for ref in materials:
            if ref and ref['index']in dynamic:parameters.append(dynamic[ref['index']])
        evidence=dict(component=record['export_index'],blueprint=str(blueprint.resolve()),template=name,source_mesh=r['mesh']['name'],dynamic_parameters=parameters)
        if record['class_name']=='SplineMeshComponent':
            if template.get('ForwardAxis')!='ESplineMeshAxis::Z':raise ValueError('Unsupported source spline forward axis')
            params=record['properties']['SplineParams'];p0=np.asarray(params['StartPos']);p1=np.asarray(params['EndPos']);t0=np.asarray(params['StartTangent']);t1=np.asarray(params['EndTangent'])
            up=np.asarray(record['properties'].get('SplineUpDir',[0,0,1]),dtype=float)
            family='SM_Publisher_RopeSpline_'+str(record['export_index'])
            for lod in[0,1]:
                v,parts,row=source.geometry(r['mesh']['name'],lod);v=v.copy();zmin=v[:,2].min();length=v[:,2].max()-zmin
                def deform(points):
                    a=(points[:,2]-zmin)/length;a2=a*a;a3=a2*a
                    center=(2*a3-3*a2+1)[:,None]*p0+(a3-2*a2+a)[:,None]*t0+(-2*a3+3*a2)[:,None]*p1+(a3-a2)[:,None]*t1
                    direction=(6*a2-6*a)[:,None]*p0+(3*a2-4*a+1)[:,None]*t0+(-6*a2+6*a)[:,None]*p1+(3*a2-2*a)[:,None]*t1
                    direction/=np.maximum(np.linalg.norm(direction,axis=1)[:,None],1e-20)
                    x=np.cross(np.tile(up,(len(a),1)),direction);x/=np.maximum(np.linalg.norm(x,axis=1)[:,None],1e-20);y=np.cross(direction,x)
                    return center+x*points[:,0,None]+y*points[:,1,None]
                original=v[:,:3].astype(float);position=deform(original);jac=[]
                for axis in range(3):
                    delta=np.zeros(3);delta[axis]=.01;jac.append((deform(original+delta)-deform(original-delta))/.02)
                jac=np.stack(jac,axis=2);v[:,:3]=position
                v[:,3:6]=np.einsum('nij,nj->ni',np.linalg.inv(jac).transpose(0,2,1),v[:,3:6]);v[:,3:6]/=np.maximum(np.linalg.norm(v[:,3:6],axis=1)[:,None],1e-20)
                v[:,10:13]=np.einsum('nij,nj->ni',jac,v[:,10:13]);v[:,10:13]/=np.maximum(np.linalg.norm(v[:,10:13],axis=1)[:,None],1e-20)
                generated=dict(row,name=family,vertices='derived:'+family,source_spline=params);source.groups[family][lod]=generated
                source.derived[family,lod]=(v,[dict(p,material='MI_Rope')for p in parts],generated)
            r['mesh']=dict(name=family);evidence['deformation']='Original cubic Hermite positions/tangents; original Z-forward Blueprint template; finite-difference normal Jacobian'
        result.append(r);audit.append(evidence)
    return result,audit

def material_writer(rows):
    w=Writer();lookup={};textures={}
    def tex(path):
        if path is None:return -1
        path=str(Path(path).resolve())
        if path not in textures:
            if not Path(path).is_file():raise FileNotFoundError(path)
            textures[path]=len(w.g['textures']);w.g['images'].append(dict(uri=path,mimeType='image/png'));w.g['textures'].append(dict(source=len(w.g['images'])-1,sampler=0))
        return textures[path]
    for name,row in rows.items():
        pbr=dict(baseColorFactor=row.get('factor',[1,1,1,1]),metallicFactor=row.get('metallic',1),roughnessFactor=row.get('roughness',1))
        if row.get('base'):pbr['baseColorTexture']=dict(index=tex(row['base']))
        if row.get('arm'):pbr['metallicRoughnessTexture']=dict(index=tex(row['arm']))
        mat=dict(name=name,pbrMetallicRoughness=pbr,doubleSided=row.get('doubleSided',False))
        if row.get('arm'):mat['occlusionTexture']=dict(index=tex(row['arm']))
        if row.get('normal'):mat['normalTexture']=dict(index=tex(row['normal']))
        if row.get('alphaMode'):mat['alphaMode']=row['alphaMode']
        if row.get('alphaCutoff')is not None:mat['alphaCutoff']=row['alphaCutoff']
        if row.get('emissive'):mat['emissiveTexture']=dict(index=tex(row['emissive']));mat['emissiveFactor']=row.get('emissive_factor',[1,1,1])
        mode=int(row.get('facade_mode',0))
        if mode:
            aux=[tex(p)for p in row.get('aux',[None]*3)];mat['extensions']={'VALWOOD_materials_facade':dict(parameters=[row.get('facade_version',1),mode,*aux,*row.get('facade_scale',[1,1,1])])}
            w.g.setdefault('extensionsUsed',[])
            if 'VALWOOD_materials_facade'not in w.g['extensionsUsed']:w.g['extensionsUsed'].append('VALWOOD_materials_facade')
        lookup[name]=len(w.g['materials']);w.g['materials'].append(mat)
    return w,lookup

def emit(path,pieces,source,lod,cooker,miny,gi_sampler=None):
    prepared=[];materials={}
    for piece in pieces:
        v,parts,row=source.geometry(piece['family'],lod,piece.get('variant'));m=piece['matrix'];v=v.copy()
        v[:,6:8]*=piece.get('uv_density_scale',1.)
        if gi_sampler is not None:v=np.column_stack((v,gi_sampler(piece,v,row,lod))).astype(np.float32)
        local=v[:,:3]@C.T/100;v[:,:3]=local@m[:3,:3].T+m[:3,3];v[:,1]-=miny
        normal_matrix=np.linalg.inv(m[:3,:3]).T;v[:,3:6]=(v[:,3:6]@C.T)@normal_matrix.T
        v[:,3:6]/=np.maximum(np.linalg.norm(v[:,3:6],axis=1)[:,None],1e-20)
        v[:,10:13]=(v[:,10:13]@C.T)@m[:3,:3].T;v[:,10:13]/=np.maximum(np.linalg.norm(v[:,10:13],axis=1)[:,None],1e-20)
        v[:,13]*=-1 if np.linalg.det(m[:3,:3])>0 else 1
        for part in parts:
            name=part['material'];override=piece.get('override',[])
            if part['slot']<len(override)and override[part['slot']]:name=override[part['slot']]['name']
            info=cooker.get(name);materials[name]=info
            indices=part['indices'].copy();p=v[indices,:3];n=v[indices,3:6].sum(1)
            wrong=np.einsum('ij,ij->i',np.cross(p[:,1]-p[:,0],p[:,2]-p[:,0]),n)<0;indices[wrong]=indices[wrong,::-1]
            prepared.append((v,indices,name,info))
    writer,mapping=material_writer(materials);primitives=[];allpos=[];triangles=0
    # A material is one draw primitive even when hundreds of source props use
    # it. This preserves every original triangle and fits the native128 cap.
    merged=collections.defaultdict(list)
    for v,indices,name,info in prepared:
        used,inverse=np.unique(indices,return_inverse=True);merged[name].append((v[used],inverse.reshape(-1,3).astype(np.uint32)))
    for name,parts in merged.items():
        info=materials[name];arrays=[];faces=[];offset=0
        for a,ix in parts:arrays.append(a);faces.append(ix+offset);offset+=len(a)
        a=np.concatenate(arrays);ix=np.concatenate(faces)
        # One correction establishes source UV semantics; the GLTF/native
        # texture path flips V once, as the established house cooker does.
        uv=a[:,6:8].copy()
        if not info.get('facade_mode'):uv*=info.get('uv_scale',[1,1])
        uv[:,1]=1-uv[:,1]
        attrs={'POSITION':writer.array(a[:,:3],'VEC3',34962),'NORMAL':writer.array(a[:,3:6],'VEC3',34962),
               'TEXCOORD_0':writer.array(uv,'VEC2',34962),'TEXCOORD_1':writer.array(np.column_stack((a[:,8],1-a[:,9])),'VEC2',34962),
               'TANGENT':writer.array(a[:,10:14],'VEC4',34962)}
        if info.get('facade_mode')in{1,3,4}:attrs['_VALWOOD_SOURCE_PAINT']=writer.array(a[:,14:17],'VEC3',34962)
        if a.shape[1]>=22:attrs['_VALWOOD_BAKED_LIGHT']=writer.array(a[:,18:22],'VEC4',34962)
        primitives.append(dict(attributes=attrs,indices=writer.array(ix.reshape(-1,1),'SCALAR',34963),material=mapping[name]));allpos.append(a[:,:3]);triangles+=len(ix)
    # The native loader grounds every GLB at its minimum Y. A shared zero-area
    # datum gives HIGH and MID the same origin without a visible extra face.
    datum=np.zeros((3,3),np.float32);datumattrs={'POSITION':writer.array(datum,'VEC3',34962),'NORMAL':writer.array(np.tile([0,1,0],(3,1)),'VEC3',34962),'TEXCOORD_0':writer.array(np.zeros((3,2)),'VEC2',34962)}
    datum_material=len(writer.g['materials']);writer.g['materials'].append(dict(name='Source_Pivot_Datum',pbrMetallicRoughness=dict(baseColorFactor=[0,0,0,1],metallicFactor=0,roughnessFactor=1)))
    primitives.append(dict(attributes=datumattrs,indices=writer.array(np.arange(3,dtype=np.uint32).reshape(-1,1),'SCALAR',34963),material=datum_material))
    if len(primitives)>128:raise ValueError(('Native primitive cap',path,len(primitives)))
    writer.g['meshes']=[dict(name=path.stem,primitives=primitives)];writer.g['nodes']=[dict(name=path.stem,mesh=0)];writer.g['scenes'][0]['nodes']=[0]
    if len(writer.g['images'])>128:raise ValueError(('Native image cap',path,len(writer.g['images'])))
    path.parent.mkdir(parents=True,exist_ok=True);writer.save(path,embedded=True)
    if path.stat().st_size>128*1024**2:raise ValueError(('Native GLB size cap',path,path.stat().st_size))
    p=np.concatenate(allpos);return dict(triangles=triangles,vertices=sum(len(p)for p in allpos),bytes=path.stat().st_size,bounds=[p.min(0).tolist(),p.max(0).tolist()],sha256=hashlib.sha256(path.read_bytes()).hexdigest())

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output',type=Path,default=OUTPUT);ap.add_argument('--audit-only',action='store_true');ap.add_argument('--only',nargs='*');ap.add_argument('--materials-json',type=Path);ap.add_argument('--include-castle-library',action='store_true');args=ap.parse_args()
    scene=json.loads((PRIVATE/'publisher-scene.json').read_text());source=Source();instances=scene['placements'];fol,fol_audit=foliage(scene);ropes,rope_audit=recover_ropes(scene,source);instances=instances+fol+ropes
    grouped=collections.defaultdict(list);placements=[];excluded=[];missing=[];source_actor_map=[]
    for component in scene['native_instance_components']:
        if component['class_name']!='FoliageInstancedStaticMeshComponent':
            excluded.append(dict(component=component['export_index'],mesh=None,reason='Publisher Blueprint fog card uses a dedicated translucent fog material; original instance data retained in scene audit.'))
    for record in instances:
        mesh=record.get('mesh')
        if not mesh:continue
        family=mesh['name']
        if family in EXCLUDED:excluded.append(dict(component=record['export_index'],mesh=family,reason=EXCLUDED[family]));continue
        if source.name(family)not in source.groups:
            if not args.only or family in args.only:missing.append(family)
            continue
        paint=source.paint['components'].get(str(record['export_index']),{}).get('variant');matrix=native_matrix(record['world_matrix_unreal_cm_column_major'])
        if np.linalg.det(matrix[:3,:3])<=0:raise ValueError(('Mirrored source actor needs explicit variant',record['export_index']))
        overrides=record['properties'].get('OverrideMaterials',[])
        # A shared family can have different material-interface overrides.
        # Include them in the identity rather than silently adopting the first.
        defaults=source.registry['meshes'].get(family,{}).get('slots',[])
        changed=any(o and (i>=len(defaults)or o['name']!=defaults[i]['MaterialInterface'])for i,o in enumerate(overrides))
        key=family+('__paint_'+paint[:10]if paint else '')
        if changed:key+='__mat_'+hashlib.sha256(json.dumps(overrides,sort_keys=True).encode()).hexdigest()[:8]
        piece=dict(family=family,variant=paint,matrix=np.eye(4),override=overrides,source_component=record['export_index'])
        if not record.get('foliage')and family.startswith(KEEP_PREFIX):
            if key not in grouped:grouped[key].append(piece)
            placements.append(dict(asset=key,matrix_native=matrix.T.flatten().tolist(),source_component=record['export_index'],actor_label=record.get('actor_label')))
        else:
            cell=np.floor(matrix[[0,2],3]/CELL_SIZE).astype(int);key=('foliage'if record.get('foliage')else'dressing')+'_'+str(cell[0])+'_'+str(cell[1]);piece['matrix']=matrix;grouped[key].append(piece)
        source_actor_map.append(dict(component=record['export_index'],family=family,asset=key,paint_variant=paint))
    if missing:raise ValueError(('Unresolved source families',sorted(set(missing))))
    castle_library=[]
    if args.include_castle_library:
        for family in ['SM_Castle','SM_Castle_Centre','SM_Castle_Pillar_01','SM_Castle_Pillar_02','SM_Castle_Side_01','SM_Castle_Side_02']:
            grouped[family]=[dict(family=family,variant=None,matrix=np.eye(4),override=[],source_component=None)]
            castle_library.append(family)
    plan=dict(schema=1,coordinate_system='native metres X=UEX,Y=UEZ,Z=-UEY; column-major matrices relative to city origin',assets=len(grouped),source_components=len(source_actor_map),source_actor_map=source_actor_map,
        foliage=fol_audit,ropes=rope_audit,excluded=excluded,spatial_cell_size_m=CELL_SIZE,groups={k:dict(pieces=len(v),families=sorted({p['family']for p in v}))for k,v in grouped.items()})
    args.output.mkdir(parents=True,exist_ok=True);(args.output/'layout-plan.json').write_text(json.dumps(plan,indent=2)+'\n')
    if args.audit_only:print(json.dumps({k:v for k,v in plan.items()if k not in{'source_actor_map','groups','excluded'}},indent=2));return
    if args.materials_json:
        rows=json.loads(args.materials_json.read_text())
        if 'name'in rows:rows={rows['name']:rows}
        class FrozenCooker:
            def get(self,name):
                if name not in rows:raise ValueError(('Missing frozen source material',name))
                return rows[name]
        cooker=FrozenCooker()
    else:
        from cook_village_source_materials import VillageMaterialCooker
        cooker=VillageMaterialCooker()
    assets=[]
    for number,(name,original_pieces)in enumerate(sorted(grouped.items())):
        if args.only and name not in args.only:continue
        pieces=[dict(p,matrix=p['matrix'].copy())for p in original_pieces]
        batch=name.startswith(('dressing_','foliage_'));anchor=np.zeros(3)
        if batch:
            _,cx,cz=name.rsplit('_',2);anchor[[0,2]]=[int(cx)*CELL_SIZE+CELL_SIZE/2,int(cz)*CELL_SIZE+CELL_SIZE/2]
            for piece in pieces:piece['matrix'][:3,3]-=anchor
        miny=float('inf')
        for piece in pieces:
            for lod in[0,1]:
                v,_,_=source.geometry(piece['family'],lod,piece.get('variant'));p=(v[:,:3]@C.T/100)@piece['matrix'][:3,:3].T+piece['matrix'][:3,3];miny=min(miny,float(p[:,1].min()))
        files=[];stats=[]
        for label,lod in[('near',0),('mid',1)]:
            path=args.output/'models'/(name+'_'+label+'.glb');stats.append(emit(path,pieces,source,lod,cooker,miny));files.append(str(path.resolve()))
        collision=source.ucx(pieces[0]['family'])if len(pieces)==1 and name.startswith(KEEP_PREFIX)else None
        if collision:
            cv=np.asarray(collision['vertices']);cv[:,1]-=miny;collision['vertices']=cv.tolist();collision['coordinate_system']='same shifted local coordinates as GLB'
        asset=dict(name=name,files=files,pivot_offset_native=[0,miny,0],bounds=stats[0]['bounds'],collision=collision,stats=stats,
            source_families=sorted({p['family']for p in pieces}),pieces=len(pieces))
        notes=[source.alias_evidence[p['family']]['export_limitation']for p in pieces if source.alias_evidence.get(p['family'],{}).get('export_limitation')]
        if notes:asset['source_export_limitations']=sorted(set(notes))
        if name in castle_library:asset['library_only']=True;asset['provenance']='Purchased ALL-format FBX, absent from supplied UE demo map; deliberately unplaced'
        assets.append(asset)
        if batch:
            placement=np.eye(4);placement[:3,3]=anchor
            placements.append(dict(asset=name,matrix_native=placement.T.flatten().tolist(),spatial_batch=True))
        print(f'[{number+1}/{len(grouped)}] {name}: {stats[0]["triangles"]} / {stats[1]["triangles"]} triangles, pivotY={miny:.5f}',flush=True)
    cameras=[]
    for c in scene['cameras_resolved']:
        cameras.append(dict(name=c['name'],position_native=c['position_native_m'],yaw_radians=c['yaw_native_radians'],pitch_radians=c['pitch_native_radians'],
            fov_y_radians=2*math.atan(math.tan(math.radians(c['horizontal_fov_degrees'])/2)/c['aspect_ratio']),source=c))
    available={a['name']for a in assets};catalog=dict(schema=1,assets=assets,placements=[p for p in placements if p['asset']in available],cameras=cameras,ground_asset='SM_Ground_Mesh',
        source_scene=scene['source'],source_scene_sha256=scene['source_sha256'],coordinate_system=plan['coordinate_system'],source_actor_map=source_actor_map,
        source_alias_evidence=str((PRIVATE/'source-aliases.json').resolve()),
        foliage=fol_audit,ropes=rope_audit,excluded=excluded,castle_library_unplaced=castle_library,paint_binding=source.binding,limitations=['Opaque source sky is provided through recovered HDRI instead of sky-dome geometry.',
        'Source fog-card shader is separately inventoried; no opaque plane substitutes.',
        'Rope dynamic start/end UV phase is recorded; original UV0 and source material tiling are retained in this bounded static cook.'])
    (args.output/'catalog.json').write_text(json.dumps(catalog,indent=2,allow_nan=False)+'\n');print('READY',args.output/'catalog.json',len(assets),'assets',len(catalog['placements']),'placements',flush=True)

if __name__=='__main__':main()
