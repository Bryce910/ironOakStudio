#!/usr/bin/env python3
"""Stage the first Valwood campsite from owned/CC0 assets, without project edits.

The render-triangle terrain and source models are read-only. Output consists of
an art-only HIGH/MID compound mesh, roast chicken, exact world-space sockets,
individual collision suggestions and clearing bounds for the integrator.
"""
from pathlib import Path
import argparse
import copy
import hashlib
import json
import math
import sys

import numpy as np

from build_opening_prologue import Glb, furniture, read_glb, array
from build_castle_expansion_assets import Mesh as KitMesh
from build_city_countryside import readfield, sample
from countryside_contact import TerrainSurfaces

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PROJECT = ROOT/'build/opening-finward-reference/v14/project'
OUT = ROOT/'docs/arrival-camp/2026-11-04/art'
SOURCE = ROOT/'projects/valwood_world/models'
MAPS = ROOT/'assets/polyhaven/castle_village'
CENTER = np.array([-1486.37664868499, 18.75, -10224.9106110642])


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def terrain(project):
    metadata = json.loads((project/'city_showcase.json').read_text())
    catalog = Path(metadata['active_geometry_catalog'])
    data = json.loads(catalog.read_text())
    grid, origin, step = readfield(project/'ground/countryside.height')
    return TerrainSurfaces(data, metadata['origin'], lambda q: sample(grid, q, origin, step)), catalog


class Mesh(KitMesh):
    def __init__(self, lod):
        super().__init__()
        self.lod = lod
        self.parts = {name: [[], [], [], []] for name in ['Wood', 'DarkWood', 'Canvas', 'Rope', 'Ember']}

    def round_beam(self, mat, a, b, radius=.04, segments=None):
        # Structural pole silhouettes remain identical in both LODs. Most of
        # the MID saving comes from the source props, not a six-sided ridge.
        segments = segments or 12
        a, b = np.asarray(a, float), np.asarray(b, float)
        axis = b-a
        axis /= np.linalg.norm(axis)
        u = np.cross(axis, [0, 1, 0] if abs(axis[1]) < .9 else [1, 0, 0])
        u /= np.linalg.norm(u)
        v = np.cross(axis, u)
        rings = [[c + radius*(u*math.cos(i*math.tau/segments)+v*math.sin(i*math.tau/segments))
                  for i in range(segments)] for c in [a, b]]
        for i in range(segments):
            j = (i+1) % segments
            self.face(mat, [rings[0][i], rings[0][j], rings[1][j], rings[1][i]],
                      rings[0][i]-a)
        self.face(mat, rings[0][::-1], -axis)
        self.face(mat, rings[1], axis)

    def translate(self, delta):
        for positions, _, _, _ in self.parts.values():
            for i, p in enumerate(positions):
                positions[i] = (np.asarray(p)+delta).tolist()


def append_mesh(g, mesh):
    mats = {}
    mats['Wood'] = g.material('Weathered camp timber', [.63, .49, .32], .96,
                              base=MAPS/'wood_base.png', normal=MAPS/'wood_normal.png', arm=MAPS/'wood_arm.png')
    mats['DarkWood'] = g.material('Charred camp timber', [.15, .11, .075], 1,
                                  base=MAPS/'wood_base.png', normal=MAPS/'wood_normal.png', arm=MAPS/'wood_arm.png')
    mats['Canvas'] = g.material('Worn oat canvas', [.52, .43, .29], 1, base=MAPS/'cloth_base.png')
    mats['Rope'] = g.material('Hemp guy rope', [.27, .22, .14], 1, base=MAPS/'cloth_base.png')
    mats['Ember'] = g.material('Dim live embers', [.035, .012, .003], 1, emissive=[.35, .035, .003])
    for name, (p, n, uv, ix) in mesh.parts.items():
        if ix:
            g.mesh(p, n, uv, ix, mats[name])


def local_height(t, x, z):
    return float(t.sample(np.array([[CENTER[0]+x, CENTER[2]+z]]))[0])-CENTER[1]


def make(project, out):
    out.mkdir(parents=True, exist_ok=True)
    models = out/'models'
    models.mkdir(exist_ok=True)
    t, catalog_path = terrain(project)
    ground = lambda x, z: local_height(t, x, z)
    fire_y = ground(0, 0)
    manifest = dict(schema_version=1, purpose='First arrival in the Meadows of Valois: three friendly people around a cooking fire',
                    source_project=str(project), terrain_catalog=str(catalog_path), terrain_catalog_sha256=sha(catalog_path),
                    origin_world=CENTER.tolist(), source_files={}, models=[], art_records=[], physics=[],
                    integration_notes=['Art candidate only: this helper never edits world.studio, actor/story files, saves or source models.',
                                       'Cull or move groundcover intersecting the clearing, tent and connection lane before visual acceptance.',
                                       'Static compound mesh must use collision None; use only individual measured collider suggestions.',
                                       'Fire VFX/light are runtime emitters at the provided socket, not a painted flame plane.'])
    art_records = manifest['art_records']
    colliders = manifest['physics']
    for lod in ['near', 'mid']:
        g = Glb()
        mesh = Mesh(lod)

        def prop(label, source, x, z, width=None, height=None, y=None, yaw=0, y_scale=1):
            path = SOURCE/source
            rec = furniture(g, path, x, z, width=width, height=height,
                            y=ground(x, z) if y is None else y, yaw=yaw, roughness=.86, y_scale=y_scale)
            if lod == 'near':
                rec.update(id=label, ground_world_y=CENTER[1]+ground(x, z))
                rec['source'] = str(path.relative_to(ROOT))
                art_records.append(rec)
                manifest['source_files'][rec['source']] = sha(path)
            return rec

        # The scanned stone ring is set 25mm into the actual rendered soil.
        pit = prop('stone_cooking_ring', 'starting_firepit.glb', 0, 0, width=1.45, y=fire_y-.025)
        if lod == 'near':
            colliders.append(dict(id='firepit', type='cylinder', center_world=[CENTER[0], CENTER[1]+fire_y+.18, CENTER[2]],
                                  radius=.72, height=.43))
        # Fuel crosses under the roast. Short irregular round logs avoid the
        # giant woodpile silhouette and keep the flame socket visibly in fuel.
        for i, angle in enumerate([-.40, .55, 1.63, 2.48]):
            y=fire_y+.12+.026*(i % 2)
            d=np.array([math.cos(angle), 0, math.sin(angle)])
            mesh.round_beam('DarkWood', -d*.47+[0,y,0], d*.47+[0,y+.018,0], .075)
        for i in range(11 if lod == 'near' else 6):
            a=i*2.39996;r=.1+.036*(i % 5);x,z=math.cos(a)*r,math.sin(a)*r
            mesh.sphere('Ember', [x,fire_y+.17,z], [.035,.018,.026], 8, 4)

        # Two forked uprights, resting across the actual ground, carry a real
        # horizontal spit with a crank and off-centre counterweight.
        axis_y=fire_y+.98
        for sign in [-1,1]:
            x=sign*.94;gy=ground(x,0)
            mesh.round_beam('Wood',[x,gy-.08,0],[x,axis_y+.04,0],.052)
            for dz in [-.12,.12]:
                mesh.round_beam('Wood',[x,axis_y-.12,0],[x,axis_y+.14,dz],.032)
            if lod == 'near':
                colliders.append(dict(id=f'spit_post_{sign}',type='box',center_world=(CENTER+[x,(gy+axis_y)/2,0]).tolist(),
                                      half_extents=[.075,(axis_y-gy)/2+.12,.075]))
        mesh.round_beam('DarkWood',[-1.11,axis_y,0],[1.18,axis_y,0],.025)
        mesh.round_beam('Wood',[1.18,axis_y,0],[1.18,axis_y-.17,0],.031)
        mesh.round_beam('Wood',[1.18,axis_y-.17,0],[1.39,axis_y-.17,0],.038)

        # Three real log stools with worn inset seats, arranged around the
        # hearth while preserving a clear southern landing/approach opening.
        stools=[('west_seat',-2.1,.2),('east_seat',2.15,.25),('north_seat',.0,-2.65)]
        for label,x,z in stools:
            gy=ground(x,z)
            mesh.lathe('DarkWood',[(gy-.035,.20),(gy+.04,.24),(gy+.37,.215),(gy+.41,.23)],(x,0,z),16 if lod=='near'else 8)
            mesh.lathe('Wood',[(gy+.41,.23),(gy+.438,.25),(gy+.447,0)],(x,0,z),16 if lod=='near'else 8)
            if lod=='near':
                colliders.append(dict(id=label,type='cylinder',center_world=(CENTER+[x,gy+.22,z]).tolist(),radius=.25,height=.46))

        # Small open A-frame shelter: actual inside and outside faces, soft
        # irregular eaves, ridge seam and guy ropes. No opaque entrance card.
        tx,tz=-3.55,-4.1
        tent_y=ground(tx,tz)
        half_w,half_d,ridge=1.7,1.45,tent_y+1.83
        for z in [-half_d,half_d]:
            for x in [-half_w,half_w]:
                xx,zz=tx+x,tz+z
                mesh.round_beam('Wood',[xx,ground(xx,zz)-.045,zz],[tx,ridge,zz],.035)
        mesh.round_beam('Wood',[tx,ridge,tz-half_d-.10],[tx,ridge,tz+half_d+.1],.045)
        mesh.round_beam('Rope',[tx,ridge+.025,tz-half_d],[tx,ridge+.025,tz+half_d],.018,8)
        # These48 roof triangles carry the silhouette; retain them in MID so
        # the canopy bounds and ridge folds remain identical on LOD changes.
        slices=12
        for sign in [-1,1]:
            for i in range(slices):
                a,b=i/slices,(i+1)/slices
                def point(u,z):
                    x=tx+sign*half_w*u
                    edge=ground(tx+sign*half_w,tz+z)+.04
                    y=ridge*(1-u)+edge*u-.06*math.sin(math.pi*u)+.012*math.sin(u*31+z*2)
                    return [x,y,tz+z]
                q=[point(a,-half_d),point(b,-half_d),point(b,half_d),point(a,half_d)]
                mesh.face('Canvas',q,[sign,1,0],uv=[[a*1.7,0],[b*1.7,0],[b*1.7,2.9],[a*1.7,2.9]])
                mesh.face('Canvas',[[x,y-.008,z]for x,y,z in q[::-1]],[-sign,-1,0],
                          uv=[[a*1.7,2.9],[b*1.7,2.9],[b*1.7,0],[a*1.7,0]])
            for z in [-half_d,half_d]:
                edge=[tx+sign*half_w,ground(tx+sign*half_w,tz+z)+.04,tz+z]
                peg=[tx+sign*(half_w+.65),ground(tx+sign*(half_w+.65),tz+z+.12)+.04,tz+z+.12]
                mesh.round_beam('Rope',edge,peg,.010,6)
                mesh.round_beam('Wood',[peg[0],peg[1]-.09,peg[2]],[peg[0]+.04,peg[1]+.10,peg[2]],.022,6)
        # A rear panel only: the whole southern face remains open and readable.
        q=[[tx-half_w,ground(tx-half_w,tz-half_d)+.04,tz-half_d],
           [tx+half_w,ground(tx+half_w,tz-half_d)+.04,tz-half_d],[tx,ridge,tz-half_d]]
        mesh.face('Canvas',q,[0,0,-1]);mesh.face('Canvas',q[::-1],[0,0,1])
        for i,x in enumerate([tx-.92,tx,tx+.92]):
            prop(f'bedroll_{i+1}',f'castle_residents_bedroll_{lod}.glb',x,tz+.10,width=.70,y=ground(x,tz+.10)+.008)

        # A restrained provisioning cluster beside the shelter, rather than
        # filling the landing with crates. All measured tabletop food contact
        # uses its support's returned upper bound.
        crate_source='purchased_village_crate_'+lod+'.glb'
        crate=prop('provision_crate',crate_source,2.9,-3.5,width=.78,yaw=-13)
        if lod=='near':
            lo,hi=np.asarray(crate['minimum']),np.asarray(crate['maximum'])
            colliders.append(dict(id='provision_crate',type='box',center_world=(CENTER+(lo+hi)/2).tolist(),
                                  half_extents=((hi-lo)/2).tolist()))
        top=crate['maximum'][1]
        for label,stem,x,z,w in [('bread','purchased_goods_bread',2.76,-3.55,.31),
                               ('cheese','purchased_goods_cheese',3.08,-3.45,.24),
                               ('cup','purchased_goods_tankard',2.96,-3.73,.12)]:
            prop(label,stem+'_'+lod+'.glb',x,z,width=w,y=top+.007,yaw=18)
        prop('water_bucket','starting_bucket.glb',3.45,-2.72,height=.46)
        prop('fuel_stack','purchased_prop_logs_'+lod+'.glb',-2.7,-1.9,width=.38,yaw=77)
        # One tiny datum triangle fixes HIGH/MID model grounding at the explicit
        # root. It has zero area and adds no visible floor or collision surface.
        datum=g.material('Invisible grounding datum',[.1,.1,.1],1)
        g.mesh([[0,0,0]]*3,[[0,1,0]]*3,[[0,0]]*3,[0,1,2],datum)
        append_mesh(g,mesh)
        g.doc['asset']['generator']='Valwood first-arrival campsite art 1'
        filename=models/f'arrival_camp_{lod}.glb'
        filename.write_bytes(g.finish())
        manifest['models'].append(inspect(filename,lod))

    # Source-faithful roast. Its exact midpoint, not the model's minimum Y,
    # is the axis of rotation. Runtime should rotate around pivot_local_m.
    roast_bounds = None
    for lod in ['near','mid']:
        g=Glb();src=SOURCE/f'purchased_goods_chicken_{lod}.glb'
        rec=furniture(g,src,0,0,width=.52,y=0,yaw=0,roughness=.80)
        # Both LODs rotate about exactly the same axis and retain the HIGH
        # envelope. The source MID differs by5mm in height; correct that small
        # reduction drift rather than allowing the bird to hop on each switch.
        if roast_bounds is not None:
            lo,hi=np.asarray(rec['minimum']),np.asarray(rec['maximum'])
            target_lo,target_hi=roast_bounds
            scale=(target_hi-target_lo)/(hi-lo)
            for material,parts in g.geometry.items():
                g.geometry[material]=[((pos-lo)*scale+target_lo,
                    (normal/scale)/np.linalg.norm(normal/scale,axis=1,keepdims=True),uv,ix)
                    for pos,normal,uv,ix in parts]
        else:
            roast_bounds=(np.asarray(rec['minimum']),np.asarray(rec['maximum']))
        g.doc['asset']['generator']='Valwood centered source roast chicken 1'
        filename=models/f'arrival_roast_{lod}.glb';filename.write_bytes(g.finish())
        row=inspect(filename,lod);manifest['models'].append(row)
        manifest['source_files'][str(src.relative_to(ROOT))]=sha(src)
        if lod=='near':
            pivot=[0,(rec['maximum'][1]+rec['minimum'][1])/2,0]
            manifest['roast']=dict(source=str(src.relative_to(ROOT)),pivot_local_m=pivot,
                 rotation_axis_local=[1,0,0],center_world_m=(CENTER+[0,axis_y,0]).tolist(),
                 instance_bottom_world_y=float(CENTER[1]+axis_y-pivot[1]),
                 suggested_rotation_radians_per_second=.12,rotation_optional=True)

    manifest['fire']=dict(id='arrival_cooking_fire',world_position=(CENTER+[0,fire_y+.145,0]).tolist(),
                         height_m=.72,width_m=.23,light_radius_m=5.5,light_rgb=[1,.39,.10],light_intensity=34,
                         notes='Live flame/sparks and quiet crackle; no smoke obscuring the three faces.')
    manifest['landing']=dict(world_feet=(CENTER+[0,ground(0,5.2),5.2]).tolist(),yaw_radians=0,pitch_radians=-.04,
                             clear_radius_m=1.8,eye_height_m=1.75)
    manifest['actors']=[dict(id=id,local_xz=[x,z],world_feet=(CENTER+[x,ground(x,z),z]).tolist(),
                             yaw_radians=math.atan2(-x,z),suggested_existing_family=family)
                        for id,x,z,family in [('arrival_guide',-2.15,-.65,'traveler'),
                                               ('arrival_provisioner',2.2,-.65,'traderwoman'),
                                               ('arrival_cook',.1,-3.2,'trader')]]
    # Return to the already accepted main path at its previous starting point.
    points=np.array([[0,5.2],[3.4,5.3],[6.1,1.7],[9,-3.8],[12.5,-9.5],[16,-14.]])
    world=points+CENTER[[0,2]];heights=t.sample(world)
    manifest['connection_world']=[[float(x),float(y),float(z)]for (x,z),y in zip(world,heights)]
    manifest['clearing']=dict(world_xz=CENTER[[0,2]].tolist(),radii_xz=[7.3,7.7],
                              path_half_width_m=1.45,feather_m=1.2,height_edits=False,
                              groundcover_note='Remove grass/clover/fern roots inside clearing, and within lane half-width plus model footprint.')
    probe=np.array([[x,z]for x in np.arange(-5,5.01,.5)for z in np.arange(-5,5.01,.5)])+CENTER[[0,2]]
    h=t.sample(probe)
    manifest['terrain_review']=dict(center_ground_world_y=CENTER[1]+fire_y,clearing_center_10m_range=[float(h.min()),float(h.max())],
                                     fallback_queries=t.fallback_queries,source_geometry_unchanged=True)
    manifest['source_files'].update({str(p.relative_to(ROOT)):sha(p)for p in [MAPS/'wood_base.png',MAPS/'wood_normal.png',MAPS/'wood_arm.png',MAPS/'cloth_base.png']})
    manifest['recipe_sha256']=sha(__file__)
    manifest['new_unique_texture_budget_note']='Existing owned props are retiered to512px by the established importer; canvas/wood are previously downloaded CC0 maps. No internet/paid asset required.'
    (out/'ART_CANDIDATE.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest


def inspect(path,lod):
    d,b=read_glb(path)
    prim=[p for m in d['meshes']for p in m['primitives']]
    xyz=np.concatenate([array(d,b,p['attributes']['POSITION'])for p in prim])
    assert np.isfinite(xyz).all()
    return dict(path=str(path.resolve()),lod=lod,sha256=sha(path),bytes=path.stat().st_size,
                bounds=[xyz.min(0).tolist(),xyz.max(0).tolist()],triangles=sum(d['accessors'][p['indices']]['count']//3 for p in prim),
                materials=len(d['materials']),images=len(d.get('images',[])),min_y_is_zero=bool(xyz[:,1].min()==0))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project',type=Path,default=DEFAULT_PROJECT)
    parser.add_argument('--output',type=Path,default=OUT)
    args=parser.parse_args();m=make(args.project,args.output)
    print(json.dumps(dict(passed=True,manifest=str(args.output/'ART_CANDIDATE.json'),
                          models=m['models'],landing=m['landing'],fire=m['fire'],actors=m['actors']),indent=2))


if __name__=='__main__':
    main()
