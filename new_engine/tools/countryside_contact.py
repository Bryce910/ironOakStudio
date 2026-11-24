"""CPU-only contact fitting against the actual rendered countryside triangles.

Does not reground source meshes at their AABB minima: fern/clover have authored
underground stems. Source root-plane zero is preserved, including across LODs.
"""
from pathlib import Path
import copy,json,math
import numpy as np
from build_village_showcase_project import glb_geometry,matrix


class TerrainSurfaces:
    def __init__(self,catalog,origin,fallback):
        self.origin=np.asarray(origin,float);self.fallback=fallback;self.tiles=[[],[]];self.fallback_queries=[0,0]
        assets={a['name']:a for a in catalog['assets']}
        for placement in catalog['placements']:
            if not placement['asset'].startswith('BEP_Countryside_Terrain_'):continue
            asset=assets[placement['asset']];m=matrix(placement['matrix_native']);m[:3,3]+=m[:3,:3]@np.asarray(asset['pivot_offset_native'])+self.origin
            for lod,path in enumerate(asset['files']):
                vertices,triangles=glb_geometry(path);q=vertices[triangles]
                area=(q[:,1,0]-q[:,0,0])*(q[:,2,2]-q[:,0,2])-(q[:,1,2]-q[:,0,2])*(q[:,2,0]-q[:,0,0])
                triangles=triangles[np.abs(area)>1e-9];q=vertices[triangles]@m[:3,:3].T+m[:3,3]
                # An edge fan is deliberately not a rectangular height grid.
                # Bin the actual triangle AABBs and use exact barycentrics so
                # stitched rims never silently become heightfield fallbacks.
                lo=q[:,:,[0,2]].min(axis=(0,1));hi=q[:,:,[0,2]].max(axis=(0,1));step=(hi-lo)/128
                low=np.clip(np.floor((q[:,:,[0,2]].min(axis=1)-lo)/step).astype(int),0,127)
                high=np.clip(np.floor((q[:,:,[0,2]].max(axis=1)-lo)/step).astype(int),0,127)
                keys=[];ids=[];index=np.arange(len(q))
                for dz in range(int((high[:,1]-low[:,1]).max())+1):
                    for dx in range(int((high[:,0]-low[:,0]).max())+1):
                        cell=low+[dx,dz];valid=np.all(cell<=high,axis=1);keys.append(cell[valid,1]*128+cell[valid,0]);ids.append(index[valid])
                keys=np.concatenate(keys);ids=np.concatenate(ids);order=np.argsort(keys,kind='stable');keys=keys[order];ids=ids[order]
                offsets=np.searchsorted(keys,np.arange(128*128+1))
                self.tiles[lod].append((lo,hi,step,q,offsets,ids))

    def sample(self,points,lod=0):
        p=np.asarray(points,float);shape=p.shape[:-1];p=p.reshape(-1,2);out=np.full(len(p),np.nan)
        for lo,hi,step,triangles,offsets,ids in self.tiles[lod]:
            selected=np.all(p>=lo,axis=1)&np.all(p<=hi,axis=1)
            indices=np.flatnonzero(selected)
            if not len(indices):continue
            cell=np.clip(np.floor((p[indices]-lo)/step).astype(int),0,127);key=cell[:,1]*128+cell[:,0]
            for k in np.unique(key):
                target=indices[key==k];q=p[target];t=triangles[ids[offsets[k]:offsets[k+1]]]
                if not len(t):continue
                a,b,c=t[:,0],t[:,1],t[:,2];den=(b[:,2]-c[:,2])*(a[:,0]-c[:,0])+(c[:,0]-b[:,0])*(a[:,2]-c[:,2])
                w=((b[:,2]-c[:,2])*(q[:,None,0]-c[:,0])+(c[:,0]-b[:,0])*(q[:,None,1]-c[:,2]))/den
                u=((c[:,2]-a[:,2])*(q[:,None,0]-c[:,0])+(a[:,0]-c[:,0])*(q[:,None,1]-c[:,2]))/den;v=1-w-u
                valid=(w>=-1e-7)&(u>=-1e-7)&(v>=-1e-7);h=w*a[:,1]+u*b[:,1]+v*c[:,1];h=np.max(np.where(valid,h,-np.inf),axis=1)
                okay=np.isfinite(h);out[target[okay]]=h[okay]
        missing=~np.isfinite(out)
        if missing.any():
            self.fallback_queries[lod]+=int(missing.sum());out[missing]=np.asarray(self.fallback(p[missing]))+.05
        return out.reshape(shape)


def root_points(asset,grass_sites):
    """Intersect the authored soil plane, not the underground-stem minimum."""
    alias=asset['native']['alias'];offset=np.asarray(asset['pivot_offset_native'])
    if alias in grass_sites:
        # The bottom of the photographed clump is opaque only across its
        # central~quarter. Include that visible strip on every crossed card,
        # not the transparent outer corners or just twelve tuft centers.
        vertices,triangles=glb_geometry(asset['files'][0]);q=vertices[triangles]
        edge=np.concatenate((q[:,[0,1]],q[:,[1,2]],q[:,[2,0]]));bottom=np.all(abs(edge[:,:,1])<1e-6,axis=1)&(np.linalg.norm(edge[:,1]-edge[:,0],axis=1)>.1)
        edge=edge[bottom];a,b=edge[:,0],edge[:,1]
        points=np.concatenate((np.asarray(grass_sites[alias],float),a+(b-a)*.35,a+(b-a)*.5,a+(b-a)*.65))
        return np.unique(np.round(points,6),axis=0)
    vertices,triangles=glb_geometry(asset['files'][0]);q=vertices[triangles]
    triangles=triangles[np.linalg.norm(np.cross(q[:,1]-q[:,0],q[:,2]-q[:,0]),axis=1)>1e-10]
    vertices=vertices+offset
    if alias in ['bep_nature_oak','bep_nature_beech','bep_nature_bush_a','bep_nature_bush_b','bep_nature_rock']:
        actual=vertices[np.unique(triangles)]
        if alias=='bep_nature_rock':points=actual[actual[:,1]<=np.percentile(actual[:,1],15)]
        else:
            points=actual[actual[:,1]<-.025]
            if alias in ['bep_nature_oak','bep_nature_beech']:points=points[np.linalg.norm(points[:,[0,2]],axis=1)<1.8]
            if not len(points):points=actual[actual[:,1]<=np.min(actual[:,1])+.025]
        if len(points)>48:points=points[np.linspace(0,len(points)-1,48,dtype=int)]
        return points
    q=vertices[triangles];edges=np.concatenate((q[:,[0,1]],q[:,[1,2]],q[:,[2,0]]))
    a,b=edges[:,0],edges[:,1];den=b[:,1]-a[:,1];cross=(a[:,1]*b[:,1]<=0)&(np.abs(den)>1e-8)
    points=a[cross]+(b[cross]-a[cross])*(-a[cross,1]/den[cross])[:,None]
    exact=vertices[np.abs(vertices[:,1])<1e-5]
    points=np.concatenate((points,exact,np.zeros((1,3))));points[:,1]=0
    points=np.unique(np.round(points,6),axis=0)
    # A bounded farthest-point sample retains the entire collar/patch footprint.
    if len(points)>48:
        chosen=[int(np.argmin(np.linalg.norm(points,axis=1)))];distance=np.full(len(points),np.inf)
        for _ in range(47):
            distance=np.minimum(distance,np.linalg.norm(points-points[chosen[-1]],axis=1));chosen.append(int(distance.argmax()))
        points=points[chosen]
    return points


def posed_points(placement,points,origin):
    m=matrix(placement['matrix_native']);return points@m[:3,:3].T+m[:3,3]+origin


def errors(placement,points,terrain):
    p=posed_points(placement,points,terrain.origin);return np.stack((p[:,1]-terrain.sample(p[:,[0,2]],0),p[:,1]-terrain.sample(p[:,[0,2]],1)))


def summary(records,assets,points,terrain):
    by_alias={}
    for i,p in enumerate(records):
        a=assets[p['asset']];alias=a.get('native',{}).get('alias','')
        if alias not in points:continue
        error=errors(p,points[alias],terrain);v=by_alias.setdefault(alias,dict(instances=0,samples=0,near=[],mid=[],bad_float=0,bad_burial=0))
        v['instances']+=1;v['samples']+=error.shape[1];v['near'].extend(error[0].tolist());v['mid'].extend(error[1].tolist())
        v['bad_float']+=int(error[0].max()>.08);v['bad_burial']+=int(error[0].min()<-.08)
    out={}
    for alias,v in by_alias.items():
        r={k:value for k,value in v.items()if k not in ['near','mid']}
        for key in ['near','mid']:
            values=np.asarray(v[key]);r[key]=dict(min=float(values.min()),max=float(values.max()),p50=float(np.median(values)),p95_abs=float(np.percentile(abs(values),95)))
        out[alias]=r
    return out


def fit_plane(m,points,terrain,scale):
    """Keep XZ and heading; fit one rigid soil plane over the real root sites."""
    heading=m[:3,0].copy();heading[1]=0;heading/=np.linalg.norm(heading)
    next_m=m.copy();up=np.array([0.,1,0.])
    for _ in range(3):
        x=heading-up*np.dot(heading,up);x/=np.linalg.norm(x);next_m[:3,:3]=np.stack((x,up,np.cross(x,up)),axis=1)*scale
        p=points@next_m[:3,:3].T+next_m[:3,3]+terrain.origin;target=terrain.sample(p[:,[0,2]],0)-.015
        q=p[:,[0,2]]-(next_m[[0,2],3]+terrain.origin[[0,2]])
        slope=np.linalg.lstsq(np.column_stack((q,np.ones(len(q)))),target,rcond=None)[0]
        up=np.array([-slope[0],1.,-slope[1]]);up/=np.linalg.norm(up)
        next_m[1,3]+=float(np.median(target-p[:,1]))
    return next_m


def correct(catalog,terrain,grass_audit):
    """Deterministic, local correction; never rerandomizes the scene."""
    assets={a['name']:a for a in catalog['assets']};grass_sites={a['alias']:a['tuft_sites_native']for a in grass_audit['geometry']}
    aliases={a.get('native',{}).get('alias'):a for a in assets.values()}
    points={alias:root_points(asset,grass_sites)for alias,asset in aliases.items()if alias and alias.startswith('bep_nature_')}
    before=summary(catalog['placements'],assets,points,terrain);changed=[];scaled=[];result=copy.deepcopy(catalog)
    for i,p in enumerate(result['placements']):
        asset=assets[p['asset']];alias=asset.get('native',{}).get('alias','')
        if alias not in points:continue
        old=matrix(p['matrix_native']);source_matrix=p.get('contact_original_matrix_native',p['matrix_native']);m=matrix(source_matrix);roots=points[alias]
        if alias.startswith('bep_nature_grass_')or alias in ['bep_nature_clover','bep_nature_fern']:
            scale=float(np.linalg.norm(m[:3,1]));best=None
            for factor in [1.,.9,.8,.7,.6,.5,.4,.3,.2,.15,.1,.075,.05,.035]:
                # Compress only the soil footprint on sharp curvature; retain
                # the original blade/frond height rather than miniaturizing it.
                uniform_min=max(.150001,scale/7.999)
                horizontal=max(scale*factor,math.sqrt(uniform_min**3/scale))
                candidate=fit_plane(m,roots,terrain,np.array([horizontal,scale,horizontal]));trial=copy.copy(p);trial['matrix_native']=candidate.T.flatten().tolist();error=errors(trial,roots,terrain)[0];score=float(np.max(abs(error+.015)))
                if best is None or score<best[0]:best=(score,candidate,factor)
                if score<=.045:break
            m=best[1]
            if best[2]<1:scaled.append(dict(index=i,alias=alias,ratio=best[2],max_near_root_error=best[0]))
        else:
            # Upright woody plants retain their scales, root sites, and yaw.
            # Only exposed actual underground roots/base vertices need repair;
            # negative source coordinates are intentionally below the soil.
            source_pose=copy.copy(p);source_pose['matrix_native']=source_matrix;error=errors(source_pose,roots,terrain)[0]
            m[1,3]-=float(np.clip(np.percentile(error,90)+.025,0,.25))
        if not np.array_equal(m,old):
            p['contact_original_matrix_native']=source_matrix
            p['matrix_native']=m.T.flatten().tolist();changed.append(dict(index=i,alias=alias,vertical_delta=float(m[1,3]-old[1,3]),horizontal_delta=0.))
    after=summary(result['placements'],assets,points,terrain)
    report=dict(kind='Actual HIGH/MID triangle root-plane contact; authored underground stems preserved',before=before,after=after,changed_placements=len(changed),scaled_patches=len(scaled),changes=changed,scaled=scaled,source_asset_records_unchanged=result['assets']==catalog['assets'],all_XZ_unchanged=all(np.array_equal(matrix(a['matrix_native'])[[0,2],3],matrix(b['matrix_native'])[[0,2],3])for a,b in zip(catalog['placements'],result['placements'])),fields_and_geometry_unchanged=True,surface_field_fallback_queries=list(terrain.fallback_queries))
    return result,report
