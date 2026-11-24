#!/usr/bin/env python3
"""Cook the publisher-authored castle scene into bounded native source chunks.

No project, world document, existing imports, saves, or preferences are changed.
Unity geometry stays in its authored world space until each chunk is recentered;
its catalog origin remains Unity-space, and GLB geometry mirrorsZ for Valwood.
"""
from pathlib import Path
import argparse,collections,functools,hashlib,json,math,re,struct,copy
import numpy as np
from bake_nature_material_fidelity import FidelityBaker,emit_native_asset,world_xz_planar_vertices
from build_cinematic_diversity import Writer
from repair_nature_cutout_alpha import repair_material_alpha
from align_nature_showcase_lods import align

ROOT=Path(__file__).resolve().parents[1]
PRIVATE=ROOT.parent/'.local-tools/unity-source'
DEFAULT_SOURCE=PRIVATE/'showcase-native/scene.json'
DEFAULT_OUTPUT=ROOT/'assets/purchased/nature_castle/showcase'
GEOLOGY={'Spawned Rocks','Rocks Walls','Rocks','Obsidian Rocks'}
DEFAULT_EXCLUDED={'Lava Rivers and Lakes','Emission'}
ROOF='Assets/NatureManufacture Assets/Castle and Dungeon/Architecture/Roofs/Materials/M_Roof_Tiles_01_Ridge_Shape_Snow.mat'
WOOD='Assets/NatureManufacture Assets/Castle and Dungeon/Architecture/Doors/Materials/M_Wood_door_planks_01.mat'
REMAP=[0,1,2,3,4,5,6,7,10,11,12,13,14,15]
PLANAR_PERIOD=6

def atomic_json(path,value):
    temporary=path.with_suffix(path.suffix+'.tmp')
    temporary.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n');temporary.replace(path)

class Source:
    def __init__(self,path):
        self.path=path;self.doc=json.loads(path.read_text());self.meshes={m['hash']:m for m in self.doc['meshes']}
        self.repairs=collections.Counter()
    @functools.lru_cache(maxsize=None)
    def mesh(self,key):
        m=self.meshes[key];raw=(self.path.parent/m['binary']).read_bytes()
        v16=np.ndarray((m['vertices'],16),'<f4',raw)
        v=np.ascontiguousarray(v16[:,REMAP],dtype='<f4')
        # Source RAM lake meshes have6 nonfinite secondaryUV values. Positions/normals stay exact.
        for col in range(6,10):
            invalid=~np.isfinite(v[:,col])
            if invalid.any():v[invalid,col]=v[invalid,6+(col%2)]if col>=8 else 0
        indices=[np.ndarray((s['indices'],),'<i4',raw,s['index_offset']).copy()for s in m['submeshes']]
        for i in indices:
            if not np.isfinite(v).all()or np.any(i<0)or np.any(i>=len(v)):raise ValueError('Invalid source mesh '+key)
        return v,indices
    @functools.lru_cache(maxsize=None)
    def costs(self,key):
        v,indices=self.mesh(key)
        return sum(len(i)//3 for i in indices),sum(len(np.unique(i))for i in indices)
    def material(self,p,sub):
        mats=p['materials'];path=mats[sub]if sub<len(mats)else''
        if path:return path
        name=p['name'].strip().lower()
        if name.startswith('roof_ridge'):self.repairs['Unassigned source roof ridge -> purchased matching ridge material']+=1;return ROOF
        if name.startswith('wood_planks'):self.repairs['Unassigned source timber -> purchased matching timber material']+=1;return WOOD
        raise ValueError('Unassigned material has no verified repair: '+p['hierarchy']+' slot '+str(sub))

def logical_units(source,args):
    slots=collections.defaultdict(list)
    for p in source.doc['placements']:
        if p['group']in DEFAULT_EXCLUDED|set(args.exclude_group):continue
        if p['materials']and all('M_Lava_River_'in path for path in p['materials']):continue
        if args.group and p['group']not in args.group:continue
        key=re.sub(r'(?i)LOD[ _]?[0-9]+','LOD',p['hierarchy'])
        # Unity allows identical siblingnames; worldmatrix distinguishes separate authored copies.
        identity=tuple(round(float(v),5)for v in p['matrix'])
        slots[(p['group'],key,identity)].append(p)
    units=[];promotions=0
    for key,ps in sorted(slots.items()):
        masks=0
        for p in ps:masks|=p['lod_mask']
        if masks==2 and key[0]=='Background Mountains':
            ps=[dict(p,lod_mask=3)for p in ps];promotions+=len(ps)
        representative=next((p for p in ps if p['lod_mask']&1),ps[0])
        anchor=np.asarray(representative['matrix'],float).reshape(4,4,order='F')[:3,3]
        cost=[0,0]
        for lod in[0,1]:
            for p in ps:
                if p['lod_mask']&(1<<lod):
                    t,v=source.costs(p['mesh']);cost[0]=max(cost[0],t);cost[1]=max(cost[1],v)
        # Additive per-level costs enforce native 1M-triangle/vertex GLB budgets.
        cost=[max(sum(source.costs(p['mesh'])[axis]for p in ps if p['lod_mask']&(1<<lod))for lod in[0,1])for axis in[0,1]]
        units.append({'key':key,'placements':ps,'anchor':anchor,'cost':cost})
    return units,promotions

def partition(units,args,material_results=None):
    initial=collections.defaultdict(list)
    for index,u in enumerate(units):
        group=u['key'][0];anchor=u['anchor']
        if group=='Background Mountains':key=('backdrop',index,0)
        else:
            category='geology'if group in GEOLOGY else'built';size=args.rock_cell if category=='geology'else args.arch_cell
            key=(category,math.floor(anchor[0]/size),math.floor(anchor[2]/size))
        initial[key].append(u)
    result=[]
    def split(name,items):
        costs=np.sum([u['cost']for u in items],axis=0)
        paths={source.material(p,s)for u in items for p in u['placements']for s in range(len(source.meshes[p['mesh']]['submeshes']))}
        encoded=0
        if material_results is not None:
            unique_images={}
            for path in paths:
                for image in material_results[path].get('paths',{}).values():
                    data=Path(image).read_bytes();unique_images[hashlib.sha256(data).digest()]=len(data)
            encoded=int(costs[1])*getattr(args,'geometry_stride',32)+int(costs[0])*12+sum(unique_images.values())
        if costs[0]<=args.max_triangles and costs[1]<=args.max_vertices and len(paths)<=38 and encoded<123*1024**2:
            result.append((name,items));return
        if len(items)==1:raise ValueError('A single logical source component exceeds native budget: '+name+' '+str(costs))
        anchors=np.array([u['anchor']for u in items]);spread=np.ptp(anchors,axis=0);axis=int(np.argmax(spread));ordered=sorted(items,key=lambda u:float(u['anchor'][axis]));half=len(ordered)//2
        split(name+'_a',ordered[:half]);split(name+'_b',ordered[half:])
    for (category,x,z),items in sorted(initial.items()):split('nms_'+category+'_'+str(x).replace('-','m')+'_'+str(z).replace('-','m'),items)
    if len(result)>args.max_chunks:raise ValueError(f'{len(result)} chunks exceed {args.max_chunks}; increase rock-cell or native triangle budget')
    return result

def local_material_parts(source,units):
    parts=collections.defaultdict(list);seen=set()
    for u in units:
        for p in u['placements']:
            if not p['lod_mask']&1:continue
            v,indices=source.mesh(p['mesh'])
            for sub,i in enumerate(indices):
                path=source.material(p,sub);key=(p['mesh'],sub,path)
                if key not in seen:parts[path].append((v,i));seen.add(key)
    return parts

def transformed_parts(source,units,material_results,omitted):
    parts=[]
    for u in units:
        for p in u['placements']:
            v,indices=source.mesh(p['mesh']);matrix=np.asarray(p['matrix'],np.float64).reshape(4,4,order='F')
            transformed=v.copy();transformed[:,:3]=v[:,:3]@matrix[:3,:3].T+matrix[:3,3]
            transformed[:,3:6]=v[:,3:6]@np.linalg.inv(matrix[:3,:3]);transformed[:,3:6]/=np.maximum(np.linalg.norm(transformed[:,3:6],axis=1,keepdims=True),1e-10)
            for sub,i in enumerate(indices):
                path=source.material(p,sub)
                if material_results[path]['status']!='candidate':omitted[path]+=len(i)//3;continue
                vertex=transformed
                if material_results[path].get('planar_period'):vertex=world_xz_planar_vertices(transformed,material_results[path]['planar_period'])
                for lod in[0,1]:
                    if p['lod_mask']&(1<<lod):parts.append(({'name':p['name'],'lod':lod,'material':path},vertex,i))
    if parts and not any(meta['lod']==0 for meta,v,i in parts):
        parts=[(dict(meta,lod=0),v,i)for meta,v,i in parts]+parts
    if parts and not any(meta['lod']==1 for meta,v,i in parts):
        parts=parts+[(dict(meta,lod=1),v,i)for meta,v,i in parts]
    return parts

def install_deduplicated_writer(results):
    by_name={info['name']:info for info in results.values()}
    original=Writer.save
    def save(self,path,embedded=False):
        if not embedded:return original(self,path,embedded)
        for material in self.g['materials']:
            info=by_name.get(material.get('name'),{})
            if info.get('alphaMode'):
                material['alphaMode']=info['alphaMode'];material['alphaCutoff']=info['alphaCutoff']
        cache={}
        for image in self.g['images']:
            payload=Path(image.pop('uri')).read_bytes();digest=hashlib.sha256(payload).digest()
            if digest not in cache:
                offset=len(self.bin);self.bin.extend(payload);self.bin.extend(b'\0'*(-len(self.bin)%4))
                cache[digest]=len(self.g['bufferViews']);self.g['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(payload)})
            image['bufferView']=cache[digest]
        self.g['buffers']=[{'byteLength':len(self.bin)}]
        encoded=json.dumps(self.g,separators=(',',':')).encode();encoded+=b' '*(-len(encoded)%4)
        Path(path).write_bytes(struct.pack('<III',0x46546c67,2,12+8+len(encoded)+8+len(self.bin))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(self.bin),0x004e4942)+self.bin)
    Writer.save=save

def main():
    global source
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=DEFAULT_SOURCE);parser.add_argument('--output',type=Path,default=DEFAULT_OUTPUT)
    parser.add_argument('--size',type=int,default=1024);parser.add_argument('--arm-size',type=int,default=512)
    parser.add_argument('--arch-cell',type=int,default=64);parser.add_argument('--rock-cell',type=int,default=128)
    parser.add_argument('--max-chunks',type=int,default=120);parser.add_argument('--max-triangles',type=int,default=850000);parser.add_argument('--max-vertices',type=int,default=850000)
    parser.add_argument('--exclude-group',action='append',default=[]);parser.add_argument('--group',action='append',default=[])
    parser.add_argument('--reuse-material-cache',action='store_true',help='Explicitly reuse completed candidate PNGs from this unchanged source/texture tier');parser.add_argument('--prepare-only',action='store_true');parser.add_argument('--allow-unsupported',action='store_true');parser.add_argument('--winter',action='store_true')
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True);source=Source(args.source)
    units,promotions=logical_units(source,args);chunks=partition(units,args)
    plan={'source':str(args.source),'source_sha256':hashlib.sha256(args.source.read_bytes()).hexdigest(),'source_groups':dict(collections.Counter(p['group']for u in units for p in u['placements'])),'logical_units':len(units),'chunks':len(chunks),'mid_only_renderers_promoted':promotions,'architecture_cell':args.arch_cell,'geology_cell':args.rock_cell,'native_triangle_budget':args.max_triangles,'native_vertex_budget':args.max_vertices,'omitted_groups':sorted(DEFAULT_EXCLUDED|set(args.exclude_group)),'chunk_plan':[{'name':n,'logical_units':len(us),'placements':sum(len(u['placements'])for u in us),'source_groups':dict(collections.Counter(u['key'][0]for u in us)),'budget_cost':np.sum([u['cost']for u in us],axis=0).tolist()}for n,us in chunks]}
    atomic_json(args.output/'plan.json',plan);print('SHOWCASE PLAN',len(chunks),'chunks /',len(units),'source components; mid-only promoted',promotions,flush=True)
    if args.prepare_only:return
    material_folder=args.output/'materials';material_folder.mkdir(exist_ok=True)
    material_parts=local_material_parts(source,units);baker=FidelityBaker(material_parts,material_folder,args.size,args.arm_size)
    report_path=args.output/'materials.json';reports=json.loads(report_path.read_text())if report_path.exists()else{}
    batch_signature=hashlib.sha256(Path(__file__).read_bytes()+(ROOT/'tools/bake_nature_material_fidelity.py').read_bytes()+(ROOT/'tools/nature_fidelity_raster.cpp').read_bytes()+str((args.size,args.arm_size,plan['source_sha256'])).encode()).hexdigest()
    results={}
    for path in sorted(material_parts):
        cached=reports.get(path,{})
        if (cached.get('batch_signature')==batch_signature or args.reuse_material_cache) and cached.get('status')=='candidate'and cached.get('size')==args.size and cached.get('arm_size')==args.arm_size and all(Path(v).exists()for v in cached['paths'].values()):info=cached
        else:
            info=baker.make(path,planar_period=PLANAR_PERIOD if ('M_Marble_path_snow'in path or'M_Marble_path_sand_black'in path) else None);info['batch_signature']=batch_signature;reports[path]=info;atomic_json(report_path,reports)
        info=repair_material_alpha(path,info)
        results[path]=info;print('SHOWCASE MATERIAL',info['name'],info['status'],flush=True)
    unsupported={k:v['status']for k,v in results.items()if v['status']!='candidate'}
    if unsupported and not args.allow_unsupported:raise ValueError('Source materials require explicit support before geometry export: '+json.dumps(unsupported))
    chunks=partition(units,args,results);plan['chunks']=len(chunks);atomic_json(args.output/'plan.json',plan)
    install_deduplicated_writer(results)
    catalog=[];omitted=collections.Counter();catalog_path=args.output/'catalog.json'
    for index,(name,us)in enumerate(chunks):
        parts=transformed_parts(source,us,results,omitted)
        if not parts:continue
        record=emit_native_asset({'name':name},parts,results,args.output/'chunks',winter=args.winter)
        record.update({'source_groups':dict(collections.Counter(u['key'][0]for u in us)),'source_placements':sum(len(u['placements'])for u in us),'unity_origin':record['origin'],'engine_local_anchor':[record['origin'][0]-500,record['origin'][1],500-record['origin'][2]],'world_bounds_min':[min(float(v[:,a].min())for meta,v,i in parts if meta['lod']==0)for a in range(3)],'world_bounds_max':[max(float(v[:,a].max())for meta,v,i in parts if meta['lod']==0)for a in range(3)]})
        align(record)
        catalog.append(record);atomic_json(catalog_path,catalog);print('SHOWCASE CHUNK',index+1,'/',len(chunks),name,record['triangles'],flush=True)
    report={**plan,'catalog':str(catalog_path),'cooked_chunks':len(catalog),'triangles':[sum(r['triangles'][lod]for r in catalog)for lod in[0,1]],'material_count':len(results),'unsupported_materials':unsupported,'omitted_triangle_instances_by_material':dict(omitted),'source_assignment_repairs':dict(source.repairs),'native_winter_cover':args.winter,'limitations':['Source instance transforms preserved; restored original FBX instead of per-instance painted mesh variants','Unity baked lightmaps and realtime reflection/sky pipeline require native lighting adaptation','Lava water simulation and dedicated emission geometry excluded explicitly; all castle/rock/terrain source geometry retained'],'batch_signature':batch_signature}
    atomic_json(args.output/'report.json',report);print(json.dumps({k:report[k]for k in['cooked_chunks','triangles','material_count','catalog','unsupported_materials']},indent=2),flush=True)
if __name__=='__main__':main()
