"""Original-component vegetation LODs from CC0 Poly Haven meshes.

Python --prepare downloads/verifies sources and reduces source leaf components.
Blender --cook decimates wood, exports static GLBs and bakes far views with
Cycles CPU, two threads. Python --finish-far packages cropped upright billboards.
No GPU renderer or generated concept imagery is used.
"""
from pathlib import Path
import copy, hashlib, json, math, struct, sys
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0,str(Path(__file__).resolve().parent))
import build_cinematic_assets as base

TREES=[dict(source='tree_small_02',mesh=0,leaf_material=1,name='cinematic_broadleaf',height=10.,near_leaves=7000,mid_leaves=1000),
       dict(source='pine_sapling_small',mesh=0,leaf_material=1,name='cinematic_pine_young',height=3.2,near_leaves=10000,mid_leaves=1250)]

class GLTF:
    def __init__(self,folder,asset):
        import numpy as np
        self.folder=folder;self.g=json.loads((folder/(asset+'.gltf')).read_text())
        self.binary=(folder/self.g['buffers'][0]['uri']).read_bytes()
    def read(self,index):
        import numpy as np
        a=self.g['accessors'][index];v=self.g['bufferViews'][a['bufferView']]
        dtype={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']]
        n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
        return np.ndarray((a['count'],n),dtype=dtype,buffer=self.binary,
            offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',n*np.dtype(dtype).itemsize),np.dtype(dtype).itemsize)).copy()

class Writer:
    def __init__(self,materials=None,textures=None,images=None):
        self.bin=bytearray();self.g={'asset':{'version':'2.0','generator':'Valwood source-component asset cook'},
            'scene':0,'scenes':[{'nodes':[]}],'nodes':[],'meshes':[],'accessors':[],'bufferViews':[],
            'materials':materials or [],'textures':textures or [],'images':images or [],
            'samplers':[{'magFilter':9729,'minFilter':9987,'wrapS':10497,'wrapT':10497}]}
    def array(self,array,kind,target):
        import numpy as np
        a=np.ascontiguousarray(array,dtype='<u4' if kind=='SCALAR' else '<f4');offset=len(self.bin)
        self.bin.extend(a.tobytes());self.bin.extend(b'\0'*(-len(self.bin)%4))
        v=len(self.g['bufferViews']);self.g['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':a.nbytes,'target':target})
        index=len(self.g['accessors']);entry={'bufferView':v,'componentType':5125 if kind=='SCALAR' else 5126,'count':len(a),'type':kind}
        if kind=='VEC3':entry.update(min=a.min(axis=0).tolist(),max=a.max(axis=0).tolist())
        self.g['accessors'].append(entry);return index
    def mesh(self,name,positions,normals,uv,indices,material):
        primitive={'attributes':{'POSITION':self.array(positions,'VEC3',34962),'NORMAL':self.array(normals,'VEC3',34962),
                   'TEXCOORD_0':self.array(uv,'VEC2',34962)},'indices':self.array(indices.reshape(-1,1),'SCALAR',34963),'material':material}
        index=len(self.g['meshes']);self.g['meshes'].append({'name':name,'primitives':[primitive]})
        node=len(self.g['nodes']);self.g['nodes'].append({'name':name,'mesh':index});self.g['scenes'][0]['nodes'].append(node)
    def save(self,path,embedded=False):
        path=Path(path)
        if not embedded:
            self.g['buffers']=[{'uri':path.with_suffix('.bin').name,'byteLength':len(self.bin)}]
            path.with_suffix('.bin').write_bytes(self.bin);path.write_text(json.dumps(self.g,indent=2));return
        for im in self.g['images']:
            data=Path(im.pop('uri')).read_bytes();offset=len(self.bin);self.bin.extend(data);self.bin.extend(b'\0'*(-len(self.bin)%4))
            im['bufferView']=len(self.g['bufferViews']);self.g['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(data)})
        self.g['buffers']=[{'byteLength':len(self.bin)}]
        j=json.dumps(self.g,separators=(',',':')).encode();j+=b' '*(-len(j)%4)
        path.write_bytes(struct.pack('<III',0x46546c67,2,12+8+len(j)+8+len(self.bin))+struct.pack('<II',len(j),0x4e4f534a)+j+struct.pack('<II',len(self.bin),0x004e4942)+self.bin)

def download(asset):
    folder=base.SOURCE/asset;folder.mkdir(exist_ok=True)
    base.fetch('https://api.polyhaven.com/files/'+asset,folder/'metadata.json')
    meta=json.loads((folder/'metadata.json').read_text());record=meta['gltf']['2k']['gltf']
    base.fetch(record['url'],folder/(asset+'.gltf'),record)
    tasks=[(r['url'],folder/name,r) for name,r in record['include'].items()]
    for key in meta:
        if 'alpha' in key.lower():
            r=meta[key]['2k']['png'];tasks.append((r['url'],folder/('source_'+key+'.png'),r))
    with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(lambda args:base.fetch(*args),tasks))

def runtime_materials(source):
    """Texture tiers bound additional VRAM; channel UV transforms bake below."""
    from PIL import Image
    folder=source.folder;g=source.g;materials=copy.deepcopy(g['materials']);images=[];textures=[];mapping={}
    for mi,mat in enumerate(materials):
        leaf='leav' in mat['name'] or 'twig' in mat['name'] or 'shrub' in mat['name']
        pbr=mat['pbrMetallicRoughness'];channels=[('base',pbr.get('baseColorTexture')),('normal',mat.get('normalTexture')),('arm',pbr.get('metallicRoughnessTexture'))]
        for kind,info in channels:
            if not info:continue
            original=source.g['images'][source.g['textures'][info['index']]['source']]['uri']
            # Two 2K albedos, three 1K normals and 512px material maps are a
            # substantially smaller GPU allocation than nine 2K RGBA images.
            limit=2048 if kind=='base' and (leaf or 'trunk' in mat['name'] or 'bark' in mat['name']) else (512 if kind=='arm' else 1024)
            key=(original,limit,kind,leaf)
            if key not in mapping:
                image=Image.open(folder/original).convert('RGBA' if leaf and kind=='base' else 'RGB')
                if leaf and kind=='base':
                    candidates=list(folder.glob('source_*alpha*.png'))
                    if candidates:image.putalpha(Image.open(candidates[0]).convert('L'))
                image.thumbnail((limit,limit),Image.Resampling.LANCZOS)
                dest=folder/('runtime_'+str(mi)+'_'+kind+'.png');image.save(dest,optimize=True)
                mapping[key]=len(textures);images.append({'uri':str(dest),'mimeType':'image/png'})
                textures.append({'source':len(images)-1,'sampler':0})
            info['index']=mapping[key]
            info.pop('texCoord',None);info.pop('extensions',None)
        mat.pop('extensions',None)
        if leaf:mat.update(alphaMode='MASK',alphaCutoff=.4,doubleSided=True)
    return materials,textures,images

def source_primitive(source,p):
    import numpy as np
    mat=source.g['materials'][p['material']];infos=base.texture_infos(mat)
    tc=infos[0].get('texCoord',0) if infos else 0
    uv=source.read(p['attributes']['TEXCOORD_'+str(tc)])
    transform=(infos[0].get('extensions',{}).get('KHR_texture_transform',{})) if infos else {}
    if transform:
        assert all(i.get('extensions',{}).get('KHR_texture_transform',{})==transform for i in infos)
        uv=uv*np.asarray(transform.get('scale',[1,1]))+np.asarray(transform.get('offset',[0,0]))
    return (source.read(p['attributes']['POSITION']),source.read(p['attributes']['NORMAL']),uv,source.read(p['indices']).reshape(-1,3))

def components(positions,indices):
    import numpy as np
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import connected_components
    start=np.r_[indices[:,0],indices[:,0]];end=np.r_[indices[:,1],indices[:,2]]
    graph=coo_matrix((np.ones(len(start),np.uint8),(start,end)),shape=(len(positions),len(positions))).tocsr()
    count,labels=connected_components(graph,directed=False)
    return count,labels,np.bincount(labels[indices[:,0]],minlength=count)

def prepare_tree(config):
    import numpy as np
    source=GLTF(base.SOURCE/config['source'],config['source'])
    materials,textures,images=runtime_materials(source)
    primitives=source.g['meshes'][config['mesh']]['primitives']
    leaf=next(p for p in primitives if p['material']==config['leaf_material'])
    pos,norm,uv,indices=source_primitive(source,leaf)
    count,labels,triangles=components(pos,indices)
    # The original source has individual leaves/needles, each with its own
    # photographed UV island. Fit each separately, retaining its true location,
    # UV window, size and orientation; no re-created branch sheets or cones.
    order=np.argsort(labels,kind='stable');starts=np.r_[0,np.flatnonzero(np.diff(labels[order]))+1,len(order)]
    leaf_ids=[];quad_p=[];quad_uv=[];quad_n=[];areas=[];centers=[]
    complex_mask=np.zeros(count,bool)
    for a,b in zip(starts[:-1],starts[1:]):
        vi=order[a:b];label=labels[vi[0]]
        if config['source']=='pine_sapling_small' and triangles[label]>20:
            complex_mask[label]=True;continue
        points=pos[vi];tex=uv[vi];low=tex.min(0);high=tex.max(0)
        if np.min(high-low)<1e-6:complex_mask[label]=True;continue
        design=np.column_stack((tex,np.ones(len(tex))));fit=np.linalg.lstsq(design,points,rcond=None)[0]
        corners=np.array([[low[0],high[1]],[high[0],high[1]],[high[0],low[1]],[low[0],low[1]]])
        q=np.column_stack((corners,np.ones(4)))@fit
        n=norm[vi].mean(0);n/=max(np.linalg.norm(n),1e-8)
        area=np.linalg.norm(np.cross(q[1]-q[0],q[3]-q[0]))
        if area<1e-10:continue
        leaf_ids.append(int(label));quad_p.append(q);quad_uv.append(corners);quad_n.append(n);areas.append(area);centers.append(points.mean(0))
    qp=np.array(quad_p);qu=np.array(quad_uv);qn=np.array(quad_n);area=np.array(areas);center=np.array(centers)
    # Area-weighted stable sampling retains the visually dominant real leaves.
    # Spread across the original crown instead of manufacturing large sheets.
    seed=31291 if config['source']=='tree_small_02' else 71921
    rng=np.random.default_rng(seed);importance=area*np.clip(np.linalg.norm(center-center.mean(0),axis=1),.25,10.)**.3
    rank=np.argsort(-np.log(np.maximum(rng.random(len(area)),1e-10))/np.maximum(importance,1e-10))
    for lod,budget in [('near',config['near_leaves']),('mid',config['mid_leaves'])]:
        selected=np.sort(rank[:min(budget,len(rank))]);writer=Writer(copy.deepcopy(materials),copy.deepcopy(textures),copy.deepcopy(images))
        q=qp[selected].copy()
        if lod=='mid':
            # Tiny individual needles need a little width at mid distance, not
            # an enlarged full branch card. Preserve each source shoot length.
            ratio=min(2.2,math.sqrt(area.sum()/max(area[selected].sum(),1e-8)))
            if config['source']=='pine_sapling_small':
                e1=q[:,1]-q[:,0];e2=q[:,3]-q[:,0];short=np.linalg.norm(e1,axis=1)<np.linalg.norm(e2,axis=1)
                for i in range(len(q)):
                    axis=(e1[i] if short[i] else e2[i]);axis/=max(np.linalg.norm(axis),1e-8)
                    delta=q[i]-q[i].mean(0);q[i]+=np.outer(delta@axis,axis)*(ratio-1.)
            else:q=q.mean(axis=1)[:,None,:]+(q-q.mean(axis=1)[:,None,:])*min(1.25,ratio)
        normal=np.repeat(qn[selected,None,:],4,axis=1)
        faces=np.tile(np.array([[0,1,2],[0,2,3]],np.uint32),(len(q),1))+np.repeat(np.arange(len(q))*4,2)[:,None]
        # Match original average normals with the new card's winding.
        geometric=np.cross(q[:,1]-q[:,0],q[:,2]-q[:,0]);flip=(geometric*qn[selected]).sum(1)<0
        for i in np.flatnonzero(flip):faces[2*i:2*i+2]=faces[2*i:2*i+2,::-1]
        writer.mesh('foliage_original_components',q.reshape(-1,3),normal.reshape(-1,3),qu[selected].reshape(-1,2),faces,config['leaf_material'])
        if complex_mask.any():
            ix=indices[complex_mask[labels[indices[:,0]]]];used=np.unique(ix);remap=np.full(len(pos),-1,np.int32);remap[used]=np.arange(len(used))
            writer.mesh('wood_foliage_branches',pos[used],norm[used],uv[used],remap[ix],config['leaf_material'])
        for p in primitives:
            if p==leaf:continue
            x,n,t,ix=source_primitive(source,p);writer.mesh('wood_'+str(p['material']),x,n,t,ix,p['material'])
        output=source.folder/('components_'+lod+'.gltf');writer.save(output)
        record={'method':'individually fitted original leaf/needle components with original atlas UVs',
                'original_components':int(count),'available_leaf_components':len(qp),'selected_leaves':len(selected),
                'lod':lod,'seed':seed,'source_url':'https://polyhaven.com/a/'+config['source'],
                'source_binary_sha256':base.sha(source.folder/(config['source']+'.bin'))}
        (source.folder/('components_'+lod+'.json')).write_text(json.dumps(record,indent=2))
        print('PREPARED',config['name'],lod,len(selected),'original leaf components',flush=True)

def prepare():
    for config in TREES:download(config['source']);prepare_tree(config)
    download('dead_tree_trunk_02')
    source=GLTF(base.SOURCE/'dead_tree_trunk_02','dead_tree_trunk_02')
    materials,textures,images=runtime_materials(source);writer=Writer(materials,textures,images)
    for p in source.g['meshes'][0]['primitives']:
        x,n,t,ix=source_primitive(source,p);writer.mesh('Scanned fallen trunk',x,n,t,ix,p['material'])
    writer.save(source.folder/'deadwood_prepared.gltf');base.provenance()

def cook():
    import bpy
    from mathutils import Vector
    import bmesh
    manifest_path=base.OUT/'cinematic_manifest.json';manifest=json.loads(manifest_path.read_text())
    def clear():bpy.ops.wm.read_factory_settings(use_empty=True)
    def tris(obj):return sum(len(p.vertices)-2 for p in obj.data.polygons)
    def decimate(obj,budget):
        bpy.context.view_layer.objects.active=obj;obj.select_set(True)
        bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.remove_doubles(threshold=.00001);bpy.ops.object.mode_set(mode='OBJECT')
        if tris(obj)>budget:
            m=obj.modifiers.new('Bounded real source wood','DECIMATE');m.ratio=budget/tris(obj);bpy.ops.object.modifier_apply(modifier=m.name)
        obj.select_set(False)
    def major(obj,limit):
        bm=bmesh.new();bm.from_mesh(obj.data);seen=set();groups=[]
        for start in bm.verts:
            if start in seen:continue
            todo=[start];seen.add(start);group=[]
            while todo:
                v=todo.pop();group.append(v)
                for e in v.link_edges:
                    other=e.other_vert(v)
                    if other not in seen:seen.add(other);todo.append(other)
            extent=Vector([max(v.co[i] for v in group)-min(v.co[i] for v in group) for i in range(3)]).length
            groups.append((extent,group))
        groups.sort(key=lambda x:x[0],reverse=True)
        remove=[v for _,g in groups[limit:] for v in g]
        if remove:bmesh.ops.delete(bm,geom=remove,context='VERTS')
        bm.to_mesh(obj.data);bm.free();obj.data.update()
    def bounds():
        pts=[o.matrix_world@v.co for o in bpy.context.scene.objects if o.type=='MESH' for v in o.data.vertices]
        return Vector([min(p[i] for p in pts) for i in range(3)]),Vector([max(p[i] for p in pts) for i in range(3)])
    def normalize(height):
        bpy.context.view_layer.update();lo,hi=bounds();scale=height/(hi.z-lo.z)
        for o in bpy.context.scene.objects:
            if o.type=='MESH':o.location.z-=lo.z;o.location*=scale;o.scale*=scale
        bpy.context.view_layer.update()
    def export(name,source,extra=None):
        for mat in bpy.data.materials:
            mat.use_backface_culling=False
            if mat.node_tree:
                for n in mat.node_tree.nodes:
                    if n.type=='TEX_IMAGE' and n.image:n.image.pack()
        path=base.OUT/(name+'.glb')
        bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',export_yup=True,export_animations=False,export_cameras=False,export_lights=False,export_image_format='AUTO')
        lo,hi=bounds();entry={'asset':name,'source':source,'license':'CC0-1.0','triangles':sum(tris(o) for o in bpy.context.scene.objects if o.type=='MESH'),
                'bounds_xyz_z_up':[[lo[i],hi[i]] for i in range(3)],'bytes':path.stat().st_size,'sha256':base.sha(path),**(extra or {})}
        manifest[:]=[r for r in manifest if r['asset']!=name];manifest.append(entry);manifest_path.write_text(json.dumps(manifest,indent=2));print('COOKED',name,entry['triangles'],flush=True)
        return entry
    def bake(config):
        scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=24
        scene.cycles.use_denoising=False;scene.cycles.max_bounces=3;scene.cycles.transparent_max_bounces=16
        scene.render.threads_mode='FIXED';scene.render.threads=2
        scene.render.resolution_x=768;scene.render.resolution_y=1024;scene.render.resolution_percentage=100
        scene.render.film_transparent=True;scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
        lo,hi=bounds();height=hi.z-lo.z;width=hi.x-lo.x;canvas=max(height*1.08,width*1024/768*1.08)
        world=bpy.data.worlds.new('Soft neutral sky');world.use_nodes=True
        world.node_tree.nodes['Background'].inputs[0].default_value=(.65,.72,.80,1);world.node_tree.nodes['Background'].inputs[1].default_value=.7;scene.world=world
        sun=bpy.data.lights.new('Soft sun','SUN');sun.energy=1.8;sun.angle=.3
        ob=bpy.data.objects.new('Soft sun',sun);bpy.context.collection.objects.link(ob);ob.rotation_euler=(.6,-.35,-.5)
        cam=bpy.data.cameras.new('Far source view');ob=bpy.data.objects.new('Far source view',cam);bpy.context.collection.objects.link(ob)
        ob.location=(0,-40,height*.5);ob.rotation_euler=(Vector((0,0,height*.5))-ob.location).to_track_quat('-Z','Y').to_euler();cam.type='ORTHO';cam.ortho_scale=canvas;scene.camera=ob
        scene.view_settings.view_transform='Standard';scene.view_settings.look='Medium High Contrast'
        output=base.SOURCE/(config['name']+'_far_cpu.png');scene.render.filepath=str(output)
        (base.SOURCE/(config['name']+'_far_camera.json')).write_text(json.dumps({'canvas_height_m':canvas,'tree_height_m':height,'resolution':[768,1024],
                  'device':'CYCLES CPU','threads':2,'samples':24},indent=2))
        print('CPU BAKE',config['name'],'two threads',flush=True);bpy.ops.render.render(write_still=True)
    for config in TREES:
        for lod in ['near','mid']:
            clear();bpy.ops.import_scene.gltf(filepath=str(base.SOURCE/config['source']/('components_'+lod+'.gltf')))
            for obj in list(bpy.context.scene.objects):
                if obj.type!='MESH' or obj.name.startswith('foliage_'):continue
                if 'foliage_branches' in obj.name:
                    major(obj,60 if lod=='near' else 20);budget=1800 if lod=='near' else 450
                elif config['source']=='tree_small_02':
                    isbranch=obj.name=='wood_0';major(obj,110 if lod=='near' else 28) if isbranch else None
                    budget=(3200 if isbranch else 1100) if lod=='near' else (600 if isbranch else 650)
                else:budget=400 if lod=='near' else 150
                decimate(obj,budget)
            normalize(config['height']);entry=export(config['name']+'_'+lod,config['source'],{'lod':lod,'method':'original source leaf/needle components with original UVs'})
            assert entry['triangles'] <= (20000 if lod=='near' else 4000),entry
            if lod=='near' and '--no-bake' not in sys.argv:bake(config)
    for lod,budget in [('near',6500),('mid',1500)]:
        clear();bpy.ops.import_scene.gltf(filepath=str(base.SOURCE/'dead_tree_trunk_02/deadwood_prepared.gltf'))
        for obj in bpy.context.scene.objects:
            if obj.type=='MESH':decimate(obj,budget)
        lo,hi=bounds()
        for obj in bpy.context.scene.objects:
            if obj.type=='MESH':obj.location-=Vector(((lo.x+hi.x)*.5,(lo.y+hi.y)*.5,lo.z))
        bpy.context.view_layer.update();export('cinematic_deadwood' if lod=='near' else 'cinematic_deadwood_mid','dead_tree_trunk_02',{'lod':lod})
    base.provenance()

def finish_far():
    import numpy as np
    from PIL import Image
    manifest_path=base.OUT/'cinematic_manifest.json';manifest=json.loads(manifest_path.read_text())
    for config in TREES:
        prefix=config['name'];image=Image.open(base.SOURCE/(prefix+'_far_cpu.png')).convert('RGBA')
        info=json.loads((base.SOURCE/(prefix+'_far_camera.json')).read_text());alpha=np.array(image)[:,:,3]
        ys,xs=np.nonzero(alpha>12);box=(max(0,int(xs.min())-1),max(0,int(ys.min())-1),min(image.width,int(xs.max())+2),min(image.height,int(ys.max())+2))
        crop=base.SOURCE/(prefix+'_far_atlas.png');image.crop(box).save(crop,optimize=True)
        unit=info['canvas_height_m']/image.height
        left=(box[0]-image.width*.5)*unit;right=(box[2]-image.width*.5)*unit;height=(box[3]-box[1])*unit
        w=Writer([{'name':'Source canopy camera-facing silhouette','alphaMode':'MASK','alphaCutoff':.4,'doubleSided':True,
                  'pbrMetallicRoughness':{'baseColorTexture':{'index':0},'metallicFactor':0,'roughnessFactor':.95}}],
                 [{'source':0,'sampler':0}],[{'uri':str(crop),'mimeType':'image/png'}])
        w.mesh(prefix+'_far',np.array([[left,0,0],[right,0,0],[right,height,0],[left,height,0]]),np.tile([0,0,1],(4,1)),
               np.array([[0,1],[1,1],[1,0],[0,0]]),np.array([[0,1,2],[0,2,3]],np.uint32),0)
        path=base.OUT/(prefix+'_far.glb');w.save(path,embedded=True)
        entry={'asset':prefix+'_far','source':config['source'],'license':'CC0-1.0','triangles':2,'height_m':height,
               'bounds_xyz_z_up':[[left,right],[0.,0.],[0.,height]],'bytes':path.stat().st_size,'sha256':base.sha(path),
               'lod':'far','root_padding_m':0,'method':'single CPU-baked upright billboard, original-component near source'}
        manifest[:]=[r for r in manifest if r['asset']!=entry['asset']];manifest.append(entry)
        print('PACKED',entry['asset'],'height',height,'root padding0',flush=True)
    manifest_path.write_text(json.dumps(manifest,indent=2));base.provenance()

if __name__=='__main__':
    if '--prepare' in sys.argv:prepare()
    elif '--finish-far' in sys.argv:finish_far()
    else:cook()
