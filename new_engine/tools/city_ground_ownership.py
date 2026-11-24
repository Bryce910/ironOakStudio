"""Exact projected paving ownership for offline town terrain generation."""
from pathlib import Path
import sys
import numpy as np
try:
    import shapely
except ImportError:
    # Optional build dependency; the native game does not import Python.
    sys.path.insert(0,'/tmp/valwood-town-geometry')
    import shapely
from build_village_showcase_project import glb_geometry,matrix


class CityGroundOwnership:
    def __init__(self,catalog,footprint_cache=None):
        if footprint_cache and Path(footprint_cache).is_file():
            self.polygon=shapely.from_wkb(Path(footprint_cache).read_bytes())
            if not self.polygon.is_valid:raise ValueError('Invalid cached source footprint')
            shapely.prepare(self.polygon);return
        asset=next(a for a in catalog['assets']if a['name']==catalog['ground_asset'])
        vertices,triangles=glb_geometry(asset['files'][0]);pieces=[]
        for placement in catalog['placements']:
            if placement['asset']!=asset['name']:continue
            m=matrix(placement['matrix_native']);m[:3,3]+=m[:3,:3]@np.asarray(asset['pivot_offset_native'])
            v=vertices@m[:3,:3].T+m[:3,3];q=v[triangles][:,:,[0,2]]
            area=np.cross(q[:,1]-q[:,0],q[:,2]-q[:,0]);q=q[abs(area)>1e-10]
            pieces.append(shapely.polygons(q))
        self.polygon=shapely.union_all(np.concatenate(pieces))
        if self.polygon.is_empty or not self.polygon.is_valid:raise ValueError('Invalid projected source ground')
        shapely.prepare(self.polygon)

    def clip(self,points,triangles):
        """Subtract source paving; preserve every untouched terrain triangle."""
        polys=shapely.polygons(points[triangles])
        intersects=shapely.intersects(self.polygon,polys)
        covered=shapely.covers(self.polygon,polys)
        result=[triangles[~intersects]];vertices=points.tolist()
        lookup={tuple(np.round(p,9)):i for i,p in enumerate(points)};faces=[]
        for index in np.flatnonzero(intersects&~covered):
            remaining=shapely.difference(polys[index],self.polygon)
            for face in shapely.get_parts(shapely.constrained_delaunay_triangles(remaining)):
                if face.area<1e-12:continue
                q=np.asarray(face.exterior.coords)[:3];ids=[]
                for p in q:
                    key=tuple(np.round(p,9))
                    if key not in lookup:lookup[key]=len(vertices);vertices.append(list(key))
                    ids.append(lookup[key])
                q=np.asarray([vertices[i]for i in ids])
                if np.cross(q[1]-q[0],q[2]-q[0])>0:ids[1],ids[2]=ids[2],ids[1]
                faces.append(ids)
        if faces:result.append(np.asarray(faces,dtype=np.int64))
        return np.asarray(vertices),np.concatenate(result)
