#!/usr/bin/env python3
"""A modest enclosed bathroom for the authored modern opening apartment.

Only adds geometry and static proxies to the caller's private composition.
Owned source meshes are read through the existing furniture importer. The
door is intentionally parked open; this helper does not add interactions.
"""
import math
import numpy as np

from opening_home_set import _rounded_box, _frustum, _tube, _room_switch


def _wall_tiles(g, material, alternate, grout, *, side, fixed, first, last,
                low=.27, high=1.48, tile=.30):
    """Shallow, restrained tiles with actual thin grout seams, inside the room."""
    count=max(1,round((last-first)/tile))
    rows=max(1,round((high-low)/tile))
    width=(last-first)/count;height=(high-low)/rows
    if side=='east':
        g.box([fixed+.006,(low+high)/2,(first+last)/2],
              [.012,high-low,last-first],grout)
    else:
        g.box([(first+last)/2,(low+high)/2,fixed-.006],
              [last-first,high-low,.012],grout)
    for row in range(rows):
        for col in range(count):
            horizontal=first+(col+.5)*width;y=low+(row+.5)*height
            color=alternate if (col*5+row*3)%17==0 else material
            if side=='east':
                g.box([fixed-.002,y,horizontal],[.010,height-.004,width-.004],color)
            else:
                g.box([horizontal,y,fixed+.002],[width-.004,height-.004,.010],color)


def _roll(g, center, radius, length, paper, cardboard):
    """A white toilet roll whose visible end has a real cardboard opening."""
    x,y,z=center;sides=24;positions=[];normals=[];uv=[];indices=[]
    for end in [-1,1]:
        zz=z+end*length/2
        for ring in [.019,radius]:
            for i in range(sides+1):
                a=i*math.tau/sides
                positions.append([x+ring*math.cos(a),y+ring*math.sin(a),zz])
                normals.append([0,0,end]);uv.append([.5+.5*math.cos(a),.5+.5*math.sin(a)])
        start=(0 if end<0 else 2*(sides+1))
        for i in range(sides):
            a=start+i;b=a+1;c=a+sides+1;d=c+1
            face=[a,b,c,b,d,c] if end<0 else [a,c,b,b,c,d]
            indices.extend(face)
    g.mesh(positions,normals,uv,indices,paper)
    _tube(g,[[x,y,z-length/2],[x,y,z+length/2]],radius,paper,sides=24)
    _tube(g,[[x,y,z-length/2-.001],[x,y,z+length/2+.001]],.018,cardboard,sides=20)


def _validate_new_geometry(g, starts):
    total=0;degenerate=0;reversed_faces=0;vertices=0
    for material,parts in g.geometry.items():
        for pos,normals,uv,indices in parts[starts.get(material,0):]:
            if not all(np.isfinite(a).all() for a in [pos,normals,uv]):
                raise ValueError('Bathroom geometry contains a non-finite coordinate')
            indices=np.asarray(indices).reshape(-1,3)
            if indices.size and (indices.min()<0 or indices.max()>=len(pos)):
                raise ValueError('Bathroom geometry contains an invalid vertex index')
            vertices+=len(pos);total+=len(indices)
            tri=np.asarray(pos)[indices];n=np.asarray(normals)[indices].mean(axis=1)
            area=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0])
            active=np.linalg.norm(area,axis=1)>1e-12
            degenerate+=int((~active).sum())
            reversed_faces+=int((np.einsum('ij,ij->i',area,n)[active]<-1e-10).sum())
    if reversed_faces:
        raise ValueError(f'Bathroom has {reversed_faces} triangles facing against their normals')
    return dict(vertices=vertices,triangles=total,degenerate_triangles=degenerate,
                reversed_triangles=reversed_faces,finite=True)


def _repair_imported_winding(g, starts):
    """Align a few tiny owned-source faces to their supplied smooth normals.

    This changes only the newly imported composite indices, never source GLBs.
    The source toilet has one such face and the sink has seven.
    """
    repaired=0
    for material,parts in g.geometry.items():
        for j in range(starts.get(material,0),len(parts)):
            pos,normals,uv,indices=parts[j]
            triangles=np.asarray(indices).reshape(-1,3).copy()
            p=np.asarray(pos)[triangles]
            n=np.asarray(normals)[triangles].mean(axis=1)
            bad=np.einsum('ij,ij->i',np.cross(p[:,1]-p[:,0],p[:,2]-p[:,0]),n)<-1e-10
            triangles[bad]=triangles[bad][:,[0,2,1]]
            repaired+=int(bad.sum())
            parts[j]=(pos,normals,uv,triangles.reshape(-1))
    return repaired


def _clean_fixture(g, starts, name, ceramic, metal):
    """Clean only the composed copy of two baked single-atlas fixtures.

    The source atlases contain pink lighting and smeared wood highlights. A
    toilet should receive the room's current light, not retain that baked light.
    Keep its full authored silhouette. Retain the sink's actual bowl and tap,
    while replacing its skewed lower cabinet with a small straight cabinet.
    This does not touch the source meshes or the already audited collision box.
    """
    pieces=[];source_triangles=0;kept_triangles=0
    for material,parts in list(g.geometry.items()):
        first=starts.get(material,0)
        for pos,normals,uv,indices in parts[first:]:
            triangles=np.asarray(indices).reshape(-1,3)
            source_triangles+=len(triangles)
            centers=np.asarray(pos)[triangles].mean(axis=1)
            if name=='clean_toilet':
                groups=[(np.ones(len(triangles),dtype=bool),ceramic)]
            else:
                basin=centers[:,1]>=.851
                tap=basin&(centers[:,1]>.946)&(abs(centers[:,2]-1.13)<.055)
                groups=[(basin&~tap,ceramic),(tap,metal)]
            for mask,target in groups:
                selected=triangles[mask]
                if not len(selected):continue
                used,reindexed=np.unique(selected.reshape(-1),return_inverse=True)
                pieces.append((np.asarray(pos)[used],np.asarray(normals)[used],
                               np.asarray(uv)[used],reindexed,target))
                kept_triangles+=len(selected)
        del parts[first:]
        if not parts:
            del g.geometry[material]
    for pos,normals,uv,indices,material in pieces:
        g.mesh(pos,normals,uv,indices,material)
    return dict(source_triangles=source_triangles,retained_triangles=kept_triangles,
                discarded_lower_cabinet_triangles=source_triangles-kept_triangles,
                original_source_unchanged=True,original_collision_unchanged=True)


def _draped_towel(g, cloth, stitching):
    """One continuous cloth sheet folded over a rail, with a soft hanging hem."""
    # A smooth bent profile joins the short back flap to the long visible face.
    profile=[]
    profile.extend([[5.248,y] for y in np.linspace(1.22,1.335,5)])
    for a in np.linspace(0,math.pi,13)[1:]:
        profile.append([5.224+.024*math.cos(a),1.335+.026*math.sin(a)])
    profile.extend([[5.200-(1.335-y)*.025,y] for y in np.linspace(1.32,.925,16)])
    positions=[];normals=[];uv=[];indices=[];width=.296;columns=32
    profile=np.asarray(profile)
    for j,(x,y) in enumerate(profile):
        tangent=profile[min(j+1,len(profile)-1)]-profile[max(j-1,0)]
        tangent=np.array([tangent[0],tangent[1],0.])
        for i in range(columns+1):
            u=i/columns;phase=u*math.tau*3.2
            fold=.007*math.cos(phase)
            # Small irregular folds and a slightly curved lower hem, without
            # the disconnected hard slats used by the earlier placeholder.
            yy=y+(.0025*math.cos(phase) if j==len(profile)-1 else 0)
            positions.append([x+fold,yy,.432+u*width])
            across=np.array([-.007*math.sin(phase)*math.tau*3.2,0,width])
            normal=np.cross(tangent,across);normal/=np.linalg.norm(normal)
            normals.append(normal);uv.append([u*2,j/(len(profile)-1)*3])
    for j in range(len(profile)-1):
        for i in range(columns):
            a=j*(columns+1)+i;b=a+1;c=a+columns+1;d=c+1
            indices.extend([a,c,b,b,c,d])
    g.mesh(positions,normals,uv,indices,cloth)
    points=[]
    for u in np.linspace(0,1,33):
        phase=u*math.tau*3.2
        points.append([5.188+.007*math.cos(phase),.933+.0025*math.cos(phase),.432+u*width])
    _tube(g,points,.0014,stitching,sides=6)


def _porcelain_basin_insert(g, porcelain, metal):
    """An intact shallow bowl above the cabinet and below the owned sink rim.

    The source material split keeps the upper rim but loses its deepest bowl
    faces. This small rounded liner closes that opening with actual recessed
    porcelain, rather than exposing the green cabinet through the basin.
    All vertices stay within the existing sink's audited bounds.
    """
    def ring(half_x,half_z,radius,y):
        points=[]
        for sx,sz,first in [(1,1,0),(-1,1,90),(-1,-1,180),(1,-1,270)]:
            for angle in np.linspace(first,first+90,9)[:-1]:
                a=math.radians(angle)
                points.append([5.145+sx*(half_x-radius)+radius*math.cos(a),
                               y,1.13+sz*(half_z-radius)+radius*math.sin(a)])
        return np.asarray(points)
    upper=ring(.150,.225,.032,.903)
    lower=ring(.099,.157,.038,.858)
    for i in range(len(upper)):
        j=(i+1)%len(upper)
        points=np.array([upper[i],upper[j],lower[j],lower[i]])
        normal=np.cross(points[1]-points[0],points[2]-points[0])
        normal/=np.linalg.norm(normal)
        if normal[1]<0:points=points[::-1];normal=-normal
        g.mesh(points,[normal]*4,[[0,0],[1,0],[1,1],[0,1]],
               [0,1,2,0,2,3],porcelain)
    positions=np.vstack([[5.145,.858,1.13],lower]);indices=[]
    for i in range(len(lower)):
        a=i+1;b=(i+1)%len(lower)+1
        if np.cross(positions[a]-positions[0],positions[b]-positions[0])[1]<0:a,b=b,a
        indices.extend([0,a,b])
    g.mesh(positions,[[0,1,0]]*len(positions),
           [[(p[0]-4.995)/.30,(p[2]-.905)/.45]for p in positions],indices,porcelain)
    _frustum(g,[5.145,.861,1.13],.018,.018,.004,metal,sides=24)


def dress_bathroom(g, furniture, source, *, boxes, floor=.18):
    """Build a compact bathroom in x[3.8,5.4], z[-1.4,1.7].

    The east and rear walls already exist in the apartment shell. The new west
    wall has a 1.05 m structural opening, .97 m after trim, centered at z.625.
    At the toilet the remaining aisle is .903 m wide. All dimensions are in m.
    """
    starts={material:len(parts) for material,parts in g.geometry.items()}
    first_box=len(boxes)
    wall=g.material('Bathroom | warm painted plaster',[.69,.70,.66],.96)
    trim=g.material('Bathroom | satin white door casing',[.74,.75,.70],.84)
    tile=g.material('Bathroom | matte warm white porcelain',[.60,.64,.62],.92)
    alternate=g.material('Bathroom | quiet porcelain variation',[.57,.61,.59],.93)
    grout=g.material('Bathroom | fine pale grey grout',[.36,.40,.38],1.)
    floor_tile=g.material('Bathroom | dry stone grey non-slip tile',[.29,.34,.33],.98)
    floor_alternate=g.material('Bathroom | dry tile restrained variation',[.31,.35,.34],.98)
    ceramic=g.material('Bathroom | shower tray white enamel',[.75,.77,.72],.63)
    porcelain=g.material('Bathroom | clean neutral porcelain fixtures',[.72,.74,.71],.48)
    vanity=g.material('Bathroom | matte sage painted vanity',[.20,.29,.25],.92)
    dark=g.material('Bathroom | muted dark hardware',[.065,.083,.083],.78)
    metal=g.material('Bathroom | brushed nickel shower hardware',[.37,.42,.42],.43)
    g.doc['materials'][metal]['pbrMetallicRoughness']['metallicFactor']=.72
    mirror=g.material('Bathroom | smoke silver mirror',[.30,.36,.36],.12)
    g.doc['materials'][mirror]['pbrMetallicRoughness']['metallicFactor']=.82
    cloth=g.material('Bathroom | soft muted blue cotton towel',[.24,.36,.39],.99,
                     base=source.parents[1]/'assets/polyhaven/castle_village/cloth_base.png')
    stitch=g.material('Bathroom | subtle cotton towel hem',[.27,.39,.41],1.)
    paper=g.material('Bathroom | unbleached white tissue',[.78,.78,.71],1.)
    cardboard=g.material('Bathroom | tissue cardboard core',[.32,.27,.20],1.)
    soap=g.material('Bathroom | muted amber soap dispenser',[.41,.28,.13],.65)
    glow=g.material('Bathroom | warm neutral practical diffuser',[.85,.82,.69],.55,
                    emissive=[.74,.68,.53])

    def solid(name,center,size,mat):
        g.box(center,size,mat)
        boxes.append((name,center,size))

    # Two real opaque partitions; no visual-only room divider or open facade.
    solid('Bathroom west wall rear',[3.72,1.68,-.70],[.12,3.,1.60],wall)
    solid('Bathroom west wall front',[3.72,1.68,1.465],[.12,3.,.63],wall)
    solid('Bathroom front wall',[4.58,1.68,1.78],[1.84,3.,.12],wall)
    solid('Bathroom doorway lintel',[3.72,2.88,.625],[.12,.60,1.05],wall)
    # Door frame has an honest .97 m clear opening; trim has a proxy as well.
    for z in [.12,1.13]:
        solid('Bathroom door casing '+str(z),[3.708,1.38,z],[.175,2.40,.04],trim)
    g.box([3.708,2.60,.625],[.175,.08,1.09],trim)
    # Fully open 95 cm leaf, resting along the outside of the rear west wall.
    _rounded_box(g,[3.60,1.385,-.375],[.045,2.37,.95],trim,.009)
    boxes.append(('Bathroom door parked open',[3.60,1.385,-.375],[.045,2.37,.95]))
    for y,sy in [(.75,.91),(1.91,.92)]:
        _rounded_box(g,[3.573,y,-.375],[.016,sy,.77],wall,.006)
    for y in [.47,1.40,2.32]:
        _tube(g,[[3.61,y-.035,.09],[3.61,y+.035,.09]],.013,metal,sides=12)
    _rounded_box(g,[3.559,1.27,-.72],[.014,.15,.045],metal,.005)
    _tube(g,[[3.548,1.30,-.72],[3.52,1.30,-.72],[3.52,1.30,-.61]],.010,metal)

    # Flush matte floor inset: visual height is only 1 cm above the shared floor.
    # It deliberately adds no second floor body or collision lip at the doorway.
    g.box([4.60,floor+.003,.15],[1.60,.006,3.10],grout)
    for i in range(5):
        for j in range(10):
            g.box([4.01+i*.30,floor+.0065,-1.20+j*.30],
                  [.295,.007,.295],floor_alternate if (i+2*j)%13==0 else floor_tile)
    # Matte, thin lower-wall tile; the shower receives full-height splash tile.
    _wall_tiles(g,tile,alternate,grout,side='east',fixed=5.382,first=-1.37,last=1.68,
                low=.27,high=1.48)
    _wall_tiles(g,tile,alternate,grout,side='rear',fixed=-1.408,first=3.80,last=5.39,
                low=.27,high=2.51)
    _wall_tiles(g,tile,alternate,grout,side='east',fixed=5.369,first=-1.37,last=-.55,
                low=1.48,high=2.51)
    for z in [-1.385,1.705]:
        g.box([4.60,.255,z],[1.60,.14,.025],trim)
    for z0,z1 in [(-1.38,.10),(1.15,1.70)]:
        g.box([3.798,.255,(z0+z1)/2],[.025,.14,z1-z0],trim)

    # Existing clean ceramic toilet and wooden vanity, at true household scale.
    # Both source fronts are +Z; yaw -90 rotates the fronts into the west aisle.
    # The toilet projects only to x4.714, leaving >.90 m of clear room width.
    items=[]
    for name,x,z,height in [('clean_toilet',5.03696,.02,.85),
                             ('sink',5.150996,1.13,.88)]:
        imported_starts={material:len(parts) for material,parts in g.geometry.items()}
        item=furniture(g,source/(name+'.glb'),x,z,height=height,y=floor,
                       yaw=-90,roughness=.76 if name=='clean_toilet' else .84)
        item['composite_winding_repairs']=_repair_imported_winding(g,imported_starts)
        item['private_material_cleanup']=_clean_fixture(g,imported_starts,name,porcelain,metal)
        items.append(item)
        lo=np.array(item['minimum']);hi=np.array(item['maximum'])
        # Tight bounds here preserve the measured .91 m aisle, without large
        # simplification margins that would let the player enter the fixtures.
        boxes.append(('Bathroom '+name,(lo+hi)/2,hi-lo))

    # Straight, gently bevelled cabinet fronts replace the source's wavy baked
    # wood body. Every new face stays inside the existing vanity proxy.
    _rounded_box(g,[5.165,.515,1.13],[.385,.67,.526],vanity,.009)
    _rounded_box(g,[5.160,.220,1.13],[.34,.08,.47],dark,.006)
    for yy in [.383,.683]:
        _rounded_box(g,[4.963,yy,1.13],[.025,.286,.505],vanity,.008)
        _rounded_box(g,[4.9505,yy+.087,1.13],[.015,.017,.32],metal,.005)
    _porcelain_basin_insert(g,porcelain,metal)
    # A small actual two-button flush plate survives the neutral ceramic pass.
    _frustum(g,[5.292,1.032,.020],.027,.027,.006,metal,sides=24)
    g.box([5.292,1.036,.020],[.0015,.001,.045],dark)

    # A shallow framed silver mirror and a shaded light over the vanity.
    _rounded_box(g,[5.361,1.90,1.13],[.041,.77,.60],dark,.014)
    _rounded_box(g,[5.334,1.90,1.13],[.015,.713,.543],mirror,.008)
    _rounded_box(g,[5.274,2.35,1.13],[.13,.055,.58],dark,.012)
    _rounded_box(g,[5.263,2.316,1.13],[.10,.017,.52],glow,.006)
    # Soap has a separate pump rather than a generic cube on the counter.
    _rounded_box(g,[5.10,1.067,1.32],[.060,.115,.065],soap,.012)
    _tube(g,[[5.10,1.125,1.32],[5.10,1.15,1.32],[5.06,1.15,1.32]],.008,dark,sides=10)

    # The shower has real recessed tray walls and a low, correctly supported
    # kerb. Its low proxy can be stepped over by the normal movement solver.
    _rounded_box(g,[4.60,floor+.029,-.995],[1.49,.058,.79],ceramic,.023)
    for center,size in [([3.88,floor+.068,-.995],[.055,.09,.79]),
                        ([5.32,floor+.068,-.995],[.055,.09,.79]),
                        ([4.60,floor+.068,-1.365],[1.49,.09,.055]),
                        ([4.60,floor+.045,-.54],[1.49,.09,.07])]:
        _rounded_box(g,center,size,ceramic,.012)
    boxes.append(('Bathroom shower tray',[4.60,floor+.029,-.995],[1.49,.058,.79]))
    boxes.append(('Bathroom shower front kerb',[4.60,floor+.045,-.54],[1.49,.09,.07]))
    # Brushed rail, height-adjustable head and a loose hose on the east wall.
    _tube(g,[[5.293,.99,-.97],[5.293,2.41,-.97]],.014,metal,sides=16)
    for y in [1.01,2.38]:
        _tube(g,[[5.37,y,-.97],[5.293,y,-.97]],.020,metal,sides=12)
    _tube(g,[[5.29,2.31,-.97],[5.18,2.36,-.97],[5.05,2.36,-.97]],.020,metal,sides=16)
    _frustum(g,[5.05,2.334,-.97],.085,.070,.040,metal,sides=24)
    _frustum(g,[5.05,2.311,-.97],.068,.068,.006,dark,sides=24)
    _tube(g,[[5.29,2.20,-.97],[5.23,1.78,-.97],[5.10,1.31,-.97],
             [5.10,.99,-.97],[5.19,.83,-.97],[5.31,.92,-.97],
             [5.31,1.15,-.97]],.008,metal,sides=12)
    _rounded_box(g,[5.30,1.17,-.97],[.12,.08,.20],metal,.020)
    _tube(g,[[5.225,1.17,-1.01],[5.20,1.17,-1.01],[5.20,1.22,-1.01]],.011,dark,sides=12)
    _frustum(g,[4.98,floor+.060,-1.18],.055,.055,.006,metal,sides=24)
    for dz in [-.026,0,.026]:
        g.box([4.98,floor+.064,-1.18+dz],[.065,.003,.009],dark)
    # A small wall soap shelf keeps the shower readable without closing it
    # behind a large opaque screen or brittle alpha-sorted glass.
    _rounded_box(g,[5.21,1.64,-1.25],[.27,.035,.24],metal,.008)
    _rounded_box(g,[5.20,1.72,-1.25],[.062,.13,.066],soap,.013)

    # Actual towel, rail and toilet paper are kept on the fixture side, beyond
    # x5.0, so they do not nibble away the capsule's narrowest usable aisle.
    for z in [.41,.75]:
        _tube(g,[[5.365,1.34,z],[5.22,1.34,z]],.012,metal)
    _tube(g,[[5.22,1.34,.41],[5.22,1.34,.75]],.014,metal)
    _draped_towel(g,cloth,stitch)
    _rounded_box(g,[5.363,.87,-.30],[.025,.10,.15],metal,.009)
    _tube(g,[[5.35,.87,-.30],[5.10,.87,-.30]],.011,metal)
    _roll(g,[5.10,.87,-.26],.057,.115,paper,cardboard)
    _rounded_box(g,[5.074,.807,-.249],[.023,.10,.108],paper,.005)

    # A modest ceiling practical and flush ventilation grille complete the room.
    _frustum(g,[4.60,3.155,.26],.165,.165,.045,trim,sides=32)
    _frustum(g,[4.60,3.128,.26],.145,.145,.015,glow,sides=32)
    _rounded_box(g,[4.60,2.88,-1.393],[.31,.25,.025],trim,.009)
    for i in range(5):
        g.box([4.60,2.80+i*.039,-1.376],[.238,.011,.009],grout)
    _room_switch(g,[3.788,1.38,1.43],[1,0,0],trim,wall)

    geometry=_validate_new_geometry(g,starts)
    toilet=items[0]
    return dict(
        furnishings=items,
        authored_groups=['Enclosed plaster partitions and clear framed doorway',
                         'Static fully open panel door',
                         'Matte stone floor and porcelain splash tiles',
                         'Owned toilet and sink bowl with clean porcelain and bevelled painted vanity',
                         'Framed mirror and shaded vanity light',
                         'Shower tray rail head mixer hose drain and soap shelf',
                         'Blue cotton hand towel and toilet paper roll',
                         'Ceiling practical ventilation grille and light switch'],
        collision_names=[name for name,_,_ in boxes[first_box:]],
        room=dict(minimum=[3.80,floor,-1.40],maximum=[5.40,3.18,1.72]),
        doorway=dict(axis='x',plane=3.72,clear_z=[.14,1.11],clear_height=2.40,
                     structural_width=1.05,finished_width=.97,center=[3.72,.625]),
        aisle=dict(west_x=3.8105,east_x=toilet['minimum'][0],
                   clear_width=toilet['minimum'][0]-3.8105),
        walk_path=[[3.,floor,2.45],[3.,floor,.625],[4.22,floor,.625],
                   [4.22,floor,-.10],[4.22,floor,.625],[3.,floor,.625]],
        inspection_camera=dict(eye=[3.17,1.68,.625],target=[5.12,1.10,.10]),
        practical_light=dict(position=[4.35,2.55,.45],color=[1.,.90,.78],radius=3.4),
        validation=geometry,
        policy='Owned local bathroom GLBs and authored geometry only; no purchases, source changes or bathroom interactions.')
