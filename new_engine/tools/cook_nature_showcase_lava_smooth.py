#!/usr/bin/env python3
"""Lava-only source geometry with shared tiled maps and continuous vertex heat.

Never routes triangles between temperature materials. Four original aliases and
their source pivots remain stable; no architecture/GI asset is modified.
"""
from pathlib import Path
import argparse, collections, copy, json
import numpy as np
import cook_nature_showcase as base
from align_nature_showcase_lods import align
from build_cinematic_diversity import Writer

def scalar(writer, values):
    values=np.ascontiguousarray(values,dtype='<f4').reshape(-1)
    offset=len(writer.bin);writer.bin.extend(values.tobytes());writer.bin.extend(b'\0'*(-len(writer.bin)%4))
    view=len(writer.g['bufferViews']);writer.g['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':values.nbytes,'target':34962})
    index=len(writer.g['accessors']);writer.g['accessors'].append({'bufferView':view,'componentType':5126,'count':len(values),'type':'SCALAR','min':[float(values.min())],'max':[float(values.max())]})
    return index

def emit(name, parts, info, output):
    near=np.concatenate([v[:,:3] for meta,v,index in parts if meta['lod']==0]);lo,hi=near.min(0),near.max(0)
    origin=np.array([(lo[0]+hi[0])/2,lo[1],(lo[2]+hi[2])/2],np.float32)
    images=[];textures=[]
    for role in ['base','normal','arm','emissive']:
        images.append({'uri':info['paths'][role],'mimeType':'image/png'});textures.append({'source':len(images)-1,'sampler':0})
    factor=info['emissive_factor'];strength=max(factor)
    material={'name':info['name'],'pbrMetallicRoughness':{'baseColorFactor':[1,1,1,1],'metallicFactor':1,'roughnessFactor':1,'baseColorTexture':{'index':0},'metallicRoughnessTexture':{'index':2}},
        'normalTexture':{'index':1},'occlusionTexture':{'index':2},'emissiveTexture':{'index':3},'emissiveFactor':[x/strength for x in factor],
        'extensions':{'KHR_materials_emissive_strength':{'emissiveStrength':strength}}}
    if info.get('double_sided'):material['doubleSided']=True
    continuous=info.get('continuous_heat',False)
    output.mkdir(parents=True,exist_ok=True);files=[];triangles=[]
    for lod,label in [(0,'near'),(1,'mid')]:
        writer=Writer([copy.deepcopy(material)],copy.deepcopy(textures),copy.deepcopy(images));groups=[[],[],[],[],[]];count=0
        writer.g['extensionsUsed']=['KHR_materials_emissive_strength']
        writer.g['asset']['extras']={'source_flow':'Original source geometry/UVs; shared source tiled crust; continuous vertex heat; bounded native emission; static flow'}
        for meta,vertex,index in parts:
            if meta['lod']!=lod:continue
            used,idx=np.unique(index,return_inverse=True);v=vertex[used];p=v[:,:3]-origin;n=v[:,3:6].copy();uv=v[:,6:8].copy();heat=meta['heat'][used]
            p[:,2]*=-1;n[:,2]*=-1;uv[:,1]=1-uv[:,1];idx=idx.reshape(-1,3)
            n/=np.maximum(np.linalg.norm(n,axis=1,keepdims=True),1e-10)
            cross=np.cross(p[idx[:,1]]-p[idx[:,0]],p[idx[:,2]]-p[idx[:,0]]);valid=np.linalg.norm(cross,axis=1)>1e-9
            if valid.any()and np.median((cross*n[idx].sum(1)).sum(1)[valid])<0:idx=idx[:,[0,2,1]]
            for slot,array in enumerate([p,n,uv,idx+count,heat]):groups[slot].append(array)
            count+=len(p)
        arrays=[np.concatenate(a)for a in groups];writer.mesh(name,*arrays[:4],0)
        if continuous:
            attrs=writer.g['meshes'][-1]['primitives'][0]['attributes']
            attrs['_VALWOOD_LAVA_HEAT']=scalar(writer,arrays[4])
        path=output/(name+'_'+label+'.glb');writer.save(path,embedded=True);files.append(str(path));triangles.append(len(arrays[3]))
    return {'name':name,'origin':origin.tolist(),'dimensions':(hi-lo).tolist(),'files':files,'triangles':triangles,'continuous_heat':continuous}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--materials',type=Path,required=True);parser.add_argument('--source',type=Path,default=base.DEFAULT_SOURCE);parser.add_argument('--output',type=Path,default=base.ROOT/'assets/purchased/nature_castle/showcase_lava_smooth');args=parser.parse_args()
    # This helper evaluates source height/slope/RGBA weights at vertices rather
    # than selecting a single material at each triangle centroid.
    from prepare_nature_lava_materials import vertex_heat
    args.output.mkdir(exist_ok=True);source=base.Source(args.source);results=json.loads(args.materials.read_text());selected=collections.defaultdict(list);placements=collections.Counter();heat_stats={}
    for p in source.doc['placements']:
        if p['group']!='Emission'and not any('M_Lava_River_'in path for path in p['materials']):continue
        original,indices=source.mesh(p['mesh']);matrix=np.asarray(p['matrix'],np.float64).reshape(4,4,order='F');v=original.copy()
        v[:,:3]=original[:,:3]@matrix[:3,:3].T+matrix[:3,3];v[:,3:6]=original[:,3:6]@np.linalg.inv(matrix[:3,:3]);v[:,3:6]/=np.maximum(np.linalg.norm(v[:,3:6],axis=1,keepdims=True),1e-10)
        for sub,index in enumerate(indices):
            path=source.material(p,sub)
            if p['group']!='Emission'and'M_Lava_River_'not in path:continue
            info=results[path]
            heat=vertex_heat(path,v)if info.get('continuous_heat')else np.ones(len(v),np.float32)
            if not np.isfinite(heat).all()or heat.min()<0 or heat.max()>1:raise ValueError('Invalid continuous source heat: '+path)
            vertex=v.copy();vertex[:,6:8]=vertex[:,6:8]*np.array(info.get('uv_scale',[1,1]))+np.array(info.get('uv_offset',[0,0]))
            for lod in [0,1]:
                if p['lod_mask']&(1<<lod):selected[path].append(({'name':p['name'],'lod':lod,'heat':heat},vertex,index))
            placements[path]+=1
    base.install_deduplicated_writer(results);catalog=[]
    for path,parts in sorted(selected.items()):
        name='nms_lava_'+Path(path).stem.lower().replace('m_','',1)
        record=emit(name,parts,results[path],args.output/'chunks');record.update(source_material=path,source_placements=placements[path],source_flow_adaptation='Original source triangles retained, one shared tiled source material, continuous pervertex source heat, bounded redorange/yellow emission; static flow')
        align(record);catalog.append(record);print('SMOOTH LAVA',name,record['triangles'],flush=True)
        h=np.concatenate([meta['heat']for meta,v,index in parts if meta['lod']==0]);heat_stats[path]={'vertices':len(h),'min':float(h.min()),'max':float(h.max()),'percentiles':np.percentile(h,[10,50,90]).tolist()}
    base.atomic_json(args.output/'catalog.json',catalog);base.atomic_json(args.output/'READY.json',{'catalog':str(args.output/'catalog.json'),'assets':len(catalog),'source_triangles_preserved':True,'categorical_triangle_routes':False,'heat_statistics':heat_stats});print('SMOOTH LAVA READY',len(catalog),'assets')

if __name__=='__main__':main()
