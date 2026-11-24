#!/usr/bin/env python3
"""Prepare isolated source-faithful NatureManufacture material candidates.

Does not replace production files, import revisions, or edit world.studio.
Example: --material M_Pillar_stone_01 --material M_stone_guard_01 --size 2048
Source planar/world-space cover still requires a native dual-UV material shader;
this tool rejects planar materials instead of silently baking the wrong mapping.
"""
from pathlib import Path
import argparse, collections, copy, ctypes, functools, hashlib, json, struct, subprocess
import numpy as np
from PIL import Image
from cook_nature_citadel import ROOT, DATA, NATIVE, yaml_material
from prepare_nature_guardian import srgb_to_linear, linear_to_srgb
from build_cinematic_diversity import Writer


def native_records(folder=NATIVE):
    """Read arbitrary NativeCitadelExport-compatible 14-float exports."""
    records=[]
    for path in sorted(Path(folder).glob('*.json')):
        doc=json.loads(path.read_text())
        if 'parts'not in doc or not path.with_suffix('.bin').is_file():continue
        blob=path.with_suffix('.bin').read_bytes();parts=[]
        for p in doc['parts']:
            v=np.ndarray((p['vertices'],14),'<f4',blob,p['vertex_offset']).copy()
            i=np.ndarray((p['indices'],),'<i4',blob,p['index_offset']).copy()
            if not np.isfinite(v).all()or np.any(i<0)or np.any(i>=len(v)):raise ValueError('Invalid native mesh')
            parts.append((p,v,i))
        records.append((doc,parts))
    return records


def native_parts(folder=NATIVE):
    parts=collections.defaultdict(list)
    for doc,source_parts in native_records(folder):
        for p,v,i in source_parts:
            if p['lod']!=0:continue
            parts[p['material']].append((v,i))
    return parts


class FidelityBaker:
    def __init__(self, parts, output, size, arm_size):
        self.parts,self.output,self.size,self.arm_size=parts,output,size,arm_size
        self.output.mkdir(parents=True,exist_ok=True)
        self.paths={e['guid']:e['path'] for e in json.loads((DATA/'INVENTORY.json').read_text())['entries']}
        library=output/'nature_fidelity_raster.so'
        subprocess.run(['g++','-O2','-shared','-fPIC',str(ROOT/'tools/nature_fidelity_raster.cpp'),'-o',str(library)],check=True)
        self.raster=ctypes.CDLL(str(library)).nature_fidelity_raster
        self.raster.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_void_p]
        yy,xx=np.mgrid[:size,:size]
        self.uv=np.stack(((xx+.5)/size,1-(yy+.5)/size),axis=2).astype(np.float32)

    @functools.lru_cache(maxsize=4)
    def source_image(self,path):
        with Image.open(path) as raw:return np.asarray(raw.convert('RGBA')).copy()

    @functools.lru_cache(maxsize=4)
    def source_mips(self,path,srgb):
        levels=[self.source_image(path)]
        while min(levels[-1].shape[:2])>1:
            a=levels[-1];h,w=a.shape[:2]
            if srgb and h%2==0 and w%2==0:
                # RGB textures decode before filtering on the GPU. Generate
                # matching color mips rather than averaging encoded sRGB.
                value=a.astype(np.float32)/255
                value[:,:,:3]=srgb_to_linear(value[:,:,:3])
                value=value.reshape(h//2,2,w//2,2,4).mean((1,3))
                value[:,:,:3]=linear_to_srgb(value[:,:,:3])
                nxt=np.uint8(np.clip(value,0,1)*255+.5)
            else:nxt=np.asarray(Image.fromarray(a).resize((max(1,w//2),max(1,h//2)),Image.Resampling.BOX)).copy()
            levels.append(nxt)
        return levels

    def make(self,path,planar_period=None,base_only=False,atlas_mapping=None,primary_uv=None):
        saved=yaml_material(DATA/'source'/path)
        f={k:v for r in saved['m_Floats']for k,v in r.items()}
        c={k:v for r in saved['m_Colors']for k,v in r.items()}
        tex={k:v for r in saved['m_TexEnvs']for k,v in r.items()}
        repairs=[]
        def resolved(info):
            guid=info.get('m_Texture',{}).get('guid')
            return guid in self.paths and (DATA/'source'/self.paths[guid]).is_file()
        # HDRP/Lit backgrounds use these names rather than the custom shape
        # shader names. Preserve the source-normal/mask maps instead of losing
        # the snow-mountain detail to neutral defaults.
        for target,alternates in {'_BaseColorMap':['_BaseMap','_MainTex'],'_BaseNormalMap':['_NormalMap','_BumpMap'],'_BaseMaskMap':['_MaskMap','_MetallicGlossMap'],'_ShapeNormal':['_ShapeNormalMap']}.items():
            if not resolved(tex.get(target,{})):
                for alternate in alternates:
                    if resolved(tex.get(alternate,{})):
                        repairs.append(target+' uses included '+alternate+' because the primary source map is missing')
                        tex[target]=tex[alternate];break
        for target,alternate in {'_BaseAORemapMin':'_AORemapMin','_BaseAORemapMax':'_AORemapMax','_BaseSmoothnessRemapMin':'_SmoothnessRemapMin','_BaseSmoothnessRemapMax':'_SmoothnessRemapMax','_ShapeNormalStrenght1':'_ShapeNormalScale'}.items():
            if target not in f and alternate in f:f[target]=f[alternate]
        base_guid=tex.get('_BaseColorMap',{}).get('m_Texture',{}).get('guid')
        constant_base=not base_guid and '_BaseColor'in c
        if constant_base:repairs.append('Source material intentionally uses constant BaseColor; no albedo image')
        planar=f.get('_BaseUsePlanarUV',0) or f.get('_Shape_UsePlanarUV',0)
        if planar:
            if planar_period is None:return {'name':Path(path).stem,'status':'requires native planar shader or explicit world-XZ period bake; not baked'}
            if not 0<float(planar_period)<=128:raise ValueError('Invalid planar bake period')
            for selector,uv_selector,tiling in [('_BaseUsePlanarUV','_UV0_UV2','_BaseTilingOffset'),('_Shape_UsePlanarUV','_Shape_UV0_UV2','_ShapeTilingOffset')]:
                if not f.get(selector,0):raise ValueError('Mixed planar/atlas material requires native shader')
                scale=c.get(tiling,{}).get('r',1)
                if scale<=0:raise ValueError('Invalid source planar period')
                c[tiling]={'r':planar_period/scale,'g':planar_period/scale,'b':0,'a':0}
                f[selector]=0;f[uv_selector]=1
        if tex.get('_BaseColorMap',{}).get('m_Texture',{}).get('guid')not in self.paths and '_fireplace'in path:
            dry=yaml_material(DATA/'source'/path.replace('_fireplace.mat','.mat'))
            dt={k:v for r in dry['m_TexEnvs']for k,v in r.items()}
            for key in ['_BaseColorMap','_BaseNormalMap','_BaseMaskMap']:tex[key]=dt[key]
            repairs.append('Unavailable trial-lava base replaced with matching purchased dry pillar base')
        if not constant_base and not resolved(tex.get('_BaseColorMap',{})):
            return {'name':Path(path).stem,'status':'missing source albedo with no included alternative; not baked','missing_guid':base_guid}
        def color(key,default=(1,1,1,1)):
            return np.array([c[key][v]for v in 'rgba'],np.float32)if key in c else np.array(default,np.float32)
        primary_uv=self.uv if primary_uv is None else np.asarray(primary_uv,np.float32)
        if primary_uv.shape!=self.uv.shape or not np.isfinite(primary_uv).all():raise ValueError('Invalid explicit primary UV atlas')
        mapping=np.full((self.size,self.size,6),np.nan,np.float32)if atlas_mapping is None else np.asarray(atlas_mapping,np.float32).copy();seen=set()
        if mapping.shape!=(self.size,self.size,6):raise ValueError('Invalid explicit secondary UV/basis atlas')
        requires_uv3=f.get('_UV0_UV2',1)<.5 or f.get('_Shape_UV0_UV2',1)<.5 or f.get('Leaks_UV0_UV2_1',1)<.5
        for vertex,index in self.parts[path]if requires_uv3 and atlas_mapping is None else []:
            # Repeated source pieces retain identical UV relationships under
            # prefab rotations/translations. A shared UV0 atlas already takes
            # first coverage, so rasterizing the same topology again adds no
            # information and can multiply bake work by hundreds of copies.
            digest=hashlib.sha256(vertex[:,6:10].tobytes()+index.tobytes()).digest()
            if digest in seen:continue
            seen.add(digest);self.raster(vertex.ctypes.data,index.ctypes.data,len(index),self.size,mapping.ctypes.data)
        missing=~np.isfinite(mapping[:,:,0]);mapping[missing,:2]=primary_uv[missing]
        mapping[missing,2:]=[1,0,0,1]
        uv3=mapping[:,:,:2]
        base_uv3=f.get('_UV0_UV2',1)<.5;shape_uv3=f.get('_Shape_UV0_UV2',1)<.5
        base_uv=uv3 if base_uv3 else primary_uv;shape_uv=uv3 if shape_uv3 else primary_uv
        sources={}
        def image(key,uv,tiling=None,default=(1,1,1,1),srgb=False):
            info=tex.get(key,{});guid=info.get('m_Texture',{}).get('guid')
            if not guid or guid not in self.paths:
                return np.broadcast_to(np.array(default,np.float32),(self.size,self.size,4)).copy()
            source=DATA/'source'/self.paths[guid];mips=self.source_mips(source,srgb);h,w=mips[0].shape[:2]
            sources[key]={'path':str(source.relative_to(ROOT)),'dimensions':[w,h]}
            tr=color(tiling,(1,1,0,0))if tiling and tiling in c else np.array([info.get('m_Scale',{}).get('x',1),info.get('m_Scale',{}).get('y',1),info.get('m_Offset',{}).get('x',0),info.get('m_Offset',{}).get('y',0)],np.float32)
            out=np.empty((self.size,self.size,4),np.float32)
            dy,dx=np.gradient(uv,axis=(0,1));density=np.array([w,h],np.float32)*tr[:2]
            rho=np.maximum(np.linalg.norm(dx*density,axis=2),np.linalg.norm(dy*density,axis=2))
            lod=np.clip(np.log2(np.maximum(rho,1)),0,len(mips)-1)
            lower=np.floor(lod).astype(np.int32);upper=np.minimum(lower+1,len(mips)-1);mix=lod-lower.astype(np.float32)
            for start in range(0,self.size,128):
                stop=min(self.size,start+128);xy=(uv[start:stop]*tr[:2]+tr[2:])%1
                out[start:stop]=0
                for level in np.unique(np.r_[lower[start:stop].ravel(),upper[start:stop].ravel()]):
                    weight=(lower[start:stop]==level)*(1-mix[start:stop])+(upper[start:stop]==level)*mix[start:stop]
                    selected=weight>0;coords=xy[selected]
                    im=mips[level];mh,mw=im.shape[:2]
                    x=(coords[:,0]*mw-.5)%mw;y=((1-coords[:,1])*mh-.5)%mh
                    xf=np.floor(x);yf=np.floor(y);x0=xf.astype(int)%mw;y0=yf.astype(int)%mh;fx=(x-xf)[:,None];fy=(y-yf)[:,None]
                    samples=[im[y0,x0].astype(np.float32)/255,im[y0,(x0+1)%mw].astype(np.float32)/255,im[(y0+1)%mh,x0].astype(np.float32)/255,im[(y0+1)%mh,(x0+1)%mw].astype(np.float32)/255]
                    if srgb:
                        for sample in samples:sample[:,:3]=srgb_to_linear(sample[:,:3])
                    row=out[start:stop]
                    row[selected]+=((samples[0]*(1-fx)+samples[1]*fx)*(1-fy)+(samples[2]*(1-fx)+samples[3]*fx)*fy)*weight[selected,None]
            return out
        base=image('_BaseColorMap',base_uv,'_BaseTilingOffset',srgb=True)
        mask=None if base_only else image('_BaseMaskMap',base_uv,'_BaseTilingOffset',default=(0,1,1,.1))
        shape=image('_ShapeCurvAOHLeaksMask',shape_uv,'_ShapeTilingOffset',default=(.5,1,1,1))
        leak_uv=primary_uv if f.get('Leaks_UV0_UV2_1',1)>=.5 else uv3
        leaks=image('_LeaksR',leak_uv,'_LeaksTilingOffset')[:,:,:1]
        # These two names are verified ShaderLab Color properties, whereas
        # UV/tiling entries are Vector data and must retain their raw values.
        # Unity's linear workflow converts Color RGB before shader upload.
        # Alpha remains a scalar blend weight; HDR emission factors are handled
        # separately and are deliberately unchanged by this base-only repair.
        stain=color('_BaseLeaksColorMultiply');stain[:3]=srgb_to_linear(stain[:3])
        tint=color('_BaseColor');tint[:3]=srgb_to_linear(tint[:3])
        weight=np.clip(stain[3]*(1-shape[:,:,3:4]),0,1)
        rgb=base[:,:,:3]*tint[:3]
        rgb*=1-weight+weight*((1-leaks)**2*stain[:3]+leaks)
        shape_ao=f.get('_ShapeAORemapMin',0)+shape[:,:,1:2]*(f.get('_ShapeAORemapMax',1)-f.get('_ShapeAORemapMin',0))
        curvature=np.clip(rgb+2*shape[:,:,:1]-1,0,1)
        reduction=f.get('_Shape_AO_Curvature_Reduction',0)
        curvature*=1-reduction+reduction*np.clip(shape_ao,0,1)
        amount=f.get('_CurvatureBlend',0);rgb=rgb*(1-amount)+curvature*amount
        if base_only:
            dest=self.output/'textures'/Path(path).stem;dest.mkdir(parents=True,exist_ok=True)
            base_path=dest/'base.png';Image.fromarray(np.uint8(np.clip(linear_to_srgb(rgb),0,1)*255+.5)).save(base_path)
            digest=hashlib.sha256(Path(__file__).read_bytes()+(DATA/'source'/path).read_bytes()+base_path.read_bytes()).hexdigest()
            return {'name':Path(path).stem,'status':'candidate','size':self.size,'arm_size':self.arm_size,'planar_period':planar_period if planar else None,'paths':{'base':str(base_path)},'fingerprint':digest,'sources':sources,'repairs':repairs,'linear_color_properties':['_BaseColor RGB','_BaseLeaksColorMultiply RGB'],'base_only':True,'uv3_coverage':float((~missing).mean())}
        normals=[]
        for key,uv,tiling,strength,uv3_used in [('_BaseNormalMap',base_uv,'_BaseTilingOffset',f.get('_BaseNormalScale',f.get('_NormalScale',1)),base_uv3),('_ShapeNormal',shape_uv,'_ShapeTilingOffset',f.get('_ShapeNormalStrenght1',1),shape_uv3)]:
            n=image(key,uv,tiling,default=(.5,.5,1,1))[:,:,:3]*2-1
            n[:,:,2]=np.sqrt(np.maximum(1-(n[:,:,:2]**2).sum(2),0))
            n[:,:,:2]*=strength;n[:,:,2]=1+(n[:,:,2]-1)*np.clip(strength,0,1)
            if uv3_used:
                x=n[:,:,0].copy();y=n[:,:,1].copy()
                n[:,:,0]=mapping[:,:,2]*x+mapping[:,:,3]*y
                n[:,:,1]=mapping[:,:,4]*x+mapping[:,:,5]*y
            normals.append(n)
        normal=np.dstack((normals[0][:,:,0]+normals[1][:,:,0],normals[0][:,:,1]+normals[1][:,:,1],normals[0][:,:,2]*normals[1][:,:,2]))
        normal/=np.maximum(np.linalg.norm(normal,axis=2,keepdims=True),1e-8)
        normal[:,:,1]*=-1 # Shipping geometry flips V; regenerated bitangent flips with it.
        smooth=f.get('_BaseSmoothnessRemapMin',0)+mask[:,:,3:4]*(f.get('_BaseSmoothnessRemapMax',1)-f.get('_BaseSmoothnessRemapMin',0))
        smooth*=1-weight+weight*(f.get('_LeaksSmoothnessMultiply',1)*(1-leaks)+leaks)
        base_ao=f.get('_BaseAORemapMin',0)+mask[:,:,1:2]*(f.get('_BaseAORemapMax',1)-f.get('_BaseAORemapMin',0))
        arm=np.concatenate((base_ao*shape_ao,1-np.clip(smooth,0,1),mask[:,:,:1]*f.get('_BaseMetallic',f.get('_Metallic',1))),2)
        dest=self.output/'textures'/Path(path).stem;dest.mkdir(parents=True,exist_ok=True)
        values=[('base',linear_to_srgb(rgb)),('normal',normal*.5+.5),('arm',arm)]
        glass='_ThicknessEmission'in c and '_Thickness'in tex
        emissive_factor=None
        if glass:
            thickness=image('_Thickness',self.uv)[:,:,:1]
            t=np.clip(f.get('_ThicknessRemapMin',0)+(1-thickness)*(f.get('_ThicknessRemapMax',1)-f.get('_ThicknessRemapMin',0)),0,1)
            values.append(('emissive',linear_to_srgb(base[:,:,:3]*t)))
            emissive_factor=color('_ThicknessEmission')[:3].tolist()
        elif np.max(color('_EmissiveColor',(0,0,0,0))[:3])>0:
            # HDRP constant-emission objects, such as the source brazier core.
            values.append(('emissive',np.ones((self.size,self.size,3),np.float32)))
            emissive_factor=color('_EmissiveColor')[:3].tolist()
        for role,value in values:
            im=Image.fromarray(np.uint8(np.clip(value,0,1)*255+.5))
            if role=='arm':im=im.resize((self.arm_size,self.arm_size),Image.Resampling.LANCZOS)
            im.save(dest/(role+'.png'))
        fingerprint=hashlib.sha256(b'Nature material fidelity1\0'+(DATA/'source'/path).read_bytes()+str((self.size,self.arm_size,planar_period)).encode())
        for source in sources.values():fingerprint.update(hashlib.sha256((ROOT/source['path']).read_bytes()).digest())
        fingerprint.update((ROOT/'tools/nature_fidelity_raster.cpp').read_bytes())
        fingerprint.update(Path(__file__).read_bytes())
        if atlas_mapping is not None:fingerprint.update(mapping.tobytes());fingerprint.update(primary_uv.tobytes())
        for vertex,index in self.parts[path]:fingerprint.update(vertex[:,:10].tobytes());fingerprint.update(index.tobytes())
        return {'name':Path(path).stem,'status':'candidate','fingerprint':fingerprint.hexdigest(),'size':self.size,'arm_size':self.arm_size,'planar_period':planar_period if planar else None,'paths':{role:str(dest/(role+'.png'))for role,value in values},'uv3_coverage':float((~missing).mean()),'sources':sources,'repairs':repairs,'emissive_factor':emissive_factor,'double_sided':glass or f.get('_DoubleSidedEnable',0)>=.5,'linear_color_properties':['_BaseColor RGB','_BaseLeaksColorMultiply RGB'],'limitations':['UV overlap uses first coverage; a native dual-UV shader preserves repeated/mirrored piece variation better','Source snow cover/world-space projection is not baked except explicit horizontal period-bake option']}


def world_xz_planar_vertices(vertices,period,source_world_matrix=None):
    """Generate Unity source-world XZ UVs for a horizontal planar period bake.

    Geometry remains local; caller passes the original Unity instance matrix
    before relocating the demo. This keeps all adjacent floor pieces in phase.
    Emit this UV0 with the usual V flip. This is not a triplanar rock substitute.
    """
    if not 0<float(period)<=128:raise ValueError('Invalid source planar period')
    result=np.ascontiguousarray(vertices,dtype=np.float32).copy();positions=result[:,:3]
    if source_world_matrix is not None:
        matrix=np.asarray(source_world_matrix,np.float32).reshape(4,4)
        positions=np.column_stack((positions,np.ones(len(positions),np.float32)))@matrix.T
    result[:,6]=positions[:,0]/period;result[:,7]=positions[:,2]/period
    result[:,8:10]=result[:,6:8]
    return result


def emit_native_asset(doc,parts,material_results,output,winter=False):
    """Cook any exported assembly to isolated GLBs; no canonical import writes.

    material_results maps full Unity material paths to FidelityBaker.make results.
    Returned origin converts source prefab placements to bottom-center geometry.
    Source prefab scale is retained; the caller owns world placement and LOD use.
    """
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    near=np.concatenate([v[:,:3]for p,v,i in parts if p['lod']==0])
    lo,hi=near.min(0),near.max(0);origin=np.array([(lo[0]+hi[0])/2,lo[1],(lo[2]+hi[2])/2],np.float32)
    materials=[];textures=[];images=[];indices={}
    for path in sorted({p['material']for p,v,i in parts}):
        info=material_results[path]
        if info['status']!='candidate':raise ValueError('Native material emission requires a valid candidate: '+path)
        n=len(textures)
        for role in ['base','normal','arm']:
            images.append({'uri':info['paths'][role],'mimeType':'image/png'});textures.append({'source':len(images)-1,'sampler':0})
        material={'name':info['name'],'pbrMetallicRoughness':{'baseColorFactor':[1,1,1,1],'metallicFactor':1,'roughnessFactor':1,'baseColorTexture':{'index':n},'metallicRoughnessTexture':{'index':n+2}},'normalTexture':{'index':n+1},'occlusionTexture':{'index':n+2}}
        if winter and 'snow'in Path(path).stem.lower():material['extensions']={'VALWOOD_materials_winter':{'parameters':[1,.86,0,0]}}
        if info['double_sided']:material['doubleSided']=True
        if info['emissive_factor']:
            images.append({'uri':info['paths']['emissive'],'mimeType':'image/png'});textures.append({'source':len(images)-1,'sampler':0})
            factor=info['emissive_factor'];strength=max(factor)
            material['emissiveTexture']={'index':len(textures)-1};material['emissiveFactor']=[x/strength for x in factor]
            material.setdefault('extensions',{})['KHR_materials_emissive_strength']={'emissiveStrength':strength}
        indices[path]=len(materials);materials.append(material)
    files=[];triangles=[]
    for lod,label in [(0,'near'),(1,'mid')]:
        writer=Writer(*copy.deepcopy((materials,textures,images)))
        if any(m.get('emissiveTexture')for m in materials):writer.g['extensionsUsed']=['KHR_materials_emissive_strength']
        groups=collections.defaultdict(lambda:[[],[],[],[]]);counts=collections.Counter()
        for meta,vertex,index in parts:
            if meta['lod']!=lod:continue
            used,idx=np.unique(index,return_inverse=True);v=vertex[used]
            p=v[:,:3]-origin;n=v[:,3:6].copy();uv=v[:,6:8].copy()
            p[:,2]*=-1;n[:,2]*=-1;uv[:,1]=1-uv[:,1];idx=idx.reshape(-1,3)
            n/=np.maximum(np.linalg.norm(n,axis=1,keepdims=True),1e-8)
            cross=np.cross(p[idx[:,1]]-p[idx[:,0]],p[idx[:,2]]-p[idx[:,0]])
            valid=np.linalg.norm(cross,axis=1)>1e-9
            if valid.any()and np.median((cross*n[idx].sum(1)).sum(1)[valid])<0:idx=idx[:,[0,2,1]]
            mat=indices[meta['material']];g=groups[mat]
            for slot,array in enumerate([p,n,uv,idx+counts[mat]]):g[slot].append(array)
            counts[mat]+=len(p)
        if not groups and lod==1:continue
        total=0
        for mat,g in groups.items():
            arrays=[np.concatenate(x)for x in g];writer.mesh(doc['name']+str(mat),*arrays,mat);total+=len(arrays[3])
        path=output/(doc['name']+'_'+label+'.glb');writer.save(path,embedded=True)
        if path.stat().st_size>128*1024**2:raise ValueError('Candidate exceeds native GLB limit; reduce texture tier')
        files.append(str(path));triangles.append(total)
    return {'name':doc['name'],'origin':origin.tolist(),'dimensions':(hi-lo).tolist(),'files':files,'triangles':triangles}


def glb_document(path):
    raw=path.read_bytes();magic,version,length=struct.unpack_from('<III',raw)
    if magic!=0x46546c67 or version!=2 or length!=len(raw):raise ValueError('Invalid source GLB')
    n,kind=struct.unpack_from('<II',raw,12)
    if kind!=0x4e4f534a:raise ValueError('Missing JSON chunk')
    doc=json.loads(raw[20:20+n]);offset=20+n
    b,kind=struct.unpack_from('<II',raw,offset)
    if kind!=0x004e4942:raise ValueError('Missing BIN chunk')
    return doc,bytearray(raw[offset+8:offset+8+b])


def repack_asset(name,report,output):
    """Reuse source-identical geometry and only substitute candidate images."""
    materials={v['name']:v for v in report.values()if v['status']=='candidate'}
    results=[]
    for lod in ['near','mid']:
        source=DATA/'citadel'/(name+'_'+lod+'.glb');doc,binary=glb_document(source)
        image_cache={};substituted=[]
        def append_image(path,image_index=None):
            payload=Path(path).read_bytes();key=hashlib.sha256(payload).hexdigest()
            if key not in image_cache:
                offset=len(binary);binary.extend(payload);binary.extend(b'\0'*(-len(binary)%4))
                view=len(doc['bufferViews']);doc['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(payload)})
                image_cache[key]=view
            image={'mimeType':'image/png','bufferView':image_cache[key]}
            if image_index is None:
                image_index=len(doc['images']);doc['images'].append(image)
                index=len(doc['textures']);doc['textures'].append({'source':image_index,'sampler':0})
                return index
            doc['images'][image_index]=image
        for mat in doc['materials']:
            info=materials.get(mat.get('name'))
            if info is None:continue
            pbr=mat['pbrMetallicRoughness']
            for role,texture in [('base',pbr['baseColorTexture']),('normal',mat['normalTexture']),('arm',pbr['metallicRoughnessTexture'])]:
                append_image(info['paths'][role],doc['textures'][texture['index']]['source'])
            if info['emissive_factor']:
                mat['emissiveTexture']={'index':append_image(info['paths']['emissive'])}
                factor=info['emissive_factor'];strength=max(factor)
                mat['emissiveFactor']=[x/strength for x in factor]
                mat.setdefault('extensions',{})['KHR_materials_emissive_strength']={'emissiveStrength':strength}
                if 'KHR_materials_emissive_strength'not in doc.setdefault('extensionsUsed',[]):doc['extensionsUsed'].append('KHR_materials_emissive_strength')
            if info['double_sided']:mat['doubleSided']=True
            substituted.append(mat['name'])
        doc['buffers'][0]['byteLength']=len(binary)
        doc['asset']['generator']='Valwood isolated NatureManufacture fidelity candidate; source geometry preserved'
        encoded=json.dumps(doc,separators=(',',':'),allow_nan=False).encode();encoded+=b' '*(-len(encoded)%4)
        body=struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(binary),0x004e4942)+binary
        result=struct.pack('<III',0x46546c67,2,len(body)+12)+body
        if len(result)>128*1024**2:raise ValueError('Candidate exceeds native static GLB limit; use smaller texture tier')
        target=output/(name+'_'+lod+'.glb');target.write_bytes(result)
        results.append({'path':str(target),'bytes':len(result),'substituted_materials':substituted,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest()})
    return results


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--material',action='append',default=[])
    p.add_argument('--asset',action='append',default=[],help='Also repack existing native geometry into isolated candidate GLBs')
    p.add_argument('--size',type=int,choices=[256,512,1024,2048,4096],default=2048)
    p.add_argument('--arm-size',type=int,choices=[256,512,1024,2048],default=1024)
    p.add_argument('--output',type=Path,default=ROOT/'docs/stronghold/2026-11-01/showcase/material-candidates')
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    parts=native_parts();names=set(args.material)
    for asset in args.asset:
        if not asset.replace('_','').isalnum():raise SystemExit('Invalid asset name')
        doc,_=glb_document(DATA/'citadel'/(asset+'_near.glb'))
        names.update(mat['name']for mat in doc['materials'])
    selected=[path for path in parts if not names or Path(path).stem in names]
    if names and len(selected)!=len(names):raise SystemExit('Unknown or ambiguous source material')
    baker=FidelityBaker(parts,args.output,args.size,args.arm_size);report={}
    for path in sorted(selected):
        report[path]=baker.make(path)
        (args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(report[path]['name'],report[path]['status'],flush=True)
    asset_report={name:repack_asset(name,report,args.output)for name in args.asset}
    if asset_report:(args.output/'assets.json').write_text(json.dumps(asset_report,indent=2)+'\n')


if __name__=='__main__':main()
