#!/usr/bin/env python3
"""Stage harmonic support in source-ground holes; preserve covered samples.

No project installation. Optional house constraints are recovered from actual
nondegenerate source LOW vertices, and rejected if they conflict with paving
or another requested footing. Outside-field houses require separate supports.
"""
from pathlib import Path
import argparse, hashlib, json, os, struct, tempfile, time
import numpy as np
from scipy.sparse import coo_matrix, diags
from scipy.sparse.linalg import cg
from scipy.spatial import ConvexHull, QhullError
from build_city_countryside import ROOT, readfield
from build_village_showcase_project import glb_geometry, matrix


def sha_bytes(data):return hashlib.sha256(data).hexdigest()
def sha(path):return sha_bytes(Path(path).read_bytes())


def recover_footings(catalog, components, origin, heights, covered, grid_origin, step):
    """Recover conservative flat, lowest-vertex hulls, without AABB proxies."""
    assets={a['name']:a for a in catalog['assets']};constraints=np.zeros_like(covered)
    levels=np.zeros(heights.shape,np.float64);records=[]
    for component in components:
        placements=[p for p in catalog['placements']if p.get('source_component')==component]
        if len(placements)!=1:raise ValueError(f'House component {component}: expected one source placement')
        placement=placements[0];asset=assets[placement['asset']]
        if not asset['name'].startswith('SM_House_'):raise ValueError(f'Component {component} is not a source house')
        vertices,triangles=glb_geometry(asset['files'][0]);q=vertices[triangles]
        valid=np.linalg.norm(np.cross(q[:,1]-q[:,0],q[:,2]-q[:,0]),axis=1)>1e-10
        vertices=vertices[np.unique(triangles[valid])]
        transform=matrix(placement['matrix_native']);transform[:3,3]+=transform[:3,:3]@asset['pivot_offset_native']+origin
        vertices=vertices@transform[:3,:3].T+transform[:3,3]
        low=float(vertices[:,1].min());roots=vertices[vertices[:,1]<=low+.02]
        points=np.unique(roots[:,[0,2]],axis=0)
        if len(points)<3:raise ValueError(f'House {component}: no broad, flat source-low footprint')
        try:hull=ConvexHull(points)
        except QhullError as error:raise ValueError(f'House {component}: degenerate source-low footprint')from error
        if hull.volume<1.:raise ValueError(f'House {component}: source-low footprint below 1m²')
        minimum=np.maximum(0,np.floor((points.min(0)-grid_origin)/step).astype(int))
        maximum=np.minimum(np.array(heights.shape)[::-1]-1,np.ceil((points.max(0)-grid_origin)/step).astype(int))
        record=dict(source_component=component,asset=asset['name'],source_model=str(asset['files'][0]),source_model_sha256=sha(asset['files'][0]),
            lowest_world_y=low,footprint_world_xz=points[hull.vertices].tolist(),footprint_area_m2=float(hull.volume))
        if np.any(maximum<minimum):
            record.update(status='outside_primary_field',constrained_nodes=0);records.append(record);continue
        yy,xx=np.mgrid[minimum[1]:maximum[1]+1,minimum[0]:maximum[0]+1]
        world=np.stack((xx,yy),-1)*step+grid_origin
        inside=np.all(world@hull.equations[:,:2].T+hull.equations[:,2]<=1e-8,axis=-1)
        ys,xs=yy[inside],xx[inside]
        # Visual soil is field+.05m. Bury the source foundation by 2.5cm.
        level=low-.025
        if np.any(covered[ys,xs]&(abs(heights[ys,xs]-level)>.03)):
            raise ValueError(f'House {component}: footing conflicts with covered source paving')
        if np.any(constraints[ys,xs]&(abs(levels[ys,xs]-level)>.03)):
            raise ValueError(f'House {component}: footing conflicts with another requested house')
        select=~covered[ys,xs];ys,xs=ys[select],xs[select]
        constraints[ys,xs]=True;levels[ys,xs]=level
        record.update(status='constrained'if len(ys)else'covered_paving_matches',field_world_y=level,constrained_nodes=len(ys));records.append(record)
    return constraints,levels,records


def harmonic(values, fixed, step):
    unknown=~fixed;ids=np.full(values.shape,-1,np.int32);ids[unknown]=np.arange(unknown.sum())
    ys,xs=np.nonzero(unknown);count=len(ys)
    if not count:return values.astype(np.float64),dict(unknown_nodes=0,iterations=0,seconds=0.)
    if not fixed.any():raise ValueError('Harmonic solve requires covered or footing constraints')
    row=np.arange(count);degree=np.zeros(count);rhs=np.zeros(count);rows=[];cols=[];weights=[]
    for dy,dx,weight in [(0,1,1/step[0]**2),(0,-1,1/step[0]**2),(1,0,1/step[1]**2),(-1,0,1/step[1]**2)]:
        yy=ys+dy;xx=xs+dx;valid=(yy>=0)&(yy<values.shape[0])&(xx>=0)&(xx<values.shape[1])
        selected=row[valid];yy=yy[valid];xx=xx[valid];degree[selected]+=weight
        boundary=fixed[yy,xx];rhs[selected[boundary]]+=values[yy[boundary],xx[boundary]]*weight
        rows.append(selected[~boundary]);cols.append(ids[yy[~boundary],xx[~boundary]]);weights.append(np.full((~boundary).sum(),-weight))
    rows.append(row);cols.append(row);weights.append(degree)
    operator=coo_matrix((np.concatenate(weights),(np.concatenate(rows),np.concatenate(cols))),shape=(count,count)).tocsr()
    iterations=[0]
    def tick(_):iterations[0]+=1
    start=time.perf_counter()
    solution,status=cg(operator,rhs,x0=values[unknown],M=diags(1/degree),rtol=1e-9,atol=1e-8,maxiter=5000,callback=tick)
    if status!=0 or not np.isfinite(solution).all():raise ValueError(f'Harmonic solve did not converge: {status}')
    output=values.astype(np.float64);output[unknown]=solution
    return output,dict(unknown_nodes=count,nonzeros=int(operator.nnz),iterations=iterations[0],seconds=time.perf_counter()-start,
        physical_weights=[float(1/step[0]**2),float(1/step[1]**2)],maximum_weighted_residual=float(abs(operator@solution-rhs).max()),
        outer_boundary='natural/no-flux; only existing in-grid neighbors contribute')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project',type=Path,default=ROOT/'projects/valwood_city_showcase')
    parser.add_argument('--catalog',type=Path,help='Source catalog; required for optional house components')
    parser.add_argument('--house-component',type=int,action='append',default=[])
    parser.add_argument('--output',type=Path,default=ROOT/'assets/purchased/bep_reforged/city-ground-support')
    args=parser.parse_args()
    if args.output.exists():raise ValueError('Use a new immutable output directory')
    source=args.project/'ground/showcase.height';coverage=args.project/'ground/city.coverage'
    heights,origin,step=readfield(source);mask,mask_origin,mask_step=readfield(coverage)
    if heights.shape!=mask.shape or not np.array_equal(origin,mask_origin)or not np.array_equal(step,mask_step):raise ValueError('Coverage grid differs from source height grid')
    if not np.isfinite(heights).all()or not np.isfinite(mask).all():raise ValueError('Non-finite source data')
    covered=mask>.5;fixed=covered.copy();values=heights.astype(np.float64);footings=[]
    if args.house_component:
        if not args.catalog:raise ValueError('--house-component requires --catalog')
        world_origin=np.asarray(json.loads((args.project/'city_showcase.json').read_text())['origin'])
        pads,levels,footings=recover_footings(json.loads(args.catalog.read_text()),args.house_component,world_origin,heights,covered,origin,step)
        fixed|=pads;values[pads]=levels[pads]
    output,solver=harmonic(values,fixed,step);output=np.ascontiguousarray(output,dtype='<f4')
    # Copy original bytes under actual source geometry, rather than relying on
    # the floating-point solver/cast to reproduce immutable publisher samples.
    output[covered]=heights[covered]
    before=heights[covered].astype('<f4').tobytes();after=output[covered].tobytes()
    if before!=after:raise AssertionError('Covered source samples changed')
    edges=[]
    for name,y,x,spacing in [('south',0,slice(None),step[0]),('north',-1,slice(None),step[0]),('west',slice(None),0,step[1]),('east',slice(None),-1,step[1])]:
        old=heights[y,x];new=output[y,x]
        edges.append(dict(edge=name,old_max_adjacent_m=float(abs(np.diff(old)).max()),new_max_adjacent_m=float(abs(np.diff(new)).max()),new_max_grade=float(abs(np.diff(new)).max()/spacing)))
    payload=source.read_bytes()[:48]+output.tobytes()
    audit=dict(version=1,method='Physical-space 5-point harmonic Dirichlet inpaint of uncovered source-ground support',
        source_height=str(source.resolve()),source_height_sha256=sha(source),coverage=str(coverage.resolve()),coverage_sha256=sha(coverage),
        output_height=str((args.output/'showcase.height').resolve()),output_height_sha256=sha_bytes(payload),grid=list(heights.shape),
        covered_nodes=int(covered.sum()),uncovered_nodes=int((~covered).sum()),changed_nodes=int((output!=heights).sum()),
        covered_samples_bitwise_exact=before==after,source_covered_samples_sha256=sha_bytes(before),output_covered_samples_sha256=sha_bytes(after),
        maximum_height_change_m=float(abs(output-heights).max()),solver=solver,edges=edges,house_footings=footings,
        note='Staged only. Generated terrain and fallback/coverage must use this same repaired support; no project installation or geometry changes.')
    args.output.mkdir(parents=True)
    with tempfile.NamedTemporaryFile(dir=args.output,prefix='height-',delete=False)as stream:
        temporary=Path(stream.name);stream.write(payload)
    os.replace(temporary,args.output/'showcase.height')
    (args.output/'audit.json').write_text(json.dumps(audit,indent=2,allow_nan=False)+'\n')
    print(json.dumps(audit,indent=2))

if __name__=='__main__':main()
