#!/usr/bin/env python3
"""Repack only the two measured barrier source components into one 2K atlas.

Every chart is an unrotated, positive U/V scaling of an original triangle.
The source shape layer retains original UV0, while tiled grain retains UV3.
No geometry, scene imports, shaders, or purchased sources are modified.
"""
from pathlib import Path
import argparse,collections,hashlib,json,math
import numpy as np
from PIL import Image
from scipy.ndimage import distance_transform_edt
from cook_nature_showcase import Source,DEFAULT_SOURCE,ROOT
from bake_nature_material_fidelity import FidelityBaker

NAMES={'Bariere_Post_LOD0','Bariere_Stairs_Middle_LOD0'}
MATERIALS={'M_Pillar_stone_02_snow','M_Pillar_stone_03_snow'}
VARIANT='M_BarrierSnow__source_dual_uv_atlas_2k'

def derivatives(p,uv):
    d=np.stack((uv[1]-uv[0],uv[2]-uv[0]),axis=0)
    if abs(np.linalg.det(d))<1e-12:return None
    return np.linalg.solve(d,np.stack((p[1]-p[0],p[2]-p[0]),axis=0))

def pack(charts,density,size,padding):
    rects=[]
    for i,c in enumerate(charts):
        span=c['uv_span'];su,sv=np.array(c['world_derivative_lengths'])*density
        w=max(2,int(math.ceil(span[0]*su))+1)+2*padding
        h=max(2,int(math.ceil(span[1]*sv))+1)+2*padding
        if max(w,h)>size:return None
        rects.append((i,w,h,su,sv))
    shelves=[];result={};height=0
    for i,w,h,su,sv in sorted(rects,key=lambda r:(-r[2],-r[1],r[0])):
        options=[(row[2]-h,row[1]-row[0]-w,j)for j,row in enumerate(shelves)if row[2]>=h and row[0]+w<=row[1]]
        if options:
            _,_,j=min(options);row=shelves[j];x,y=row[0],row[3];row[0]+=w
        else:
            if height+h>size:return None
            x,y=0,height;shelves.append([w,size,h,height]);height+=h
        result[i]=(x,y,w,h,su,sv)
    return result

def normalized(v):return v/np.maximum(np.linalg.norm(v,axis=-1,keepdims=True),1e-20)
def basis(n,du,dv):
    t=normalized(du-n*np.sum(n*du,axis=-1,keepdims=True));b=np.cross(n,t)
    b*=np.where(np.sum(b*dv,axis=-1,keepdims=True)<0,-1.,1.)
    return t,b

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source',type=Path,default=DEFAULT_SOURCE)
    ap.add_argument('--output',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-barrier-atlas')
    ap.add_argument('--size',type=int,default=2048)
    ap.add_argument('--piece',action='append',default=[])
    ap.add_argument('--hierarchy-contains',default='')
    ap.add_argument('--variant',default=VARIANT)
    ap.add_argument('--minimum-density',type=int,default=112)
    ap.add_argument('--padding',type=int,default=6)
    args=ap.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    names=set(args.piece)if args.piece else NAMES
    source=Source(args.source);placements=[p for p in source.doc['placements']if p['lod_mask']&1 and p['name']in names and args.hierarchy_contains in p['hierarchy']and any(Path(m).stem in MATERIALS for m in p['materials'])]
    by_mesh=collections.defaultdict(list)
    for p in placements:by_mesh[p['mesh']].append(p)
    charts=[];parts=collections.defaultdict(list);skipped=0
    for mesh,ps in sorted(by_mesh.items()):
        v,indices=source.mesh(mesh)
        for sub,i in enumerate(indices):
            paths={source.material(p,sub)for p in ps if Path(source.material(p,sub)).stem in MATERIALS}
            for path in sorted(paths):
                matrices=[np.asarray(p['matrix']).reshape(4,4,order='F')[:3,:3]for p in ps if source.material(p,sub)==path]
                parts[path].append((v,i))
                for triangle,ids in enumerate(i.reshape(-1,3)):
                    q=v[ids];d=derivatives(q[:,:3],q[:,6:8])
                    if d is None or np.linalg.norm(np.cross(q[1,:3]-q[0,:3],q[2,:3]-q[0,:3]))<1e-10:skipped+=1;continue
                    lengths=np.max([np.linalg.norm(d@m.T,axis=1)for m in matrices],axis=0)
                    charts.append({'mesh':mesh,'submesh':sub,'triangle':triangle,'material':path,'vertex_ids':ids.tolist(),'uv_min':q[:,6:8].min(0),'uv_max':q[:,6:8].max(0),'uv_span':np.ptp(q[:,6:8],axis=0),'world_derivative_lengths':lengths,'v':q,'d':d})
    padding=args.padding;chosen=None
    if not 3<=padding<=16:raise ValueError('Atlas gutter must be3–16texels')
    for density in range(256,63,-4):
        chosen=pack(charts,density,args.size,padding)
        if chosen:break
    if chosen is None or density<args.minimum_density:raise ValueError('The requested atlas cannot meet the minimum source-world density: '+str(density))
    print('ATLAS',len(charts),'triangle charts;',density,'texels/source-world m;',len(placements),'instances',flush=True)
    primary=np.zeros((args.size,args.size,2),np.float32);mapping=np.zeros((args.size,args.size,6),np.float32);mapping[:,:,2]=mapping[:,:,5]=1
    owner=np.full((args.size,args.size),-1,np.int8);valid=np.zeros((args.size,args.size),bool);paths=sorted(parts);records=[]
    for ci,c in enumerate(charts):
        x,y,w,h,su,sv=chosen[ci];q=c['v'];lo,hi=c['uv_min'],c['uv_max'];yy,xx=np.mgrid[y:y+h,x:x+w]
        uv=np.stack((lo[0]+(xx-x-padding)/su,hi[1]-(yy-y-padding)/sv),axis=-1)
        d=np.stack((q[1,6:8]-q[0,6:8],q[2,6:8]-q[0,6:8]),axis=0);bc=(uv-q[0,6:8])@np.linalg.inv(d);weights=np.stack((1-bc.sum(-1),bc[:,:,0],bc[:,:,1]),axis=-1)
        uv3=weights@q[:,8:10];nw=np.maximum(weights,0);nw/=np.maximum(nw.sum(-1,keepdims=True),1e-20);n=normalized(nw@q[:,3:6]);du,dv=c['d'];du3=derivatives(q[:,:3],q[:,8:10]);du3,dv3=du3 if du3 is not None else(du,dv)
        t0,b0=basis(n,du,dv);t3,b3=basis(n,du3,dv3)
        primary[y:y+h,x:x+w]=uv;mapping[y:y+h,x:x+w,:2]=uv3
        for k,value in enumerate((np.sum(t3*t0,-1),np.sum(b3*t0,-1),np.sum(t3*b0,-1),np.sum(b3*b0,-1))):mapping[y:y+h,x:x+w,2+k]=value
        owner[y:y+h,x:x+w]=paths.index(c['material']);inside=(weights.min(-1)>=-1e-6);valid[y:y+h,x:x+w]=inside
        newuv=np.stack(((x+padding+.5+(q[:,6]-lo[0])*su)/args.size,1-(y+padding+.5+(hi[1]-q[:,7])*sv)/args.size),axis=-1).astype(np.float32)
        # Positive independent U/V scale leaves the orthonormal tangent basis
        # unchanged, even where source UV islands are very skinny.
        changed=derivatives(q[:,:3],newuv);old_t,old_b=basis(normalized(q[:,3:6]),du,dv);new_t,new_b=basis(normalized(q[:,3:6]),*changed)
        if min(np.sum(old_t*new_t,-1).min(),np.sum(old_b*new_b,-1).min())<.99999:raise ValueError('Chart rotation/parity changed')
        records.append({'mesh':c['mesh'],'submesh':c['submesh'],'triangle':c['triangle'],'material':c['material'],'vertex_ids':c['vertex_ids'],'atlas_uv':newuv.tolist(),'rectangle':[x,y,w,h],'texels_per_source_world_meter':density})
    baker=FidelityBaker(parts,args.output/'source-materials',args.size,args.size);baked={}
    for path in paths:
        baked[path]=baker.make(path,atlas_mapping=mapping,primary_uv=primary)
        if baked[path]['status']!='candidate':raise ValueError('Source atlas bake failed '+path)
        print('ATLAS material',Path(path).stem,flush=True)
    nearest=distance_transform_edt(~valid,return_distances=False,return_indices=True);dest=args.output/'textures';dest.mkdir(exist_ok=True);output_paths={}
    for role in('base','normal','arm'):
        out=np.zeros((args.size,args.size,3),np.uint8)
        for i,path in enumerate(paths):
            a=np.asarray(Image.open(baked[path]['paths'][role]).convert('RGB'));mask=owner==i;out[mask]=a[mask]
        out=out[nearest[0],nearest[1]];image=Image.fromarray(out)
        if role=='arm':image=image.resize((512,512),Image.Resampling.LANCZOS)
        file=dest/(role+'.png');image.save(file);output_paths[role]=str(file.resolve())
    original=np.asarray(Image.open(output_paths['base']).convert('RGB'));quality=None
    for q in(96,97,98,99):
        file=dest/'base.jpg';Image.fromarray(original).save(file,quality=q,subsampling=0,optimize=True);decoded=np.asarray(Image.open(file).convert('RGB'));e=np.abs(original.astype(float)-decoded)
        if e.mean()<=1.5 and np.percentile(e,99)<=8:quality={'quality':q,'chroma_subsampling':0,'mean_error_u8':float(e.mean()),'p99_error_u8':float(np.percentile(e,99))};break
    if quality is None:raise ValueError('Atlas opaque encoding exceeds source quality guard')
    output_paths['base']=str(file.resolve())
    info={'name':args.variant,'status':'candidate','size':args.size,'arm_size':512,'paths':output_paths,'double_sided':False,'emissive_factor':None,'alphaMode':'OPAQUE','fingerprint':hashlib.sha256(primary.tobytes()+mapping.tobytes()+json.dumps(records).encode()).hexdigest(),'source_materials':paths,'source_UV0_shape_and_UV3_base_preserved':True,'unrotated_positive_UV_charts':True,'base_encoding':quality}
    (args.output/'material.json').write_text(json.dumps(info,indent=2)+'\n')
    report={'source':str(args.source.resolve()),'source_placement_count':len(placements),'source_placements':[{'name':p['name'],'hierarchy':p['hierarchy'],'mesh':p['mesh'],'matrix':p['matrix']}for p in placements],'charts':records,'atlas_size':args.size,'padding':padding,'texels_per_world_meter':density,'skipped_degenerate_triangles':skipped,'filled_triangle_texels':int(valid.sum()),'production_untouched':True,'geometry_positions_normals_GI_unchanged':True}
    (args.output/'charts.json').write_text(json.dumps(report,indent=2)+'\n');print('READY',args.output/'material.json',flush=True)

if __name__=='__main__':main()
