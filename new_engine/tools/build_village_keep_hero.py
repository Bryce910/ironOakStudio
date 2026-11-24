#!/usr/bin/env python3
"""Author an exterior keep with actual recessed apertures, using resident maps.

This independent cooker does not run the prototype-kit cooker or change layout.
--stage writes only keep_hero/stage; --validate re-audits those GLBs; --publish
copies the two validated GLBs and merges only their two prototype manifest rows.
All coordinates are native Y-up metres. The old collision/opening metadata is
retained verbatim: visual window recesses are exterior detail, not new entrances.
"""
from pathlib import Path
import argparse
import copy
import hashlib
import json
import math
import shutil
import struct
import sys

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
MODELS = ROOT / 'projects/valwood_world/models'
WORK = ROOT / 'assets/purchased/keep_hero'
BEFORE = WORK / 'before'
STAGE = WORK / 'stage'
sys.path.insert(0, str(ROOT / 'tools'))
from build_starting_area_assets import glb
from build_purchased_village import encode_glb

RECIPE = 1
LIMITS = {'near': 32000, 'mid': 13000}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def normalize(v):
    n = np.asarray(v, dtype=float)
    return n / np.linalg.norm(n)


class Mesh:
    def __init__(self, lod):
        self.lod = lod
        self.parts = {name: [[], [], [], []] for name in ['Stone', 'Wood', 'Roof', 'DarkWood']}
        self.apertures = []

    def face(self, mat, points, hint=None, uv=None, normals=None):
        p = np.asarray(points, dtype=float)
        if len(p) < 3:
            return
        normal = np.cross(p[1] - p[0], p[2] - p[0])
        if np.linalg.norm(normal) < 1e-10:
            # A roof-tip fan may have a repeated first point. Triangulate each
            # nonzero fan triangle rather than emitting a degenerate accessor.
            for i in range(1, len(p) - 1):
                if np.linalg.norm(np.cross(p[i] - p[0], p[i + 1] - p[0])) > 1e-10:
                    self.face(mat, [p[0], p[i], p[i + 1]], hint)
            return
        normal = normalize(normal)
        if hint is not None and np.dot(normal, hint) < 0:
            p = p[::-1]
            normal = -normal
            if uv is not None:
                uv = list(reversed(uv))
            if normals is not None:
                normals = list(reversed(normals))
        if uv is None:
            # Metric planar UVs prevent roof texture stretching across the
            # entire keep. Native sampling preserves the resident map policy.
            axis = int(np.argmax(np.abs(normal)))
            axes = [(2, 1), (0, 2), (0, 1)][axis]
            tile = {'Stone': 2.4, 'Wood': 1.6, 'Roof': 2.6, 'DarkWood': 1.6}[mat]
            uv = [[v[axes[0]] / tile, v[axes[1]] / tile] for v in p]
        vertices, ns, uvs, indices = self.parts[mat]
        start = len(vertices)
        vertices.extend(p.tolist())
        ns.extend([normal.tolist()] * len(p) if normals is None else normals)
        uvs.extend(uv)
        for i in range(1, len(p) - 1):
            if np.linalg.norm(np.cross(p[i] - p[0], p[i + 1] - p[0])) > 1e-10:
                indices.extend([start, start + i, start + i + 1])

    def box(self, mat, x0, x1, y0, y1, z0, z1):
        assert min(x1 - x0, y1 - y0, z1 - z0) > 0
        p = [(x, y, z) for y in [y0, y1] for z in [z0, z1] for x in [x0, x1]]
        centre=np.array([(x0+x1)/2,(y0+y1)/2,(z0+z1)/2])
        for ids in [(0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4),
                    (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)]:
            points=[p[i] for i in ids]
            self.face(mat, points, np.mean(points,axis=0)-centre)

    def mapped_face(self, mat, coords, project, hint=None):
        self.face(mat, [project(u, y, d) for u, y, d in coords], hint,
                  [[u / 2.4, y / 2.4] for u, y, d in coords])

    def aperture(self, project, u, y, width, height, label, normal=None):
        """A true opening in an already-cut wall, with 0.30m visible reveal.

        The back is an opaque, dark timber shutter at 0.32m recess, preventing
        an unsupported promise of a furnished interior. No front dark quads.
        """
        r = width / 2
        cy = y + height - r
        segments = 8 if self.lod == 'near' else 4
        contour = [(u - r, y), (u + r, y)]
        contour += [(u + r * math.cos(t), cy + r * math.sin(t))
                    for t in np.linspace(0, math.pi, segments + 1)]
        outer_r = r + (.17 if width > .65 else .13)
        outer = [(u - outer_r, y - .10), (u + outer_r, y - .10)]
        outer += [(u + outer_r * math.cos(t), cy + outer_r * math.sin(t))
                  for t in np.linspace(0, math.pi, segments + 1)]
        n = len(contour)
        for i in range(n):
            j = (i + 1) % n
            # Visible stone intrados, jamb, and sill: no surface spans the void.
            middle=(np.array(contour[i])+np.array(contour[j]))/2
            reveal_hint=np.array(project(u,y+height*.45,-.16))-np.array(project(*middle,-.16))
            self.mapped_face('Stone', [(*contour[i], 0), (*contour[j], 0),
                             (*contour[j], -.32), (*contour[i], -.32)], project,reveal_hint)
            # One coherent arch ring with actual thickness and cut-stone joints.
            inset = .0035 if self.lod == 'near' else .001
            a, b = np.array(contour[i]), np.array(contour[j])
            c, d = np.array(outer[i]), np.array(outer[j])
            aa, bb = a + (b - a) * inset, b + (a - b) * inset
            cc, dd = c + (d - c) * inset, d + (c - d) * inset
            depth = .10 if i else .15
            front = [(*aa, depth), (*bb, depth), (*dd, depth), (*cc, depth)]
            self.mapped_face('Stone', front, project, normal)
            self.mapped_face('Stone', [(*cc, depth), (*dd, depth), (*dd, 0), (*cc, 0)], project)
        self.mapped_face('DarkWood', [(a, b, -.32) for a, b in contour], project, normal)
        # Timber mullion and transom are behind the stone face, not pasted on it.
        if width > .65:
            for a, b, bottom, top in [(u - .026, u + .026, y + .04, y + height - .06),
                                       (u - r + .025, u + r - .025, cy - .04, cy + .015)]:
                self.mapped_face('Wood', [(a, bottom, -.275), (b, bottom, -.275),
                                         (b, top, -.275), (a, top, -.275)], project, normal)
        self.apertures.append(dict(label=label, centre=project(u, y + height * .45, 0),
                                   back=project(u, y + height * .45, -.32),
                                   width=width, height=height, depth=.32))

    def wall(self, name, width, origin, u_axis, normal, windows, door=False):
        origin, u_axis, normal = map(lambda x: np.asarray(x, dtype=float), [origin, u_axis, normal])
        def project(u, y, depth):
            return (origin + u_axis * u + np.array([0, y, 0]) + normal * depth).tolist()
        holes = [(u - w / 2, u + w / 2, y, y + h, w, False)
                 for u, y, w, h in windows]
        if door:
            holes.append((-2.3, 2.3, 0, 6, 4.6, True))
        us = sorted(set([-width / 2, width / 2] + [v for h in holes for v in h[:2]]))
        ys = sorted(set([0, 22] + [v for h in holes for v in h[2:4]]))
        for a, b in zip(us, us[1:]):
            for c, d in zip(ys, ys[1:]):
                if any(x0 < (a+b)/2 < x1 and y0 < (c+d)/2 < y1 for x0,x1,y0,y1,_,_ in holes):
                    continue
                for depth, hint in [(0, normal), (-.4, -normal)]:
                    self.mapped_face('Stone', [(a,c,depth),(b,c,depth),(b,d,depth),(a,d,depth)], project, hint)
        for u, y, w, h in windows:
            r = w / 2; cy = y + h - r
            # Fill the arch spandrels above the curved intrados; the wall's
            # rectangular aperture grid is genuinely absent below this curve.
            samples = np.linspace(0, math.pi, 9 if self.lod == 'near' else 5)
            for t0, t1 in zip(samples, samples[1:]):
                a,b = u+r*math.cos(t0), u+r*math.cos(t1)
                c,d = cy+r*math.sin(t0), cy+r*math.sin(t1)
                for depth,hint in [(0,normal),(-.4,-normal)]:
                    self.mapped_face('Stone', [(a,c,depth),(a,y+h,depth),(b,y+h,depth),(b,d,depth)],project,hint)
            self.aperture(project,u,y,w,h,name,normal)
        if door:
            # The original rectangular entrance stays entirely clear to 6m.
            self.mapped_face('Stone',[(-2.3,0,0),(-2.3,6,0),(-2.3,6,-.4),(-2.3,0,-.4)],project)
            self.mapped_face('Stone',[(2.3,0,-.4),(2.3,6,-.4),(2.3,6,0),(2.3,0,0)],project)
            self.mapped_face('Stone',[(-2.3,6,0),(2.3,6,0),(2.3,6,-.4),(-2.3,6,-.4)],project)
            self.portal(project,normal)
        # A chamfered foundation course and two restrained floor stringcourses.
        for y, h, ext in [(.05,.65,.10),(7.15,.18,.12),(14.35,.17,.10),(21.65,.30,.22)]:
            ranges = [(-width/2,width/2)] if not door or y>=6 else [(-width/2,-2.3),(2.3,width/2)]
            for a,b in ranges:
                profile=[(y,0),(y+.06,ext),(y+h-.06,ext),(y+h,0)]
                for (c,e),(d,f) in zip(profile,profile[1:]):
                    self.mapped_face('Stone',[(a,c,e),(b,c,e),(b,d,f),(a,d,f)],project,normal)

    def portal(self, project, normal):
        # Romanesque archivolt rises ABOVE the existing full-height rectangular
        # gate. Its intrados never removes the upper corners of the 4.6x6 void.
        for side in [-1,1]:
            a,b = sorted([side*2.3,side*2.73])
            for y0,y1 in [(0,.35),(.35,5.65),(5.65,6)]:
                self.mapped_face('Stone',[(a,y0,.23),(b,y0,.23),(b,y1,.23),(a,y1,.23)],project,normal)
                for edge in [a,b]:
                    self.mapped_face('Stone',[(edge,y0,0),(edge,y1,0),(edge,y1,.23),(edge,y0,.23)],project)
        count=18 if self.lod=='near' else 10
        for a,b in zip(np.linspace(0,math.pi,count+1),np.linspace(0,math.pi,count+1)[1:]):
            inner=[(2.3*math.cos(t),6+2.3*math.sin(t)) for t in [a,b]]
            outer=[(2.73*math.cos(t),6+2.73*math.sin(t)) for t in [a,b]]
            self.mapped_face('Stone',[(*inner[0],.23),(*inner[1],.23),(*outer[1],.23),(*outer[0],.23)],project,normal)
            self.mapped_face('Stone',[(*inner[0],0),(*inner[1],0),(*inner[1],.23),(*inner[0],.23)],project)
            self.mapped_face('Stone',[(*outer[0],.23),(*outer[1],.23),(*outer[1],0),(*outer[0],0)],project)

    def ring(self,cx,cz,profile,segments):
        for (y0,r0),(y1,r1) in zip(profile,profile[1:]):
            for i in range(segments):
                a,b=i*math.tau/segments,(i+1)*math.tau/segments
                p=lambda r,y,t:(cx+r*math.cos(t),y,cz+r*math.sin(t))
                self.face('Stone',[p(r0,y0,a),p(r0,y0,b),p(r1,y1,b),p(r1,y1,a)],
                          [math.cos((a+b)/2),.01,math.sin((a+b)/2)])

    def tower(self,cx,cz):
        radius=3.7
        phis=[1.5*math.pi if cz<0 else .5*math.pi]
        if self.lod=='near':
            phis.append(0 if cx>0 else math.pi)
        holes=[]
        for phi in phis:
            for y in [4.7,12,19.3]:holes.append((phi*radius,y,.52,1.75))
        n=48 if self.lod=='near' else 24
        # Put the cylindrical seam away from all cardinal-direction apertures.
        us=sorted(set([(-math.pi/4+i*math.tau/n)*radius for i in range(n+1)]+
                      [v for u,y,w,h in holes for v in [u-w/2,u+w/2]]))
        ys=sorted(set([0,26.5]+[v for u,y,w,h in holes for v in [y,y+h]]))
        def project(u,y,d):
            return [cx+(radius+d)*math.cos(u/radius),y,cz+(radius+d)*math.sin(u/radius)]
        for a,b in zip(us,us[1:]):
            for c,d in zip(ys,ys[1:]):
                if any(u-w/2<(a+b)/2<u+w/2 and y<(c+d)/2<y+h for u,y,w,h in holes):continue
                p=[project(a,c,0),project(b,c,0),project(b,d,0),project(a,d,0)]
                normal=[math.cos((a+b)/2/radius),0,math.sin((a+b)/2/radius)]
                normals=[[math.cos(u/radius),0,math.sin(u/radius)] for u in [a,b,b,a]]
                self.face('Stone',p,normal,[[u/2.4,v/2.4] for u,v in [(a,c),(b,c),(b,d),(a,d)]],normals)
        for u,y,w,h in holes:
            r=w/2;cy=y+h-r;sample=np.linspace(0,math.pi,9 if self.lod=='near' else 5)
            normal=[math.cos(u/radius),0,math.sin(u/radius)]
            for t0,t1 in zip(sample,sample[1:]):
                a,b=u+r*math.cos(t0),u+r*math.cos(t1)
                c,d=cy+r*math.sin(t0),cy+r*math.sin(t1)
                self.mapped_face('Stone',[(a,c,0),(a,y+h,0),(b,y+h,0),(b,d,0)],project,normal)
            self.aperture(project,u,y,w,h,'tower',normal)
        for y in [1.0,8.0,15.4]:
            self.ring(cx,cz,[(y,radius),(y+.07,3.82),(y+.23,3.82),(y+.30,radius)],n)
        # Real sloping corbels carry the crown rather than floating trim boxes.
        count=16 if self.lod=='near' else 8
        for i in range(count):
            phi=(i+.5)*math.tau/count;half=.20/radius
            p=lambda t,y,r:(cx+r*math.cos(t),y,cz+r*math.sin(t))
            a,b=phi-half,phi+half
            section=[(24.15,3.70),(24.72,3.96),(25.16,3.96),(25.16,3.70)]
            for (y0,r0),(y1,r1) in zip(section,section[1:]+section[:1]):
                hint=np.array(p(phi,(y0+y1)/2,(r0+r1)/2))-np.array(p(phi,24.78,3.80))
                self.face('Stone',[p(a,y0,r0),p(b,y0,r0),p(b,y1,r1),p(a,y1,r1)],hint)
            for t in [a,b]:
                hint=[-math.sin(t),0,math.cos(t)]
                if t==a:hint=[-v for v in hint]
                self.face('Stone',[p(t,y,r) for y,r in section],hint)
        self.ring(cx,cz,[(25.1,3.70),(25.22,3.96),(25.45,3.96),(25.55,3.82),
                         (26.35,3.82),(26.48,3.96),(26.7,3.96),(26.8,3.70)],n)
        # Alternating short merlons remain beneath the unchanged conical eaves.
        for i in range(count):
            phi=i*math.tau/count;half=.45/radius
            a,b=phi-half,phi+half
            p=lambda t,y,r:(cx+r*math.cos(t),y,cz+r*math.sin(t))
            for r in [3.55,3.82]:
                hint=[math.cos(phi),0,math.sin(phi)]
                if r==3.55:hint=[-v for v in hint]
                self.face('Stone',[p(a,26.7,r),p(b,26.7,r),p(b,27.1,r),p(a,27.1,r)],hint)
            for t in [a,b]:
                hint=[-math.sin(t),0,math.cos(t)]
                if t==a:hint=[-v for v in hint]
                self.face('Stone',[p(t,26.7,3.55),p(t,26.7,3.82),p(t,27.1,3.82),p(t,27.1,3.55)],hint)
            self.face('Stone',[p(a,27.1,3.55),p(b,27.1,3.55),p(b,27.1,3.82),p(a,27.1,3.82)],[0,1,0])
        self.cone_roof(cx,cz,n)

    def cone_roof(self,cx,cz,segments):
        peak=35 if self.lod=='near' else 35.002384185791016
        rows=14 if self.lod=='near' else 5
        def p(t,y,r):return (cx+r*math.cos(t),y,cz+r*math.sin(t))
        for row in range(rows):
            y0=27+(peak-27)*row/rows;y1=27+(peak-27)*(row+1)/rows
            r0=4.2*(1-row/rows);r1=4.2*(1-(row+1)/rows)
            for i in range(segments):
                a,b=i*math.tau/segments,(i+1)*math.tau/segments
                # Course bevel is inside the cone envelope, avoiding coincident
                # layer surfaces and preserving the exact four roof extrema.
                q0=p(a,y0,r0);q1=p(b,y0,r0)
                q2=p(b,y1,r1);q3=p(a,y1,r1)
                hint=[math.cos((a+b)/2),.5,math.sin((a+b)/2)]
                uv=[(a*4.2/2.6,(y0-27)/2.6),(b*4.2/2.6,(y0-27)/2.6),
                    (b*4.2/2.6,(y1-27)/2.6),(a*4.2/2.6,(y1-27)/2.6)]
                if row+1==rows:self.face('Roof',[q0,q1,q2],hint,uv[:3])
                else:
                    lip_y=min(y0+.045,y1)
                    lip_r=4.2*(1-(lip_y-27)/(peak-27))-.025
                    la,lb=p(a,lip_y,lip_r),p(b,lip_y,lip_r)
                    self.face('Roof',[q0,q1,lb,la],hint,[uv[0],uv[1],(uv[1][0],uv[1][1]+.018),(uv[0][0],uv[0][1]+.018)])
                    self.face('Roof',[la,lb,q2,q3],hint,[(uv[0][0],uv[0][1]+.018),(uv[1][0],uv[1][1]+.018),uv[2],uv[3]])
        # Underside visible at an upward walking view; four roof-base discs also
        # establish the original +/-18,+/-14 metre extrema exactly.
        for i in range(segments):
            a,b=i*math.tau/segments,(i+1)*math.tau/segments
            self.face('Wood',[(cx,27,cz),p(a,27,4.2),p(b,27,4.2)],[0,-1,0])

    def hall_roof(self):
        for side in [-1,1]:
            # Overlapping pitched slate courses use narrow bevels; no duplicated
            # nearly coplanar roof planes that could flicker at distance.
            rows=16 if self.lod=='near' else 7
            for row in range(rows):
                t0,t1=row/rows,(row+1)/rows
                x0,x1=side*12.5*t0,side*12.5*t1
                y0,y1=30-8*t0,30-8*t1
                q=[(x0,y0,-9.5),(x1,y1,-9.5),(x1,y1,9.5),(x0,y0,9.5)]
                self.face('Roof',q,[side*.5,1,0],[[t0*14.84/2.6,-9.5/2.6],[t1*14.84/2.6,-9.5/2.6],
                                                    [t1*14.84/2.6,9.5/2.6],[t0*14.84/2.6,9.5/2.6]])
        for z,n in [(-9,[0,0,-1]),(9,[0,0,1])]:
            self.gable(z,n)
            # Stone tabling follows the gable, not a flat rectangular cap.
            for side in [-1,1]:
                self.face('Stone',[(0,29.68,z),(side*12,22,z),(side*12,22.25,z+(.15 if z>0 else -.15)),
                                   (0,29.93,z+(.15 if z>0 else -.15))],n)
        self.box('Wood',-12.5,12.5,21.84,22,-9.5,9.5)

    def gable(self,z,normal):
        """Three real high-level lancets break the broad triangular hall gable."""
        windows=[(-2.1,23.4,.75,1.85),(0,23.4,.85,2.55),(2.1,23.4,.75,1.85)]
        project=lambda u,y,d:[u,y,z+(d if z>0 else -d)]
        holes=[(u-w/2,u+w/2,y,y+h) for u,y,w,h in windows]
        us=sorted(set([-12,0,12]+[v for h in holes for v in h[:2]]))
        ys=sorted(set([22,29.68]+[v for h in holes for v in h[2:]]))
        def clip(poly,side):
            result=[]
            signed=lambda p:29.68-(7.68/12)*side*p[0]-p[1]
            for a,b in zip(poly,poly[1:]+poly[:1]):
                aa,bb=signed(a),signed(b)
                if aa>=-1e-9:result.append(a)
                if (aa>=0)!=(bb>=0):
                    t=aa/(aa-bb);result.append([a[0]+t*(b[0]-a[0]),a[1]+t*(b[1]-a[1])])
            return result
        for a,b in zip(us,us[1:]):
            for c,d in zip(ys,ys[1:]):
                if any(x0<(a+b)/2<x1 and y0<(c+d)/2<y1 for x0,x1,y0,y1 in holes):continue
                poly=clip(clip([[a,c],[b,c],[b,d],[a,d]],1),-1)
                if len(poly)>=3:
                    self.mapped_face('Stone',[(u,y,0) for u,y in poly],project,normal)
        for u,y,w,h in windows:
            r=w/2;cy=y+h-r
            samples=np.linspace(0,math.pi,9 if self.lod=='near' else 5)
            for a,b in zip(samples,samples[1:]):
                x0,x1=u+r*math.cos(a),u+r*math.cos(b)
                y0,y1=cy+r*math.sin(a),cy+r*math.sin(b)
                self.mapped_face('Stone',[(x0,y0,0),(x0,y+h,0),(x1,y+h,0),(x1,y1,0)],project,normal)
            self.aperture(project,u,y,w,h,'gable',normal)


def build(lod):
    m=Mesh(lod)
    front=[(x,2.0,.48,1.5) for x in [-8,-4,4,8]]
    front += [(x,y,1.10,2.30) for y in [9.6,16.35] for x in [-8,-4,0,4,8]]
    back=[(x,2.0,.48,1.5) for x in [-8,-4,0,4,8]]
    back += [(x,y,1.10,2.30) for y in [9.6,16.35] for x in [-8,-4,0,4,8]]
    side=[(x,2.0,.48,1.5) for x in [-5,0,5]]
    side += [(x,y,1.10,2.30) for y in [9.6,16.35] for x in [-5,0,5]]
    m.wall('front',24,[0,0,-9],[1,0,0],[0,0,-1],front,True)
    m.wall('rear',24,[0,0,9],[-1,0,0],[0,0,1],back)
    m.wall('left',18,[-12,0,0],[0,0,-1],[-1,0,0],side)
    m.wall('right',18,[12,0,0],[0,0,1],[1,0,0],side)
    # Retain baseline interior collision supports and doorway-accessible floor.
    m.box('Wood',-11.6,11.6,.01,.02,-9,8.6)
    m.box('Wood',-11.75,11.75,7,7.15,-8.75,8.75)
    for x in [-7,7]:
        for z in [-4,4]:m.box('Wood',x-.4,x+.4,.02,7,z-.4,z+.4)
    m.hall_roof()
    for x in [-13.8,13.8]:
        for z in [-9.8,9.8]:m.tower(x,z)
    return m


class GLBWriter:
    def __init__(self):
        self.doc=dict(asset=dict(version='2.0',generator='Valwood exterior keep recipe 1'),
                      scene=0,scenes=[dict(nodes=[0])],nodes=[dict(mesh=0,name='Valwood_Keep_Exterior')],
                      meshes=[dict(primitives=[])],buffers=[dict(byteLength=0)],
                      bufferViews=[],accessors=[],materials=[],images=[],textures=[],samplers=[])
        self.binary=bytearray();self.image_keys={};self.texture_keys={};self.sampler_keys={}
        self.provenance=[]

    def blob(self,raw,target=None):
        self.binary.extend(b'\0'*((-len(self.binary))%4))
        i=len(self.doc['bufferViews'])
        row=dict(buffer=0,byteOffset=len(self.binary),byteLength=len(raw))
        if target:row['target']=target
        self.doc['bufferViews'].append(row);self.binary.extend(raw)
        return i

    def accessor(self,values,components,ctype=5126):
        array=np.asarray(values,dtype='<f4' if ctype==5126 else '<u4')
        index=len(self.doc['accessors'])
        row=dict(bufferView=self.blob(array.tobytes(),34963 if ctype==5125 else 34962),
                 componentType=ctype,count=len(array),type={1:'SCALAR',2:'VEC2',3:'VEC3'}[components])
        if components==3:row.update(min=array.min(axis=0).tolist(),max=array.max(axis=0).tolist())
        self.doc['accessors'].append(row)
        return index

    def material(self,path,name,new_name,dark=False):
        doc,binary=glb(path)
        original=next(row for row in doc['materials'] if row.get('name')==name)
        material=copy.deepcopy(original)
        material.pop('extensions',None)
        material.update(name='keep_dark' if new_name=='DarkWood' else new_name.lower(),
                        alphaMode='OPAQUE',doubleSided=False)
        material.pop('alphaCutoff',None)
        pbr=material.setdefault('pbrMetallicRoughness',{})
        # All retained maps use AO=R/roughness=G/metallic=B. Authored masonry and
        # timber remain dielectric even if an old glTF default was metallic=1.
        pbr['metallicFactor']=0
        if new_name=='Roof':pbr['baseColorFactor']=[.52,.60,.66,1]
        if dark:pbr.update(baseColorFactor=[.035,.040,.045,1],roughnessFactor=1)
        def texture(index):
            row=doc['textures'][index];image=doc['images'][row['source']]
            view=doc['bufferViews'][image['bufferView']]
            raw=binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
            key=hashlib.sha256(raw).hexdigest()
            if key not in self.image_keys:
                self.image_keys[key]=len(self.doc['images'])
                self.doc['images'].append(dict(bufferView=self.blob(raw),mimeType=image['mimeType'],name=image.get('name',key[:12])))
            sampler=copy.deepcopy(doc.get('samplers',[{}])[row.get('sampler',0)])
            skey=json.dumps(sampler,sort_keys=True)
            if skey not in self.sampler_keys:
                self.sampler_keys[skey]=len(self.doc['samplers']);self.doc['samplers'].append(sampler)
            tkey=(key,skey)
            if tkey not in self.texture_keys:
                self.texture_keys[tkey]=len(self.doc['textures'])
                self.doc['textures'].append(dict(source=self.image_keys[key],sampler=self.sampler_keys[skey]))
            return self.texture_keys[tkey]
        for parent,key in [(material,'normalTexture'),(material,'occlusionTexture'),(pbr,'baseColorTexture'),(pbr,'metallicRoughnessTexture')]:
            if key in parent:parent[key]['index']=texture(parent[key]['index'])
        self.doc['materials'].append(material)
        self.provenance.append(dict(material=new_name,path=str(path.relative_to(ROOT)),source_material=name,
                                    source_sha256=sha(path),unchanged_encoded_images=True))

    def save(self,path,mesh):
        house=MODELS/'purchased_village_house_a_near.glb'
        self.material(house,'Purchased_Tile_T_Tileable_Wall_Stone_01','Stone')
        self.material(house,'Purchased_Source_Wood','Wood')
        self.material(BEFORE/'village_keep.glb','roof','Roof')
        self.material(house,'Purchased_Source_Wood','DarkWood',True)
        for i,name in enumerate(mesh.parts):
            p,n,uv,indices=mesh.parts[name]
            assert p and indices
            self.doc['meshes'][0]['primitives'].append(dict(mode=4,material=i,
                 attributes=dict(POSITION=self.accessor(p,3),NORMAL=self.accessor(n,3),TEXCOORD_0=self.accessor(uv,2)),
                 indices=self.accessor(indices,1,5125)))
        self.doc['buffers'][0]['byteLength']=len(self.binary)
        path.write_bytes(encode_glb(self.doc,bytes(self.binary)))
        return self.provenance


def accessor(doc,binary,index):
    row=doc['accessors'][index];view=doc['bufferViews'][row['bufferView']]
    n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[row['type']]
    dtype={5126:'<f4',5125:'<u4',5123:'<u2'}[row['componentType']]
    assert 'byteStride' not in view
    return np.frombuffer(binary,dtype=dtype,count=row['count']*n,
                         offset=view.get('byteOffset',0)+row.get('byteOffset',0)).reshape((-1,n))


def triangles(path):
    doc,binary=glb(path);points=[];tris=[]
    for primitive in doc['meshes'][0]['primitives']:
        p=accessor(doc,binary,primitive['attributes']['POSITION'])
        n=accessor(doc,binary,primitive['attributes']['NORMAL'])
        uv=accessor(doc,binary,primitive['attributes']['TEXCOORD_0'])
        index=accessor(doc,binary,primitive['indices']).flatten()
        assert np.isfinite(p).all() and np.isfinite(n).all() and np.isfinite(uv).all()
        assert np.max(np.abs(np.linalg.norm(n,axis=1)-1))<1e-5
        ti=index.reshape((-1,3));t=p[ti]
        cross=np.cross(t[:,1]-t[:,0],t[:,2]-t[:,0])
        assert np.min(np.sum(cross*np.mean(n[ti],axis=1),axis=1))>1e-8,'winding/normal disagreement'
        points.append(p);tris.append(t)
    return doc,np.concatenate(points),np.concatenate(tris)


def ray_hits(tris,origin,direction):
    """Vectorized double-sided ray test, used only by the CPU proof."""
    a,b,c=tris[:,0],tris[:,1],tris[:,2];e1=b-a;e2=c-a
    direction=np.asarray(direction,float);origin=np.asarray(origin,float)
    h=np.cross(np.broadcast_to(direction,e2.shape),e2);det=np.sum(e1*h,axis=1)
    keep=np.abs(det)>1e-8;inv=np.zeros_like(det);inv[keep]=1/det[keep]
    s=origin-a;u=inv*np.sum(s*h,axis=1);q=np.cross(s,e1)
    v=inv*np.sum(direction*q,axis=1);t=inv*np.sum(e2*q,axis=1)
    return np.sort(t[keep&(u>=-1e-7)&(v>=-1e-7)&(u+v<=1+1e-7)&(t>1e-6)])


def image_policies(path):
    doc,binary=glb(path);keys=set()
    for material in doc.get('materials',[]):
        pbr=material.get('pbrMetallicRoughness',{})
        for parent,key,srgb in [(pbr,'baseColorTexture',True),(material,'normalTexture',False),
                                 (pbr,'metallicRoughnessTexture',False),(material,'occlusionTexture',False)]:
            if key not in parent:continue
            tex=doc['textures'][parent[key]['index']];image=doc['images'][tex['source']]
            view=doc['bufferViews'][image['bufferView']]
            raw=binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
            sampler=doc.get('samplers',[{}])[tex.get('sampler',0)]
            keys.add((hashlib.sha256(raw).hexdigest(),srgb,json.dumps(sampler,sort_keys=True)))
    return keys


def validate():
    manifest=json.loads((STAGE/'keep_hero_manifest.json').read_text())
    old=json.loads((BEFORE/'village_asset_manifest.json').read_text())
    rows={r['asset']:r for r in old['assets']}
    resident=image_policies(BEFORE/'village_keep.glb')|image_policies(MODELS/'purchased_village_house_a_near.glb')
    result=[]
    for row in manifest['assets']:
        path=STAGE/(row['asset']+'.glb');doc,p,t=triangles(path)
        assert sha(path)==row['sha256'] and path.stat().st_size==row['bytes'],'staged asset changed since cook'
        assert len(doc['materials'])==len(doc['meshes'][0]['primitives'])==4
        assert len(t)<=LIMITS[row['lod']]
        assert np.max(np.abs(p.min(axis=0)-rows[row['asset']]['bounds_y_up'][0]))<1e-5
        assert np.max(np.abs(p.max(axis=0)-rows[row['asset']]['bounds_y_up'][1]))<1e-5
        area=np.linalg.norm(np.cross(t[:,1]-t[:,0],t[:,2]-t[:,0]),axis=1)
        assert np.min(area)>1e-8,'degenerate triangles'
        assert row['collision_boxes']==rows[row['asset']]['collision_boxes']
        assert row['door_openings']==rows[row['asset']]['door_openings']
        assert len(row['collision_boxes'])==14
        assert image_policies(path)<=resident,'new image/sRGB/sampler policy'
        # The whole full-height portal is tested, including its upper corners.
        # A ray traverses the front wall and continues into the old interior.
        door_rays=0
        for x in np.linspace(-2.299,2.299,17):
            for y in np.linspace(.05,5.999,19):
                hits=ray_hits(t,[x,y,-10.0],[0,0,1])
                assert not np.any(hits<2.0),('portal obstructed',x,y,hits[:4])
                door_rays+=1
        # Windows must hit the dark shutter behind the original wall plane, and
        # a neighbouring masonry ray must still hit the wall at z=-9.0.
        window_depths=[]
        for x,y in [(0,10.2),(-4,17.0),(-8,2.45)]:
            hits=ray_hits(t,[x,y,-10],[0,0,1]);assert len(hits)
            depth=float(hits[0]-1);assert .27<depth<.34,(x,y,depth)
            window_depths.append(depth)
        for x,y in [(1.6,10.2),(-2.2,17),(-6.8,2.45)]:
            hits=ray_hits(t,[x,y,-10],[0,0,1]);assert abs(float(hits[0])-1)<1e-5
        # Curved tower slits use the same real recess, not flat dark stickers.
        towerhits=ray_hits(t,[13.8,12.6,-15],[0,0,1])
        towerdepth=float(towerhits[0]-(15-9.8-3.7));assert .27<towerdepth<.35,towerdepth
        # Exhaustively sample every authored aperture, including rear and side
        # walls that the showcase camera cannot see. The mullion's front lies
        # 0.275m behind the wall, the opaque shutter 0.32m behind it.
        apertures=build(row['lod']).apertures
        for aperture in apertures:
            centre=np.asarray(aperture['centre']);back=np.asarray(aperture['back'])
            outward=normalize(centre-back)
            hits=ray_hits(t,centre+outward,-outward)
            assert len(hits) and .265<float(hits[0]-1)<.35,(aperture,float(hits[0]-1) if len(hits) else None)
        # Negative controls prove these probes reject the old printed window
        # and a crossbar cutting the original passage.
        barred=np.concatenate([t,np.array([[[-2.3,3,-9.2],[2.3,3,-9.2],[2.3,4,-9.2]],
                                          [[-2.3,3,-9.2],[2.3,4,-9.2],[-2.3,4,-9.2]]])])
        assert ray_hits(barred,[0,3.5,-10],[0,0,1])[0]<1
        plain=np.array([[[-1,9.5,-9],[1,9.5,-9],[1,11,-9]],[[-1,9.5,-9],[1,11,-9],[-1,11,-9]]])
        assert abs(float(ray_hits(plain,[0,10.2,-10],[0,0,1])[0])-1)<1e-5
        result.append(dict(asset=row['asset'],triangles=len(t),draw_materials=4,
                           bounds_y_up=[p.min(axis=0).tolist(),p.max(axis=0).tolist()],
                           door_rays_clear=door_rays,body_window_depths_m=window_depths,
                           tower_window_depth_m=towerdepth,unchanged_colliders=14,
                           actual_apertures_depth_tested=len(apertures),all_triangle_winding_agrees=True,
                           resident_image_policies=len(image_policies(path)),
                           portal_crossbar_negative_control=True,flat_window_negative_control=True,
                           sha256=sha(path),bytes=path.stat().st_size))
    manifest['validation']=result
    (STAGE/'keep_hero_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(result,indent=2),flush=True)
    return manifest


def stage():
    WORK.mkdir(exist_ok=True,parents=True);BEFORE.mkdir(exist_ok=True);STAGE.mkdir(exist_ok=True)
    for name in ['village_keep.glb','village_keep_mid.glb','village_asset_manifest.json']:
        if not (BEFORE/name).exists():shutil.copy2(MODELS/name,BEFORE/name)
    old=json.loads((BEFORE/'village_asset_manifest.json').read_text())
    rows={r['asset']:r for r in old['assets']}
    assets=[];provenance=[]
    for lod,name in [('near','village_keep'),('mid','village_keep_mid')]:
        mesh=build(lod);writer=GLBWriter();path=STAGE/(name+'.glb')
        provenance=writer.save(path,mesh)
        row=copy.deepcopy(rows[name]);doc,p,t=triangles(path)
        row.update(bytes=path.stat().st_size,sha256=sha(path),triangles=len(t),materials=4,draw_materials=4,
                   source='Valwood authored recessed exterior architecture; retained purchased masonry/timber and CC0 slate PBR',
                   license='Geometry authored for Valwood; purchased textures retain user-purchased license; slate CC0-1.0',
                   hero_recipe=RECIPE,bounds_y_up=[p.min(axis=0).tolist(),p.max(axis=0).tolist()],
                   validation=dict(actual_recessed_apertures=len(mesh.apertures),window_recess_m=.32,
                                   entry_width_m=4.6,entry_height_m=6,exterior_only=True))
        assets.append(row)
    manifest=dict(schema_version=1,recipe=RECIPE,assets=assets,materials=provenance,
                  constraints=dict(unchanged_colliders=14,entry_width_m=4.6,entry_height_m=6,
                                   maximum_design_dimensions=[36,35,28],draw_materials=4,
                                   triangle_limits=LIMITS,new_texture_images=0,exterior_only=True),
                  baseline_manifest_sha256=sha(BEFORE/'village_asset_manifest.json'))
    (STAGE/'keep_hero_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    validate()


def publish():
    manifest=validate()
    path=MODELS/'village_asset_manifest.json';old=json.loads(path.read_text())
    others={r['asset']:copy.deepcopy(r) for r in old['assets'] if r['asset'] not in {'village_keep','village_keep_mid'}}
    replacement={r['asset']:r for r in manifest['assets']}
    for i,row in enumerate(old['assets']):
        if row['asset'] in replacement:
            assert row['collision_boxes']==replacement[row['asset']]['collision_boxes']
            assert row['door_openings']==replacement[row['asset']]['door_openings']
            old['assets'][i]=replacement[row['asset']]
    assert others=={r['asset']:r for r in old['assets'] if r['asset'] in others}
    for name in replacement:shutil.copy2(STAGE/(name+'.glb'),MODELS/(name+'.glb'))
    path.write_text(json.dumps(old,indent=2)+'\n')
    shutil.copy2(STAGE/'keep_hero_manifest.json',WORK/'keep_hero_manifest.json')
    print('Published only two hero-keep GLBs and their two prototype-kit manifest rows.',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--stage',action='store_true')
    parser.add_argument('--validate',action='store_true');parser.add_argument('--publish',action='store_true')
    args=parser.parse_args()
    if args.stage:stage()
    elif args.publish:publish()
    elif args.validate:validate()
    else:parser.error('choose --stage, --validate, or --publish')
