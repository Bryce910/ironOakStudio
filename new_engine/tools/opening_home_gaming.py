#!/usr/bin/env python3
"""Small gaming corner for the opening's already owned apartment furniture.

The original ``tv_stand.glb`` contains the cabinet, television, feet and vase in
one mesh. Keep it intact: this helper inserts an animated picture panel inside
its existing screen and adds accessories on the existing table surfaces.
Coordinates are house-local metres; every prop is cosmetic and adds no collider.
"""
from pathlib import Path
import math
import numpy as np


def dress_gaming_corner(g, furniture, source):
    # Imported here so opening_home_set may call this helper without a cycle.
    from opening_home_set import _rounded_box, _frustum, _tube

    before = {m: len(parts) for m, parts in g.geometry.items()}
    first_material = len(g.doc['materials'])
    black = g.material('Gaming | softly worn charcoal plastic', [.032, .039, .043], .86)
    rubber = g.material('Gaming | dark rubber thumbsticks', [.010, .014, .016], .97)
    grey = g.material('Gaming | faded light grey gamepad shell', [.50, .52, .50], .91)
    led = g.material('Gaming | restrained green power light', [.13, .41, .22], .8,
                     emissive=[.08, .25, .10])
    blue = g.material('Gaming | blue game case', [.026, .11, .26], .89)
    purple = g.material('Gaming | purple game case', [.18, .055, .19], .89)
    cream = g.material('Gaming | cream game sleeve', [.60, .59, .47], .98)
    red = g.material('Gaming | muted coral controls', [.56, .12, .10], .9)
    yellow = g.material('Gaming | muted ochre controls', [.58, .41, .08], .9)
    teal = g.material('Gaming | muted teal controls', [.08, .36, .31], .9)

    # Two triangles retain the existing old TV cabinet and screen dimensions.
    # Its private-project shader draws original looping pixel-game animation;
    # no external clip, video decode or render target runs in the apartment.
    from opening_home_tv import add_animated_tv, TV_CENTER, TV_SIZE
    screen_center = np.array(TV_CENTER)
    screen_size = np.array(TV_SIZE)
    tv_animation = add_animated_tv(g)

    # The owned low-profile console is a separate source model, scaled to a
    # normal 30cm appliance and seated on the cabinet's ~.731m top.
    console=furniture(g,Path(source)/'xbox.glb',-1.95,-.79,
                      width=.30,y=.734,roughness=.88)
    # Add restrained vents and a status light to reinforce its silhouette.
    for x in np.linspace(-2.055,-1.995,7):
        _rounded_box(g,[float(x),.819,-.805],[.003,.002,.11],rubber,.0007,subdivisions=1)
    _rounded_box(g,[-1.91,.773,-.6386],[.012,.006,.003],led,.001,subdivisions=1)
    _tube(g,[[-1.95,.751,-.941],[-1.95,.79,-1.015],[-2.28,.79,-1.02],
             [-2.69,.80,-1.015],[-2.79,.94,-.845]],.004,black,sides=6)
    _tube(g,[[-1.98,.75,-.94],[-2.02,.69,-1.025],[-2.02,.42,-1.025]],
          .0035,black,sides=6)

    def gamepad(x,y,z,shell,yaw=0):
        angle=math.radians(yaw);c,s=math.cos(angle),math.sin(angle)
        def point(px,py,pz):return [x+px*c+pz*s,y+py,z-px*s+pz*c]
        def box(px,py,pz,size,mat,radius=.01,extra_yaw=0):
            _rounded_box(g,point(px,py,pz),size,mat,radius,
                         yaw=yaw+extra_yaw,subdivisions=1)
        def button(px,pz,r,mat):
            _frustum(g,point(px,.028,pz),r,r*.91,.006,mat,sides=10)
        box(0,0,-.01,[.171,.034,.075],shell,.014)
        box(-.060,-.002,.030,[.052,.039,.09],shell,.017,extra_yaw=-17)
        box(.060,-.002,.030,[.052,.039,.09],shell,.017,extra_yaw=17)
        # Twin thumbsticks: raised collars, narrower stems and tactile caps.
        for xx in [-.032,.032]:
            _frustum(g,point(xx,.019,.018),.016,.014,.005,black,sides=12)
            _frustum(g,point(xx,.028,.018),.006,.006,.013,rubber,sides=10)
            _frustum(g,point(xx,.036,.018),.013,.013,.006,rubber,sides=12)
        # Cross-shaped D-pad and four individual coloured face buttons.
        box(-.050,.021,-.026,[.011,.008,.034],black,.002)
        box(-.050,.021,-.026,[.034,.008,.011],black,.002)
        for xx,zz,mat in [(.049,-.042,yellow),(.049,-.014,teal),
                          (.035,-.028,blue),(.063,-.028,red)]:
            button(xx,zz,.0055,mat)
        box(0,.021,-.029,[.014,.006,.008],black,.002)
        for xx in [-.049,.049]:
            box(xx,.003,-.053,[.047,.022,.011],black,.004)

    # The notebook is X[-2.255,-1.945],Z[1.235,1.465], and the cup/books
    # occupy the west side of the table. These accessories leave all of them
    # exposed and retain the existing capsule clearance to the east.
    gamepad(-1.755,.837,1.62,black,yaw=-12)
    gamepad(-2.35,.758,-.682,grey,yaw=8)
    # Slim remote, seated on the near corner; raised buttons are simple enough
    # to survive the mid-distance view without becoming texture noise.
    _rounded_box(g,[-1.735,.824,1.055],[.044,.018,.155],black,.008,subdivisions=1)
    _frustum(g,[-1.735,.837,1.005],.006,.006,.003,red,sides=10)
    for xx in [-1.746,-1.725]:
        for zz in [1.035,1.058,1.081]:
            _rounded_box(g,[xx,.836,zz],[.010,.003,.012],rubber,.002,subdivisions=1)
    for i,case in enumerate([blue,purple]):
        yy=.824+i*.022
        _rounded_box(g,[-2.965,yy,1.085],[.133,.020,.187],case,.005,subdivisions=1)
        _rounded_box(g,[-2.965,yy+.0108,1.085],[.117,.002,.171],cream,.001,subdivisions=1)
        _rounded_box(g,[-2.965,yy+.0125,1.073],[.093,.001,.111],teal if i else blue,.0004,subdivisions=1)
        _rounded_box(g,[-2.965,yy+.0133,1.040],[.055,.001,.025],yellow,.0003,subdivisions=1)

    added=[]
    for mat,parts in g.geometry.items():added.extend(parts[before.get(mat,0):])
    points=np.concatenate([part[0] for part in added])
    tri_count=sum(len(part[3])//3 for part in added)
    return dict(
        name='A modest father\'s television and gaming corner',
        existing_source_preserved='tv_stand.glb cabinet, TV, feet and vase remain intact',
        furnishings=[console],
        authored_groups=['Original looping pixel-platform-adventure TV animation',
                         'Two ergonomic twin-stick gamepads',
                         'Console vents power light and cables',
                         'Slim television remote','Two unbranded game cases'],
        screen=dict(center=screen_center.tolist(),size=screen_size.tolist(),
                    front_normal=[0,0,1],original_source_aspect='Approximately 4:3',
                    animation=tv_animation, mode=tv_animation['mode']),
        minimum=points.min(0).tolist(),maximum=points.max(0).tolist(),
        triangles=int(tri_count),new_materials=[m['name'] for m in g.doc['materials'][first_material:]],
        collision_bodies_added=0,
        clear_notebook=True,
        suggested_inspection_camera=dict(
            eye=[-3501.02,601.65,-10997.55],
            target=[-3502.62,601.14,-11000.65],fov=.90),
        policy='Existing owned console and furniture; original animated game picture and authored accessories; source files unchanged; cosmetic props only.')
