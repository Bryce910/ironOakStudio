#!/usr/bin/env python3
"""Restrained modern story dressing for the overworked father's apartment.

Only cosmetic geometry inside the accepted furniture bounds is added. The
work bag, notebook, mugs, furniture and route are already present; reuse them
rather than crowding the opening with duplicate props. Printed material is
original fictional text drawn into one small, embedded matte-paper atlas.
No external asset, generated photograph or additional collision is needed.
"""
from pathlib import Path
import hashlib
import io
import math

import numpy as np
from PIL import Image, ImageDraw, ImageFont


RENT_NOTICE_CENTER = [-2.55, .817, 1.055]
WORK_HOURS_CENTER = [-.16, .978, -1.465]


def _paper_atlas():
    """Two 512px pages in one 1024x512 PNG; font is bundled with the repo."""
    root=Path(__file__).resolve().parents[1]
    font=root/'third_party/imgui/misc/fonts/DroidSans.ttf'
    if not font.is_file():
        raise FileNotFoundError('The repository-bundled DroidSans font is required')
    def face(size):return ImageFont.truetype(str(font),size)
    atlas=Image.new('RGB',(1024,512),(217,211,192))
    d=ImageDraw.Draw(atlas)
    ink=(52,58,57);faded=(99,100,92);red=(130,57,43);gold=(154,132,89)
    # Reducing a letter's hierarchy is deliberate: the large short headings
    # survive the opening camera; the smaller content rewards closer looking.
    for offset in [0,512]:
        d.rectangle((offset+3,3,offset+508,508),fill=(227,222,204))
        d.line((offset+28,100,offset+484,100),fill=gold,width=2)
        # Original thin fibre marks, fully deterministic and lower contrast
        # than any print; no baked lighting or dirty broad stains.
        for row in range(17):
            y=17+row*29;x=offset+12+(row*97)%440
            d.line((x,y,x+23,y+1),fill=(222,217,200),width=1)
    d.text((28,25),'PAYMENT REMINDER',font=face(29),fill=red)
    d.text((28,65),'RENT / THIS MONTH',font=face(21),fill=ink)
    d.text((28,125),'BALANCE STILL DUE',font=face(27),fill=ink)
    d.text((28,174),'Rent is due before payday.',font=face(24),fill=ink)
    d.text((28,216),'Power. Food. Travel.',font=face(24),fill=ink)
    d.text((28,254),'Everything costs more.',font=face(24),fill=ink)
    d.line((28,304,484,304),fill=gold,width=2)
    d.text((28,328),'Extra shift requested',font=face(26),fill=red)
    d.text((28,379),'Check hours before Friday.',font=face(22),fill=faded)
    d.text((28,453),'Account copy / keep for your records',font=face(16),fill=faded)
    x=540
    d.text((x,25),'WORK / THIS WEEK',font=face(29),fill=ink)
    d.text((x,65),'SHIFT RECORD',font=face(21),fill=red)
    d.text((x,120),'DAY',font=face(20),fill=faded)
    d.text((x+160,120),'START',font=face(20),fill=faded)
    d.text((x+305,120),'FINISH',font=face(20),fill=faded)
    for row,day in enumerate(['MON','TUE','WED','THU','FRI','SAT']):
        y=159+row*36
        d.text((x,y),day,font=face(24),fill=ink)
        d.text((x+160,y),'06:30',font=face(24),fill=ink)
        d.text((x+305,y),'19:00',font=face(24),fill=ink)
        d.line((x,y+31,x+455,y+31),fill=(197,193,178),width=1)
    d.text((x,394),'ONE MORE SHIFT?',font=face(26),fill=red)
    d.text((x,438),'Just enough to keep up.',font=face(23),fill=faded)
    out=io.BytesIO();atlas.save(out,format='PNG',optimize=True)
    return out.getvalue(),dict(font='Repository-bundled DroidSans.ttf',
        resolution=[1024,512],pages=['Fictional rent and household-cost reminder',
                                   'Fictional six-day work shift record'],
        original_print=True)


def _paper(g,center,size,material,uv_rect,yaw=0):
    """A paper top quad with outward +Y winding; corners never float."""
    angle=math.radians(yaw);c,s=math.cos(angle),math.sin(angle)
    cx,cy,cz=center;w,h=size
    # From the near edge, UV top stays toward the far end of the document.
    p=[]
    for x,z in [(-w/2,-h/2),(-w/2,h/2),(w/2,h/2),(w/2,-h/2)]:
        p.append([cx+x*c+z*s,cy,cz-x*s+z*c])
    u0,v0,u1,v1=uv_rect
    g.mesh(p,[[0,1,0]]*4,[[u0,v0],[u0,v1],[u1,v1],[u1,v0]],
           [0,1,2,0,2,3],material)


def dress_opening_story(g):
    """Append to the builder's existing home GLB; never edits colliders.

    The rent notice is on the unoccupied coffee-table near edge, clear of the
    notebook, cup and controller. The timesheet is on the breakfast table's
    rear half. The meal sits on the existing plate. The entry work bag gets a
    small worn badge, making its purpose clear without adding another bag.
    """
    from opening_home_set import _rounded_box
    before={m:len(parts)for m,parts in g.geometry.items()}
    first_material=len(g.doc['materials'])
    atlas,atlas_meta=_paper_atlas()
    print_mat=g.material('Story | matte original bills and overworked shift record',
                         [.91,.90,.86],1.)
    texture=g.texture(atlas,size=1024)
    g.doc['materials'][print_mat]['pbrMetallicRoughness']['baseColorTexture']={'index':texture}
    paper=g.material('Story | warm thin paper edges',[.66,.63,.54],1.)
    kraft=g.material('Story | inexpensive folded takeaway carton',[.33,.24,.14],.98)
    fold=g.material('Story | takeaway paper folds',[.26,.18,.105],1.)
    work_badge=g.material('Story | worn unbranded work badge',[.41,.42,.36],.96)
    # Accepted coffee table top is .8103036 m; this shallow receipt stack stays
    # above the surface and within its accepted blocker, never on the route.
    g.box([RENT_NOTICE_CENTER[0],.8135,RENT_NOTICE_CENTER[2]],[.255,.006,.225],paper)
    _paper(g,RENT_NOTICE_CENTER,[.25,.22],print_mat,[0.,0.,.5,1.],yaw=0)
    # Accepted breakfast top ends at Y .9755. A 1 mm printed surface gives a
    # real hierarchy without a floating page or an extra collision proxy.
    g.box([WORK_HOURS_CENTER[0],.9768,WORK_HOURS_CENTER[2]],[.275,.002,.190],paper)
    _paper(g,WORK_HOURS_CENTER,[.270,.185],print_mat,[.5,0.,1.,1.],yaw=0)
    # Existing plate center[-.47,.99,-1.25]. Keep all meal detail inside its
    # rim; no taller clutter hides the route or existing kitchen switch.
    _rounded_box(g,[-.47,1.034,-1.25],[.19,.065,.15],kraft,.009,subdivisions=1)
    _rounded_box(g,[-.47,1.069,-1.25],[.205,.008,.16],kraft,.004,subdivisions=1)
    for dx in [-.045,.045]:
        g.box([-.47+dx,1.074,-1.25],[.003,.002,.145],fold)
    # Canvas bag already lives on the entry bench. A thin patch is cosmetic
    # and stays against its exposed face; there is no new floor obstruction.
    _rounded_box(g,[1.361,.90,4.469],[.13,.06,.002],work_badge,.0007,
                 yaw=-8,subdivisions=1)
    for dx in [-.037,-.019,0.,.019,.037]:
        # Follow the canvas bag's accepted -8 degree yaw instead of leaving a
        # flat, floating patch in front of a rotated source prop.
        angle=math.radians(-8)
        c,s=math.cos(angle),math.sin(angle)
        points=[]
        for xx,yy in [(-.002,-.015),(-.002,.015),(.002,.015),(.002,-.015)]:
            points.append([1.361+(dx+xx)*c,.90+yy,4.4675-(dx+xx)*s])
        g.mesh(points,[[-s,0,-c]]*4,[[0,1],[0,0],[1,0],[1,1]],
               [0,1,2,0,2,3],fold)
    added=[]
    for mat,parts in g.geometry.items():added.extend(parts[before.get(mat,0):])
    points=np.concatenate([p[0]for p in added])
    return dict(name='An ordinary overworked father struggling to keep up',
        authored_groups=['Original rent and household-cost reminder on the coffee table',
                         'Six-day long-shift timesheet on the breakfast table',
                         'Modest takeaway meal on the existing plate',
                         'Worn badge on the existing entry work bag'],
        readable_story_anchors=dict(rent_notice=RENT_NOTICE_CENTER,
                                    work_hours=WORK_HOURS_CENTER),
        atlas={**atlas_meta,**dict(sha256=hashlib.sha256(atlas).hexdigest(),
                                embedded_bytes=len(atlas),texture_index=texture)},
        minimum=points.min(0).tolist(),maximum=points.max(0).tolist(),
        triangles=int(sum(len(p[3])//3 for p in added)),
        new_materials=[m['name']for m in g.doc['materials'][first_material:]],
        collision_bodies_added=0,existing_notebook_unchanged=True,
        existing_room_bounds_unchanged=True,
        policy='Original fictional printed text and cosmetic generated geometry only; no diagnosis, invented tragedy, downloaded assets or source changes.')
