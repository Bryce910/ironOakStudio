#!/usr/bin/env python3
"""Art-directed modern home dressing for the isolated opening builder.

This helper only adds to the supplied GLB accumulator. It reads owned source
furniture through the caller's importer and never changes a source asset.
Coordinates are local to the 11 x 10 m opening shell; floor top is 0.18 m.
"""
from pathlib import Path
import math
import numpy as np

FLOOR = .18
HOME_WINDOW_Z = 3.10


def primary_furnishings():
    """Replacement spec list: (owned source basename, x, z, importer kwargs).

    The original route, coffee table and notebook anchor stay unchanged. The
    sofa's scanned source faces +Z; 180 degrees puts its seat toward the TV.
    Full computer desk already includes an office chair, so no duplicate chair
    is put in front of it. The wardrobe is on the bedroom partition rather than
    intersecting the bed.
    """
    return [
        ('couch', -2.75, 3.85, dict(width=2.7, yaw=180)),
        ('coffee_table', -2.3, 1.35, dict(width=1.55)),
        ('tv_stand', -2.8, -.8, dict(width=2.35)),
        ('full_computer_desk', -3.95, -3.82, dict(width=2.05)),
        ('trash_can', .72, -2.87, dict(height=.56)),
        ('book_shelf', -5., -.15, dict(height=1.95, yaw=90)),
        ('nice_bed', 3.85, -3.65, dict(width=1.8)),
        ('closet', 1.76, -3.48, dict(height=1.8, yaw=90)),
        ('radaitor', 5.03, HOME_WINDOW_Z, dict(height=.68, yaw=90)),
        ('door', 3.65, 5., dict(height=2.36)),
        ('rug', -2.6, 1.65, dict(width=3.4, y=FLOOR+.006, y_scale=.08)),
        ('coffee_cup', -2.65, 1.34, dict(height=.105, y=.82)),
        ('potted_plant', 4.70, 4.35, dict(height=1.2)),
        ('wooden_chair', -.22, -2.25, dict(height=.91)),
    ]


def _rounded_box(g, center, size, material, radius=.035, yaw=0, subdivisions=3):
    """Six rounded box faces; genuine silhouette bevels with smooth normals."""
    half=np.asarray(size,dtype=float)/2
    radius=min(radius,float(half.min())*.95)
    inner=np.maximum(0,half-radius)
    angle=math.radians(yaw);c,s=math.cos(angle),math.sin(angle)
    rot=np.array([[c,0,s],[0,1,0],[-s,0,c]])
    pos=[];normal=[];uv=[];idx=[]
    # Extra cuts near the edge keep narrow furniture bevels from becoming
    # swollen pillows; duplicate UV seams keep the faces independently tiled.
    coords=sorted(set([-1.,1.,0.]+[
        float(v) for v in np.linspace(-1,1,subdivisions+1)]))
    for axis in range(3):
        a=(axis+1)%3;b=(axis+2)%3
        edge_a=min(.49,radius/max(half[a],1e-6));edge_b=min(.49,radius/max(half[b],1e-6))
        us=sorted(set(coords+[-1+edge_a,1-edge_a]));vs=sorted(set(coords+[-1+edge_b,1-edge_b]))
        for sign in [-1,1]:
            start=len(pos)
            for v in vs:
                for u in us:
                    p=np.zeros(3);p[axis]=sign*half[axis];p[a]=u*half[a];p[b]=v*half[b]
                    q=np.minimum(inner,np.maximum(-inner,p));n=p-q;n/=max(np.linalg.norm(n),1e-12)
                    p=q+n*radius
                    pos.append(p@rot.T+center);normal.append(n@rot.T);uv.append(((u+1)/2,(v+1)/2))
            stride=len(us)
            for j in range(len(vs)-1):
                for i in range(len(us)-1):
                    k=start+j*stride+i;face=[k,k+1,k+stride+1,k+stride]
                    if sign<0:face=face[::-1]
                    idx.extend([face[0],face[1],face[2],face[0],face[2],face[3]])
    g.mesh(pos,normal,uv,idx,material)


def _frustum(g, center, bottom, top, height, material, sides=24, cap=True):
    pos=[];normal=[];uv=[];idx=[]
    slope=(bottom-top)/height
    for level,radius in enumerate([bottom,top]):
        for i in range(sides+1):
            a=i*2*math.pi/sides;n=np.array([math.cos(a),slope,math.sin(a)]);n/=np.linalg.norm(n)
            pos.append([center[0]+radius*math.cos(a),center[1]+(level-.5)*height,center[2]+radius*math.sin(a)])
            normal.append(n);uv.append([i/sides,level])
    for i in range(sides):
        k=i;l=i+sides+1;idx.extend([k,l,k+1,k+1,l,l+1])
    if cap:
        for level,radius in enumerate([bottom,top]):
            start=len(pos);y=center[1]+(level-.5)*height
            pos.append([center[0],y,center[2]]);normal.append([0,level*2-1,0]);uv.append([.5,.5])
            for i in range(sides+1):
                a=i*2*math.pi/sides;pos.append([center[0]+radius*math.cos(a),y,center[2]+radius*math.sin(a)])
                normal.append([0,level*2-1,0]);uv.append([.5+.5*math.cos(a),.5+.5*math.sin(a)])
            for i in range(sides):
                if level:idx.extend([start,start+i+2,start+i+1])
                else:idx.extend([start,start+i+1,start+i+2])
    g.mesh(pos,normal,uv,idx,material)


def _curtain(g, z_center, width, material, hem_material):
    """Pleated cloth at the actual east window, kept clear of the walkway."""
    positions=[];normals=[];uv=[];idx=[]
    nx,ny=40,10
    for j in range(ny+1):
        v=j/ny
        for i in range(nx+1):
            u=i/nx;phase=u*math.pi*8
            # A slight gathering toward the tie at half height is enough to
            # give a convincing cloth silhouette rather than a rigid screen.
            gather=1-.16*math.exp(-((v-.47)/.19)**2)
            z=z_center+(u-.5)*width*gather
            x=5.13+.055*math.cos(phase)
            y=.66+v*2.16+.018*math.cos(phase)*(1-v)
            n=np.array([-1.,0.,-.055*math.sin(phase)*math.pi*8/width]);n/=np.linalg.norm(n)
            positions.append([x,y,z]);normals.append(n);uv.append([u*2,v*4])
    for j in range(ny):
        for i in range(nx):
            a=j*(nx+1)+i;b=a+1;c=a+nx+1;d=c+1
            idx.extend([a,b,c,b,d,c])
    g.mesh(positions,normals,uv,idx,material)
    # A softly raised tie, back against the fabric rather than floating in air.
    _rounded_box(g,[5.06,1.66,z_center],[.045,.055,width*.84],hem_material,.018)


def _polygon_indices(points, positions, normal):
    """Ear-clip a small concave print silhouette with outward winding."""
    q=np.asarray(points,dtype=float);p=np.asarray(positions,dtype=float)
    area=sum(q[i,0]*q[(i+1)%len(q),1]-q[(i+1)%len(q),0]*q[i,1]for i in range(len(q)))
    sign=1 if area>0 else -1
    remaining=list(range(len(q)));triangles=[]
    def cross2(a,b):return float(a[0]*b[1]-a[1]*b[0])
    while len(remaining)>3:
        for k,b in enumerate(remaining):
            a=remaining[k-1];c=remaining[(k+1)%len(remaining)]
            if cross2(q[b]-q[a],q[c]-q[b])*sign<=1e-12:continue
            contains=False
            for j in remaining:
                if j in (a,b,c):continue
                if all(cross2(q[v]-q[u],q[j]-q[u])*sign>=-1e-12 for u,v in [(a,b),(b,c),(c,a)]):
                    contains=True;break
            if not contains:
                triangles.append([a,b,c]);del remaining[k];break
        else:raise ValueError('Non-simple print silhouette')
    triangles.append(remaining)
    out=[]
    for a,b,c in triangles:
        if np.dot(np.cross(p[b]-p[a],p[c]-p[a]),normal)<0:b,c=c,b
        out.extend([a,b,c])
    return out


def _picture(g, x, y, z, width, height, mats, side='front', variation=0):
    """A framed, unlettered landscape study: no invented family imagery/lore."""
    frame,mat,paper,far,near=mats
    if side=='front':
        def box(cx,cy,cz,sx,sy,sz,material):g.box([cx,cy,cz],[sx,sy,sz],material)
        box(x,y,z,width,height,.055,frame)
        box(x,y,z-.033,width-.055,height-.055,.014,mat)
        box(x,y,z-.044,width-.12,height-.12,.009,paper)
        def polygon(points,material,depth):
            p=[[x+u*width,y+v*height,z-depth]for u,v in points];indices=_polygon_indices(points,p,[0,0,-1])
            g.mesh(p,[[0,0,-1]]*len(p),[[u+.5,v+.5]for u,v in points],indices,material)
    elif side=='rear':
        def box(cx,cy,cz,sx,sy,sz,material):g.box([cx,cy,cz],[sx,sy,sz],material)
        box(x,y,z,width,height,.055,frame)
        box(x,y,z+.033,width-.055,height-.055,.014,mat)
        box(x,y,z+.044,width-.12,height-.12,.009,paper)
        def polygon(points,material,depth):
            p=[[x+u*width,y+v*height,z+depth]for u,v in points];indices=_polygon_indices(points,p,[0,0,1])
            g.mesh(p,[[0,0,1]]*len(p),[[u+.5,v+.5]for u,v in points],indices,material)
    else:
        def box(cx,cy,cz,sx,sy,sz,material):g.box([cz,cy,cx],[sz,sy,sx],material)
        box(z,y,x,width,height,.055,frame)
        box(z,y,x+.033,width-.055,height-.055,.014,mat)
        box(z,y,x+.044,width-.12,height-.12,.009,paper)
        def polygon(points,material,depth):
            p=[[x+depth,y+v*height,z+u*width]for u,v in points];indices=_polygon_indices(points,p,[1,0,0])
            g.mesh(p,[[1,0,0]]*len(p),[[u+.5,v+.5]for u,v in points],indices,material)
    peak=.08 if variation%2==0 else -.12
    polygon([(-.39,-.35),(-.39,-.05),(-.12,.20+peak),(.13,-.12),(.39,.09),(.39,-.35)],far,.051)
    polygon([(-.39,-.35),(-.39,-.19),(-.13,-.12),(.12,.05),(.39,-.07),(.39,-.35)],near,.053)


def _tube(g, points, radius, material, sides=12, closed=False):
    points=np.asarray(points,dtype=float)
    if closed and np.linalg.norm(points[0]-points[-1])>1e-9:
        points=np.vstack([points,points[0]])
    pos=[];normals=[];uv=[];idx=[]
    for i,point in enumerate(points):
        tangent=points[min(i+1,len(points)-1)]-points[max(i-1,0)]
        tangent/=max(np.linalg.norm(tangent),1e-12)
        axis=np.array([1.,0.,0.]) if abs(tangent[0])<.8 else np.array([0.,1.,0.])
        first=np.cross(tangent,axis);first/=np.linalg.norm(first)
        second=np.cross(tangent,first)
        for j in range(sides+1):
            angle=j*2*math.pi/sides
            n=math.cos(angle)*first+math.sin(angle)*second
            pos.append(point+radius*n);normals.append(n);uv.append([j/sides,i/max(1,len(points)-1)])
    for i in range(len(points)-1):
        for j in range(sides):
            a=i*(sides+1)+j;b=a+sides+1
            idx.extend([a,a+1,b,a+1,b+1,b])
    # Keep a sharply bent small lever's local triangle winding outward too.
    p=np.asarray(pos);n=np.asarray(normals)
    for i in range(0,len(idx),3):
        a,b,c=idx[i:i+3]
        if np.dot(np.cross(p[b]-p[a],p[c]-p[a]),n[[a,b,c]].mean(0))<0:
            idx[i+1],idx[i+2]=idx[i+2],idx[i+1]
    g.mesh(pos,normals,uv,idx,material)


def _room_switch(g, center, normal, plate, rocker):
    """Flush, household-size wall plate at a runtime room-light anchor.

    The local outward normal is explicit so plates on partition side walls
    cannot float in the aisle or put their rocker through the plaster.
    """
    center=np.asarray(center,dtype=float);normal=np.asarray(normal,dtype=float)
    if not (np.isclose(np.linalg.norm(normal),1) and normal[1]==0):
        raise ValueError('A room switch must have a horizontal unit face normal')
    side=bool(abs(normal[0])>.5)
    size=[.016,.14,.09] if side else [.09,.14,.016]
    rocker_size=[.012,.072,.047] if side else [.047,.072,.012]
    _rounded_box(g,center,size,plate,.008)
    _rounded_box(g,center+normal*.012,rocker_size,rocker,.005)


def _kitchen_base(g,paint,cabinet_paint,dark,cream,boxes):
    stone=g.material('Set | honed warm grey worktop',[.40,.40,.36],.90)
    steel=g.material('Set | brushed sink and tap',[.29,.33,.34],.48)
    g.doc['materials'][steel]['pbrMetallicRoughness']['metallicFactor']=.65
    basin=g.material('Set | shaded sink basin',[.17,.21,.22],.56)
    glass=g.material('Set | unlit oven and hob glass',[.035,.048,.047],.42)
    burner=g.material('Set | unlit hob ring',[.14,.16,.15],.70)

    # Three ordinary 0.7 m base units, with a recessed toe space and separately
    # bevelled fronts. Front-facing surfaces point into the room along +Z.
    _rounded_box(g,[-.22,.66,-4.53],[2.10,.80,.62],paint,.012)
    _rounded_box(g,[-.22,.255,-4.58],[2.02,.15,.43],dark,.008)
    for xx in [-.92,-.22]:
        _rounded_box(g,[xx,.65,-4.197],[.666,.73,.035],cabinet_paint,.010)
    # Fine face-frame rails and the middle unit's drawer split are modelled.
    g.box([-.22,.775,-4.172],[.665,.012,.017],dark)
    g.box([-.22,.525,-4.172],[.665,.010,.017],dark)
    for xx,yy in [(-.92,.91),(-.22,.90),(-.22,.65),(-.22,.40)]:
        _rounded_box(g,[xx,yy,-4.146],[.23,.018,.025],dark,.007)

    # Four worktop pieces form a true hole around the sink rather than a black
    # rectangle pasted onto a solid countertop. All pieces share a quiet edge.
    for center,size in [
        ([-1.225,1.08,-4.52],[.11,.04,.68]),
        ([.095,1.08,-4.52],[1.53,.04,.68]),
        ([-.92,1.08,-4.81],[.50,.04,.10]),
        ([-.92,1.08,-4.26],[.50,.04,.16])]:
        _rounded_box(g,center,size,stone,.009)
    # Counter-to-wall upstand stops the tile/surface seam looking unfinished.
    _rounded_box(g,[-.22,1.14,-4.846],[2.14,.12,.026],stone,.006)

    # Bevelled stainless rim and sloping sides with a recessed bowl bottom.
    for center,size in [
        ([-1.166,1.095,-4.55],[.026,.016,.432]),
        ([-.674,1.095,-4.55],[.026,.016,.432]),
        ([-.92,1.095,-4.761],[.515,.016,.026]),
        ([-.92,1.095,-4.339],[.515,.016,.026])]:
        _rounded_box(g,center,size,steel,.006)
    outer=np.array([[-1.153,1.083,-4.748],[-.687,1.083,-4.748],
                    [-.687,1.083,-4.352],[-1.153,1.083,-4.352]])
    inner=np.array([[-1.10,.91,-4.695],[-.74,.91,-4.695],
                    [-.74,.91,-4.405],[-1.10,.91,-4.405]])
    for i in range(4):
        j=(i+1)%4;p=np.array([outer[i],outer[j],inner[j],inner[i]])
        n=np.cross(p[1]-p[0],p[2]-p[0]);n/=np.linalg.norm(n)
        # Surface normal must face up and into the empty bowl.
        if n[1]<0:p=p[::-1];n=-n
        g.mesh(p,[n]*4,[[0,0],[1,0],[1,1],[0,1]],[0,1,2,0,2,3],steel)
    _rounded_box(g,[-.92,.905,-4.55],[.36,.012,.29],basin,.012)
    _frustum(g,[-.92,.914,-4.55],.031,.031,.006,dark,sides=16)
    _tube(g,[[-.92,1.10,-4.79],[-.92,1.27,-4.79],[-.92,1.36,-4.77],
             [-.92,1.40,-4.72],[-.92,1.40,-4.66],[-.92,1.36,-4.61],
             [-.92,1.30,-4.61]],.016,steel,sides=12)
    _frustum(g,[-.92,1.115,-4.79],.037,.030,.027,steel,sides=16)
    _tube(g,[[-.83,1.11,-4.78],[-.83,1.18,-4.78],[-.77,1.18,-4.78]],.009,steel,sides=10)

    # Quiet, powered-off cooker. Four raised burner rings, three small knobs,
    # recessed oven glass and a separate handle provide close-up silhouette.
    _rounded_box(g,[.48,1.106,-4.52],[.61,.018,.54],glass,.018)
    for xx in [.33,.63]:
        for zz in [-4.67,-4.37]:
            points=[[xx+.073*math.cos(a),1.12,zz+.073*math.sin(a)]
                    for a in np.linspace(0,2*math.pi,33)]
            _tube(g,points,.004,burner,sides=6,closed=True)
            _frustum(g,[xx,1.116,zz],.046,.046,.009,dark,sides=20)
    _rounded_box(g,[.48,.65,-4.192],[.646,.735,.042],dark,.014)
    _rounded_box(g,[.48,.573,-4.163],[.52,.37,.018],glass,.012)
    _rounded_box(g,[.48,.57,-4.148],[.44,.28,.009],basin,.010)
    _rounded_box(g,[.48,.924,-4.159],[.58,.115,.019],steel,.008)
    for xx in [.31,.48,.65]:
        _tube(g,[[xx,.924,-4.144],[xx,.924,-4.119]],.020,dark,sides=14)
    _rounded_box(g,[.48,.811,-4.122],[.46,.024,.035],steel,.009)
    for xx in [.264,.696]:g.box([xx,.811,-4.148],[.021,.024,.043],steel)
    boxes.append(('Kitchen base and counter',[-.22,.64,-4.52],[2.14,.92,.68]))


def dress_home(g, furniture, source, *, floor=FLOOR, boxes=None, window_z=HOME_WINDOW_Z):
    """Add architecture-scale modern dressing, returning audited source props.

    Caller applies primary_furnishings() first and retains notebook ownership.
    `boxes` is the builder's list of (name, center, size) local static proxies;
    only the breakfast table and entry bench need collision. Thin wall details,
    cloth, light shades and tabletop objects deliberately add none.
    """
    boxes=[] if boxes is None else boxes
    source=Path(source);items=[]
    paint=g.material('Set | soft greige painted wood',[.52,.49,.43],.94)
    oak=g.material('Set | worn pale oak edges',[.36,.25,.16],.91)
    dark=g.material('Set | matte charcoal hardware',[.055,.065,.064],.84)
    cloth=source.parents[1]/'assets/polyhaven/castle_village/cloth_base.png'
    linen=g.material('Set | oatmeal linen curtains',[.90,.84,.72],1.,base=cloth)
    sage=g.material('Set | faded sage woven cloth',[.37,.51,.45],1.,base=cloth)
    rust=g.material('Set | faded terracotta cloth',[.61,.34,.23],1.,base=cloth)
    cream=g.material('Set | offwhite paper and ceramics',[.69,.64,.53],.95)
    shade=g.material('Set | warm linen lamp shade',[.95,.84,.66],.95,base=cloth,emissive=[.08,.055,.022])
    light=g.material('Set | shielded warm lamp diffuser',[.75,.57,.31],.85,emissive=[.65,.35,.11])
    blue=g.material('Set | faded blue bookcloth',[.14,.22,.26],.98)
    tile=g.material('Set | matte ivory backsplash',[.57,.56,.49],.87)
    tile_warm=g.material('Set | slightly aged backsplash',[.53,.51,.44],.91)
    art_paper=g.material('Set | warm unlettered print paper',[.63,.59,.50],1.)
    art_far=g.material('Set | muted landscape ink',[.32,.37,.34],1.)
    art_near=g.material('Set | dark landscape ink',[.17,.23,.22],1.)
    cabinet_paint=g.material('Set | matte sage cabinet enamel',[.20,.29,.25],.94)

    # The owned sofa already includes its cushions. Extra rigid throw boxes
    # read as floating props, so the source silhouette remains unobstructed.
    for i in range(3):
        _picture(g,-3.49+i*.74,2.12,4.83,.59,.81,
                 (oak,cream,art_paper,art_far,art_near),variation=i)
    _picture(g,-5.35,1.94,1.8,.90,1.13,
             (dark,cream,art_paper,art_far,art_near),side='west',variation=1)

    # Lighting has a physical shade and a base. The caller's practical light
    # is at the lamp diffuser, so a glowing cube is no longer necessary.
    _frustum(g,[-4.69,.205,3.52],.22,.22,.05,dark)
    _frustum(g,[-4.69,.92,3.52],.019,.016,1.40,dark,sides=12)
    _frustum(g,[-4.69,1.69,3.52],.31,.22,.36,shade,cap=False)
    _frustum(g,[-4.69,1.53,3.52],.285,.285,.012,light)

    # Correctly scaled white window frame replaces the scanned source, which
    # included its own floating yellow curtains and a baked photograph.
    window_paint=g.material('Set | painted window frame',[.72,.70,.64],.93)
    for zz in [window_z-.82,window_z+.82]:
        _rounded_box(g,[5.43,1.88,zz],[.16,1.74,.09],window_paint,.012)
    for yy in [1.055,2.705]:
        _rounded_box(g,[5.43,yy,window_z],[.16,.09,1.74],window_paint,.012)
    _rounded_box(g,[5.45,1.88,window_z],[.075,1.57,.045],window_paint,.007)
    _rounded_box(g,[5.45,1.95,window_z],[.075,.045,1.57],window_paint,.007)
    # A dark gasket gives real depth between the white frame and sky plane.
    for zz in [window_z-.75,window_z+.75]:g.box([5.49,1.88,zz],[.025,1.55,.016],dark)
    for yy in [1.13,2.63]:g.box([5.49,yy,window_z],[.025,.016,1.51],dark)
    # Actual recessed sill, paired gathered curtains and a slender dark rail.
    _rounded_box(g,[5.17,1.015,window_z],[.45,.095,1.95],paint,.018)
    g.box([5.12,2.91,window_z],[.035,.035,2.77],dark)
    for zc in [window_z-1.13,window_z+1.13]:_curtain(g,zc,.56,linen,paint)
    for zc in [window_z-1.30,window_z-1.12,window_z-.94,window_z+.95,window_z+1.13,window_z+1.31]:
        _frustum(g,[5.11,2.867,zc],.034,.034,.024,dark,sides=10)
    # Radiator supply pipes ground the old imported cast-iron radiator.
    for zc in [window_z-.32,window_z+.32]:
        _frustum(g,[5.03,.33,zc],.014,.014,.30,paint,sides=10)

    # Quiet ordinary belongings on the coffee table and desk: no narrative
    # text, branded posters, invented family photographs or floating objects.
    _rounded_box(g,[-2.77,.821,1.61],[.33,.027,.22],blue,.006,yaw=8)
    _rounded_box(g,[-2.77,.842,1.61],[.30,.015,.205],cream,.003,yaw=8)
    _rounded_box(g,[-2.75,.857,1.60],[.33,.009,.22],blue,.003,yaw=8)
    _frustum(g,[-2.65,.816,1.34],.078,.078,.008,oak,sides=20)
    g.box([-3.84,.964,-3.65],[.33,.012,.24],cream)
    _frustum(g,[-3.23,1.02,-3.78],.045,.045,.115,blue,sides=16)
    for i in range(4):
        _frustum(g,[-3.25+i*.012,1.12,-3.78],.002,.002,.13,oak,sides=6)

    # Full-size modern base units replace the miniature scanned kitchen.
    # Countertop is 0.91 m above the finished floor and 0.68 m deep; the sink
    # is a genuine open basin, with an arched tap and four independent hob rings.
    _kitchen_base(g,paint,cabinet_paint,dark,cream,boxes)
    # Three shallow upper cabinets, proper face frames, matte backsplash tile
    # and undercabinet bar make the modest kitchen a believable working room.
    for i in range(3):
        x=-.92+i*.70
        _rounded_box(g,[x,2.045,-4.68],[.685,.79,.36],paint,.012)
        _rounded_box(g,[x,2.045,-4.484],[.63,.735,.033],cabinet_paint,.009)
        g.box([x+.23,2.02,-4.46],[.018,.12,.025],dark)
    g.box([-.22,1.625,-4.51],[2.05,.025,.05],light)
    # Tiling is bounded to a splashback, not a high-frequency whole-wall grid.
    for row in range(3):
        for col in range(11):
            x=-1.22+col*.197
            _rounded_box(g,[x,1.16+row*.14,-4.86],[.19,.133,.018],
                         tile_warm if (col+row*3)%7==0 else tile,.004,subdivisions=1)
    # Shallow breakfast table keeps the center route open and fills the old
    # empty transition between kitchen and bedroom. Chair is an owned mesh.
    _rounded_box(g,[-.22,.938,-1.25],[1.17,.075,.82],oak,.025)
    for xx in [-.70,.26]:
        for zz in [-1.56,-.94]:
            _rounded_box(g,[xx,.548,zz],[.055,.72,.055],dark,.009)
    boxes.append(('Breakfast table',[-.22,.578,-1.25],[1.12,.79,.77]))
    _frustum(g,[-.47,.99,-1.25],.15,.15,.022,cream,sides=24)
    _frustum(g,[.10,1.029,-1.15],.037,.044,.10,blue,sides=16)
    # A shaded kitchen pendant supplies a clean high point over the table.
    _frustum(g,[-.22,2.92,-1.25],.012,.012,.49,dark,sides=10)
    _frustum(g,[-.22,2.59,-1.25],.30,.065,.25,paint,cap=False)
    _frustum(g,[-.22,2.48,-1.25],.27,.27,.012,light)

    # Entry bench, hooks and small bag make the door wall feel lived in. They
    # stay west of X1.95, leaving the X3.65 exit approach entirely unobstructed.
    _rounded_box(g,[1.12,.62,4.56],[1.42,.095,.48],oak,.023)
    for xx in [.54,1.70]:
        for zz in [4.40,4.72]:_rounded_box(g,[xx,.40,zz],[.065,.42,.065],dark,.008)
    boxes.append(('Entry bench',[1.12,.43,4.56],[1.40,.50,.46]))
    g.box([1.12,1.98,4.84],[1.35,.14,.055],paint)
    for xx in [.67,1.12,1.57]:
        g.box([xx,1.98,4.78],[.024,.045,.09],dark)
        g.box([xx,2.015,4.745],[.024,.075,.018],dark)
    _rounded_box(g,[1.35,.84,4.55],[.40,.35,.16],sage,.045,yaw=-8)
    # Raised bag handle is an open squared arch; no fake opaque handle slab.
    for xx in [1.23,1.47]:g.box([xx,1.06,4.55],[.028,.17,.028],dark)
    _rounded_box(g,[1.35,1.14,4.55],[.27,.035,.028],dark,.012)
    _rounded_box(g,[.53,.70,4.55],[.31,.06,.24],blue,.009)
    _rounded_box(g,[.53,.737,4.55],[.29,.014,.22],cream,.003)

    # Bedroom: a folded blanket, a proportionate bedside lamp, rug and a print.
    # Bed and wardrobe no longer intersect. The doorway stays open and readable.
    _rounded_box(g,[3.85,.69,-2.99],[1.63,.075,.48],sage,.035)
    _rounded_box(g,[3.85,.57,-2.76],[1.63,.22,.035],sage,.012)
    _picture(g,3.85,2.13,-4.83,1.20,.62,
             (oak,cream,art_paper,art_far,art_near),side='rear',variation=2)
    for x in [2.49,3.64]:
        g.box([x,1.35,-1.61],[.09,2.40,.07],paint)
    g.box([3.075,2.57,-1.61],[1.24,.12,.07],paint)

    # Individual runtime switches, facing into their own room. The kitchen
    # plate is attached to the partition rather than floating at its old
    # location beside the open doorway. The bathroom adds its own fourth plate.
    for point,normal in [([.90,1.40,4.892],[0,0,-1]),
                         ([1.162,1.40,-2.10],[-1,0,0]),
                         ([3.92,1.40,-1.588],[0,0,-1])]:
        _room_switch(g,point,normal,paint,cream)
    # Keep the low wall outlet separate from the interactive switches.
    _rounded_box(g,[-4.75,.48,4.86],[.09,.14,.016],paint,.008)
    for dx in [-.017,.017]:
        _rounded_box(g,[-4.75+dx,.48,4.848],[.006,.021,.004],dark,.001)

    return dict(furnishings=items,authored_groups=[
        'Owned sofa with clear source silhouette','Three framed unlettered landscape prints',
        'Full-size white window frame with cloth curtains and recessed sill','Physical shaded floor lamp',
        'Coffee table books and coaster','Full-size kitchen base units sink tap oven hob upper cabinets and backsplash',
        'Breakfast nook and shaded pendant','Entry bench hooks and canvas bag',
        'Bedroom blanket and existing bedside lamps','Flush electrical plates'],
        collision_names=['Kitchen base and counter','Breakfast table','Entry bench'],
        policy='All dressing is generated geometry or already owned source furniture; no new downloads or invented personal photographs.')
