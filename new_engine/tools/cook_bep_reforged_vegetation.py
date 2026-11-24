#!/usr/bin/env python3
"""Bounded native cook of explicitly selected purchased Unity TreeCreator meshes.

Reads the recovered binary-prefab mesh manifest and only its selected texture
dependencies. Does not import Unity, execute source scripts, alter projects or
run a GPU. Source metres/pivots, submeshes and authored LODs remain explicit.
The two grass patches are new deterministic compositions of intact source cards.
"""
from pathlib import Path
import argparse,collections,hashlib,json,sys
import numpy as np
from PIL import Image
from scipy.spatial.transform import Rotation
from scipy.ndimage import distance_transform_edt
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
from cook_bep_countryside_materials import linear,srgb,resize,sha
from cook_village_publisher_scene import material_writer

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT.parent/'.local-tools/bep-source'
OUT=ROOT/'assets/purchased/bep_reforged/countryside-vegetation'
sys.path.insert(0,str(ROOT.parent/'.local-tools/unity-source/pythondeps'))

SPECS={
 'oak':('c618fe5bfa375084691c1af3579755fe','6a9e9884164f6924f8a16989015a5e28'),
 # The standalone Beech_03 MID references three absent texture GUIDs in both
 # purchased packages. No authored wrapper overrides them. Keep exact HIGH
 # rather than guessing an unrelated atlas on lower geometry.
 'beech':('8ab88abcedd9d5f499a4869a492f6283',None),
 'bush_a':('4384371648ebb954fb1f484651563807',None),
 'bush_b':('2721ae6d86a0bfb4f99c0750de265625',None),
 'fern':('19d1542897ce4ab488b51ff52e215ced','384222e3ba907154a8256c5fdf550dd9'),
 'clover':('683be07145cdf3a4aa2d304ad946a9b8','6549c3dcb6892384386adc4e1044b59b'),
 'grass_long':('dba0ef41e3e74b849a75b78132c74ef3','dba0ef41e3e74b849a75b78132c74ef3'),
 'grass_broad':('dba0ef41e3e74b849a75b78132c74ef3','dba0ef41e3e74b849a75b78132c74ef3'),
}

def canonical(raw):
    h=raw.hex();return ''.join(h[i+1]+h[i]for i in range(0,len(h),2))
def pairs(rows):return {(k.get('name')if isinstance(k,dict)else k):v for k,v in rows}
def normalize(v):return v/np.maximum(np.linalg.norm(v,axis=1,keepdims=True),1e-12)

class Cook:
    def __init__(self,output):
        self.out=Path(output);(self.out/'models').mkdir(parents=True,exist_ok=True);(self.out/'textures').mkdir(exist_ok=True)
        self.manifest=json.loads((SOURCE/'prefab-meshes/manifest.json').read_text());self.assets={a['source']['guid']:a for a in self.manifest['assets']}
        self.inventory={};self.texture_audit=[];self.rows={};self.image_cache={};self.env={};self.geometry_audit=[]
        for package in ['BEP_Built-in_01','BEP_Built-in_02']:
            for r in json.loads((SOURCE/package/'inventory.json').read_text())['assets']:self.inventory[r['guid']]=dict(r,package=package)
    def source_file(self,guid):
        row=self.inventory[guid];p=SOURCE/row['package']/'guid'/guid/'asset'
        if not p.is_file():raise FileNotFoundError('Explicitly recover GUID '+guid+' ('+row['path']+')')
        return p
    def texture_guid(self,asset,tex):
        if tex.get('guid'):return tex['guid']
        import UnityPy
        guid=asset['source']['guid']
        if guid not in self.env:
            env=UnityPy.load(str(self.source_file(guid)));self.env[guid]=next(iter(env.files.values()))
        file=int(tex.get('m_FileID',tex.get('file_id',0)))
        if not file:raise ValueError(('Expected external source texture',guid,tex))
        return canonical(self.env[guid].externals[file-1].guid)
    def image(self,a,role,size):
        a=np.uint8(np.clip(np.rint(a*255),0,255));key=hashlib.sha256(a.tobytes()+str(a.shape).encode()).hexdigest()
        if key not in self.image_cache:
            p=self.out/'textures'/(key[:20]+'_'+role+'.png');Image.fromarray(a).save(p,compress_level=3);self.image_cache[key]=p
        return str(self.image_cache[key].resolve())
    def material(self,asset,ref):
        guid=ref.get('guid',asset['source']['guid']);source=self.assets[guid]
        obj=next(o for o in source['objects']if o['id']==ref['file_id']and o['type']=='Material');data=obj['data'];key=guid+':'+str(obj['id'])
        if key in self.rows:return key
        shader=data['m_Shader']['file_id']
        if shader not in [10604,10605]:raise ValueError(('Unsupported source shader',guid,data['m_Name'],shader))
        props=data['m_SavedProperties'];tex=pairs(props['m_TexEnvs']);floats=pairs(props['m_Floats']);colors=pairs(props['m_Colors'])
        textures={name:self.texture_guid(source,row['m_Texture'])for name,row in tex.items()if name in ['_MainTex','_BumpSpecMap','_TranslucencyMap']}
        for row in tex.values():
            assert row['m_Scale']==dict(x=1.,y=1.)and row['m_Offset']==dict(x=0.,y=0.),'Nonidentity source UV transform requires explicit baking'
        raw={name:np.asarray(Image.open(self.source_file(g)).convert('RGBA'),np.float32)/255 for name,g in textures.items()}
        d=raw['_MainTex'];b=raw['_BumpSpecMap'];t=raw['_TranslucencyMap'];leaf=shader==10605
        cutoff=float(floats.get('_Cutoff',.5));target=(min(1024,d.shape[1]),min(1024,d.shape[0]));alpha=d[:,:,3]
        rgb=linear(d[:,:,:3]);a=np.clip(resize(alpha[:,:,None],target)[:,:,0],0,1)
        rgb=resize(rgb*alpha[:,:,None],target)/np.maximum(a[:,:,None],1e-6)
        valid=a>.02
        if valid.any():
            nearest=distance_transform_edt(~valid,return_distances=False,return_indices=True);rgb[~valid]=rgb[nearest[0][~valid],nearest[1][~valid]]
        if target!=(d.shape[1],d.shape[0]):
            coverage=float((alpha>=cutoff).mean());lo,hi=.25,4.
            for _ in range(24):
                f=(lo+hi)*.5
                if float((a*f>=cutoff).mean())<coverage:lo=f
                else:hi=f
            a=np.clip(a*(lo+hi)*.5,0,1)
        base=self.image(np.dstack((srgb(np.clip(rgb,0,1)),a)),'base',1024)
        # The packed source is TreeCreator DXT5nm, NOT an ordinary RGB normal.
        xy=b[:,:,[3,1]]*2-1;n=np.dstack((xy,np.sqrt(np.maximum(1-np.sum(xy*xy,axis=2),0))))
        normal_size=(min(512,b.shape[1]),min(512,b.shape[0]));n=normalize(resize(n,normal_size).reshape(-1,3)).reshape(normal_size[1],normal_size[0],3);n[:,:,1]*=-1
        normal=self.image(n*.5+.5,'normal',512)
        # Source Blinn-Phong exponent = 128 * map.R. Match its lobe width to
        # GGX perceptual roughness. Native fixed dielectric F0 is an explicit
        # approximation; source gloss amplitude is retained in unused ARM.A.
        rough=(2/(128*b[:,:,0]+2))**.25;gloss=t[:,:,3]
        arm=np.dstack((np.ones_like(rough),rough,np.zeros_like(rough),gloss));arm=resize(arm,normal_size)
        arm_path=self.image(arm,'arm',512)
        color=colors.get('_Color',dict(r=1,g=1,b=1,a=1));factor=linear(np.array([color[k]for k in 'rgb'])).tolist()+[1.]
        row=dict(name=data['m_Name'],base=base,normal=normal,arm=arm_path,metallic=0.,roughness=1.,factor=factor,
            # Native draw masks only when cutoff>0 and never blends alpha.
            # MASK/0 bark therefore rejects no texel, exactly like opaque,
            # while sharing the leaf atlas instead of force-alpha255 copying.
            alphaMode='MASK',alphaCutoff=cutoff if leaf else 0.,doubleSided=leaf,facade_mode=0,source_paint=True,
            source_shader=shader,source_material_guid=guid,source_material_id=obj['id'],source_texture_guids=textures,
            source_gloss_amplitude='Retained in ARM.A; fixed native dielectric F0 is an approximation',source_vertex_alpha='Canonical RGB8 source-paint AO; material tint replaces original wind RGB',
            limitations=['Native existing wind/transmission approximate the source TreeCreator lighting; no source wind scripts are executed'])
        self.rows[key]=row
        self.texture_audit.append(dict(material=key,source_shader=shader,source_cutoff=cutoff,leaf=leaf,source_factor=color,native_linear_factor=factor,
            textures=[dict(role=k,guid=g,path=self.inventory[g]['path'],sha256=sha(self.source_file(g)),size=list(Image.open(self.source_file(g)).size))for k,g in textures.items()],
            normal_channels='A/G; +Z reconstructed; native green inverted once for UV V flip',roughness='(2/(128*BumpSpec.R+2))^.25',
            maps={k:dict(path=row[k],sha256=sha(row[k]))for k in ['base','normal','arm']}))
        return key
    def geometry(self,guid):
        asset=self.assets[guid];mesh=asset['meshes'][0];z=np.load(SOURCE/mesh['arrays']);v={k:z[k].copy()for k in z.files if not k.startswith('indices_')};parts=[]
        obj={o['id']:o for o in asset['objects']};filters=[o for o in asset['objects']if o['type']=='MeshFilter'and o['data']['m_Mesh']['file_id']==mesh['id']]
        if len(filters)>1:
            groups=[o for o in asset['objects']if o['type']=='LODGroup']
            if len(groups)==1:
                lod_renderers=groups[0]['data']['m_LODs'][0]['renderers'];gos={obj[r['renderer']['file_id']]['data']['m_GameObject']['file_id']for r in lod_renderers}
                filters=[o for o in filters if o['data']['m_GameObject']['file_id']in gos]
        if len(filters)!=1:raise ValueError(('Ambiguous source embedded mesh binding',guid))
        go=filters[0]['data']['m_GameObject']['file_id'];renderer=next(o for o in asset['objects']if o['type']=='MeshRenderer'and o['data']['m_GameObject']['file_id']==go)
        binding=None
        if guid=='384222e3ba907154a8256c5fdf550dd9':
            # Actual Fern_01 LODGroup assigns the HIGH leaf material to MID.
            # Its standalone generated material is not the authored binding.
            wrapper=self.assets['803835cab50068744a1bc9d93f9a6604'];wrapper_filters=[o for o in wrapper['objects']if o['type']=='MeshFilter'and o['data']['m_Mesh'].get('guid')==guid]
            assert len(wrapper_filters)==1
            wrapper_go=wrapper_filters[0]['data']['m_GameObject']['file_id'];renderer=next(o for o in wrapper['objects']if o['type']=='MeshRenderer'and o['data']['m_GameObject']['file_id']==wrapper_go)
            binding=dict(wrapper_guid=wrapper['source']['guid'],wrapper_path=wrapper['source']['path'],mesh_filter=wrapper_filters[0]['id'],renderer=renderer['id'],materials=renderer['data']['m_Materials'])
        transform=next(o['data']for o in asset['objects']if o['type']=='Transform'and o['data']['m_GameObject']['file_id']==go)
        q=transform['m_LocalRotation'];r=Rotation.from_quat([q[k]for k in 'xyzw']).as_matrix();s=np.array([transform['m_LocalScale'][k]for k in 'xyz']);tr=np.array([transform['m_LocalPosition'][k]for k in 'xyz'])
        m=r@np.diag(s);v['position']=v['position']@m.T+tr;v['normal']=normalize(v['normal']@np.linalg.inv(m));v['tangent'][:,:3]=normalize(v['tangent'][:,:3]@m.T)
        for sm in mesh['submeshes']:
            slot=sm['slot'];ref=renderer['data']['m_Materials'][slot];parts.append((z['indices_'+str(slot)].copy(),self.material(asset,ref)))
        # Left-handed Unity to right-handed native. V is also reflected, so
        # tangent handedness remains the original sign (two reflections).
        for key in ['position','normal']:v[key][:,2]*=-1
        v['tangent'][:,2]*=-1;v['uv0'][:,1]=1-v['uv0'][:,1];v['uv1'][:,1]=1-v['uv1'][:,1]
        for i,(ix,key)in enumerate(parts):
            p=v['position'][ix];nn=v['normal'][ix].sum(1);bad=np.einsum('ij,ij->i',np.cross(p[:,1]-p[:,0],p[:,2]-p[:,0]),nn)<0;ix[bad]=ix[bad,::-1];parts[i]=(ix,key)
        self.geometry_audit.append(dict(guid=guid,source=asset['source']['path'],mesh=mesh['id'],original_npz_sha256=mesh['sha256'],vertices=len(v['position']),source_triangles=sum(len(ix)for ix,_ in parts),color_alpha_range=[float(v['color'][:,3].min()),float(v['color'][:,3].max())],source_wrapper_material_binding=binding))
        return v,parts
    def cluster(self,guid,alias,lod):
        v,parts=self.geometry(guid);assert len(parts)==1
        ix,key=parts[0];n=len(v['position']);graph=coo_matrix((np.ones(ix.size),(ix.reshape(-1),np.roll(ix,1,axis=1).reshape(-1))),shape=(n,n));_,labels=connected_components(graph,directed=False)
        groups=[ix[labels[ix[:,0]]==label]for label in np.unique(labels[ix[:,0]])];groups=[g for g in groups if len(g)>=2]
        groups.sort(key=lambda g:(len(g),float(v['position'][np.unique(g),0].mean())))
        rng=np.random.default_rng(481 if alias=='grass_long'else 829);arrays=collections.defaultdict(list);faces=[];offset=0
        # Twelve complete original strips form HIGH. MID retains four exact
        # spatially spread strips from that same patch, not another silhouette
        # or arbitrary triangle decimation. Both aliases share source textures.
        count=12
        for i in range(count):
            g=groups[i%len(groups)];used,inverse=np.unique(g,return_inverse=True);a={k:x[used].copy()for k,x in v.items()}
            p=a['position'];p[:,0]-=(p[:,0].min()+p[:,0].max())*.5;p[:,2]-=(p[:,2].min()+p[:,2].max())*.5;p[:,1]-=p[:,1].min()
            height=float(p[:,1].max());width=max(float(np.ptp(p[:,0])),float(np.ptp(p[:,2])),.1)
            scale=min((.45+.30*rng.random())/max(height,.1),.90/width);a['position']*=scale
            angle=rng.uniform(-np.pi,np.pi);r=Rotation.from_euler('y',angle).as_matrix()
            a['position']=a['position']@r.T;a['normal']=a['normal']@r.T;a['tangent'][:,:3]=a['tangent'][:,:3]@r.T
            anchor=i;x=(anchor%4-1.5)*.78+rng.uniform(-.14,.14);z=(anchor//4-1)*.90+rng.uniform(-.14,.14)
            a['position']+=np.array([x,0,z]);
            if lod==1 and i not in [0,3,8,11]:continue
            for k in a:arrays[k].append(a[k])
            faces.append(inverse.reshape(-1,3)+offset);offset+=len(used)
        v={k:np.concatenate(a).astype(np.float32)for k,a in arrays.items()};return v,[(np.concatenate(faces).astype(np.uint32),key)]
    def emit(self,path,v,parts,miny,source_ao=True):
        v={k:a.copy()for k,a in v.items()};v['position'][:,1]-=miny;writer,mapping=material_writer({key:self.rows[key]for _,key in parts});primitives=[]
        for ix,key in parts:
            used,inverse=np.unique(ix,return_inverse=True);a={k:x[used]for k,x in v.items()};ao=np.round(np.clip(a['color'][:,3],0,1)*255)/255
            attrs={name:writer.array(a[k],kind,34962)for name,k,kind in [('POSITION','position','VEC3'),('NORMAL','normal','VEC3'),('TEXCOORD_0','uv0','VEC2'),('TEXCOORD_1','uv1','VEC2'),('TANGENT','tangent','VEC4')]}
            if source_ao:attrs['_VALWOOD_SOURCE_PAINT']=writer.array(np.repeat(ao[:,None],3,axis=1),'VEC3',34962)
            primitives.append(dict(attributes=attrs,indices=writer.array(inverse.reshape(-1,1),'SCALAR',34963),material=mapping[key]))
        # Shared zero datum keeps source pivots/LOD roots exact when the loader
        # grounds each model. It is a separate ordinary material, never facade.
        p=np.zeros((3,3),np.float32);dm=len(writer.g['materials']);writer.g['materials'].append(dict(name='Source_Pivot_Datum',pbrMetallicRoughness=dict(metallicFactor=0,roughnessFactor=1)))
        primitives.append(dict(attributes={'POSITION':writer.array(p,'VEC3',34962),'NORMAL':writer.array(np.tile([0,1,0],(3,1)),'VEC3',34962),'TEXCOORD_0':writer.array(p[:,:2],'VEC2',34962)},indices=writer.array(np.arange(3).reshape(-1,1),'SCALAR',34963),material=dm))
        writer.g['meshes']=[dict(primitives=primitives)];writer.g['nodes']=[dict(mesh=0)];writer.g['scenes'][0]['nodes']=[0];writer.save(path,embedded=True)
        assert path.stat().st_size<128*1024**2
        return dict(vertices=len(v['position']),triangles=sum(len(ix)for ix,_ in parts),bytes=path.stat().st_size,sha256=sha(path),bounds=[v['position'].min(0).tolist(),v['position'].max(0).tolist()])
    def run(self):
        catalog=[]
        for alias,(near,mid)in SPECS.items():
            mid=mid or near
            high=self.cluster(near,alias,0)if alias.startswith('grass_')else self.geometry(near)
            medium=self.cluster(mid,alias,1)if alias.startswith('grass_')else self.geometry(mid)
            miny=min(float(high[0]['position'][:,1].min()),float(medium[0]['position'][:,1].min()),0.)
            files=[];stats=[]
            for lod,(v,parts)in enumerate([high,medium]):
                p=self.out/'models'/('bep_nature_'+alias+('_near'if lod==0 else'_mid')+'.glb');stats.append(self.emit(p,v,parts,miny));files.append(str(p.resolve()))
            tree=alias in ['oak','beech'];small=alias.startswith('grass_')or alias=='clover'
            native=dict(alias='bep_nature_'+alias,vegetation=2 if tree else 1,collision=3 if tree else 0,distance=850 if tree else(95 if small else 170),split=75 if tree else(22 if small else 35))
            catalog.append(dict(name='BEP_Reforged_'+alias,files=files,pivot_offset_native=[0,miny,0],bounds=stats[0]['bounds'],stats=stats,native=native,
                source_guids=[near,mid],composed_grass_patch=alias.startswith('grass_'),source_geometry='Intact source connected strips/cards composed in irregular 3m patch'if alias.startswith('grass_')else'Exact native Unity source mesh; no simplification',
                lod_limitations=['Standalone source MID has absent texture GUIDs; exact HIGH reused']if alias=='beech'else(['Source has no distinct MID; exact HIGH reused']if alias.startswith('bush_')else[])))
            print('READY',alias,[s['triangles']for s in stats],flush=True)
        images={str(p):Image.open(p).size for p in self.image_cache.values()};bytes_gpu=sum(w*h*4*4/3 for w,h in images.values())
        # Record actual loader opacity variants (bark MASK/0 intentionally
        # shares leaf RGBA and generates none).
        opaque_bases={row['base']for row in self.rows.values()if row['alphaMode']=='OPAQUE'}
        opaque_variant_bytes=sum(np.prod(Image.open(p).size)*4*4/3 for p in opaque_bases)
        doc=dict(schema=1,assets=catalog,placements=[],cameras=[],source='BEP Built-in Reforged explicitly selected native Unity prefabs',source_manifest=str((SOURCE/'prefab-meshes/manifest.json').resolve()),
            texture_budget_RGBA8_mips_MiB=(bytes_gpu+opaque_variant_bytes)/1024**2,source_image_pages_RGBA8_mips_MiB=bytes_gpu/1024**2,
            native_opaque_albedo_variant_RGBA8_mips_MiB=opaque_variant_bytes/1024**2,texture_count=len(images),geometry_source_exact_except_new_grass_patch_compositions=True,
            limitations=['TreeCreator BlinnPhong gloss/transmission/wind are approximated by existing native PBR/foliage; source lobe roughness, AG normal, cutoff, material tint and vertex AO retained','Source material Color properties decoded sRGB to native linear factors','Bush original dimensions remain exact; size shrubs with instance scale, not geometry normalization'])
        (self.out/'catalog.json').write_text(json.dumps(doc,indent=2,allow_nan=False)+'\n');(self.out/'materials.json').write_text(json.dumps(self.rows,indent=2)+'\n')
        (self.out/'source-audit.json').write_text(json.dumps(dict(materials=self.texture_audit,geometry=self.geometry_audit,
            texture_GPU_RGBA8_mips_MiB=(bytes_gpu+opaque_variant_bytes)/1024**2,source_image_pages_RGBA8_mips_MiB=bytes_gpu/1024**2,native_opaque_albedo_variant_RGBA8_mips_MiB=opaque_variant_bytes/1024**2),indent=2,allow_nan=False)+'\n')
        print('CATALOG READY',self.out/'catalog.json',len(catalog),'assets',bytes_gpu/1024**2,'MiB',flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=OUT);a=p.parse_args();Cook(a.output).run()
