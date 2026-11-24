#!/usr/bin/env python3
"""Original pixel-adventure and optional story title screen for the home TV.

The screen is a single opaque quad inside the owned TV. Its distinctive
material plus tight world-space bounds opt into a private-project shader path.
There is no video decode, per-frame upload, texture atlas, render target or
second gameplay world. The existing renderer time drives every moving layer.
The story uniform is disabled by default, preserving the accepted legacy loop.
"""
from pathlib import Path
import hashlib


TV_EMISSIVE_TAG = [.2137, .4271, .6419]
TV_CENTER = [-2.799567, 1.267020, -.789]
TV_SIZE = [.910 * 2.35 / 2.009109974, .693 * 2.35 / 2.009109974]
TV_LOOP_SECONDS = 8.
TV_WORLD_CENTER = [-3502.799567, 601.267020, -11000.789]
CONSOLE_POWER_LOCAL = [-1.91, .773, -.6386]
CONSOLE_POWER_WORLD = [-3501.91, 600.773, -11000.6386]


def add_animated_tv(g):
    """Insert two triangles; UV Y increases from bottom to top."""
    material = g.material('VALWOOD opening television | animated original pixel adventure',
                          [.006, .009, .014], .95, emissive=TV_EMISSIVE_TAG)
    cx, cy, cz = TV_CENTER
    width, height = TV_SIZE[0]*.976, TV_SIZE[1]*.963
    points = [[cx-width/2,cy-height/2,cz+.0007],
              [cx+width/2,cy-height/2,cz+.0007],
              [cx+width/2,cy+height/2,cz+.0007],
              [cx-width/2,cy+height/2,cz+.0007]]
    g.mesh(points, [[0,0,1]]*4, [[0,0],[1,0],[1,1],[0,1]],
           [0,1,2,0,2,3], material)
    return dict(mode='Real-time original looping platform-adventure animation',
                material=g.doc['materials'][material]['name'],
                emissive_material_tag=TV_EMISSIVE_TAG,
                loop_seconds=TV_LOOP_SECONDS, raster=[320,240],
                moving_layers=['Scrolling island platforms','Parallax clouds',
                               'Running and jumping adventurer','Moving cape and legs',
                               'Spinning gold pickups','Animated water'],
                geometry_triangles=2, extra_textures=0, per_frame_uploads=0,
                playable_second_game=False, time_uniform='uTime',
                story_state_uniform='uHomeGameState',
                story_modes={0:'Powered off / standby',1:'VALOIS / NEW GAME title screen',
                             2:'Loading and transport',3:'Original adventure loop'},
                legacy_animation_when_story_disabled=True,
                temporal_confidence=.2)


# Positions are exact integer pixels on a deliberately modest 320x240 screen.
# The whole scene loops every eight seconds, including gait, water and coins.
# All colours are scene-linear and bounded so HDR tone mapping retains the
# game's colour instead of turning the modest old TV into a white light panel.
TV_GLSL = r'''
// VALWOOD_OPENING_TV_ANIMATION_V1
float homeTVBox(vec2 p,vec2 lo,vec2 hi) {
    return float(all(greaterThanEqual(p,lo))&&all(lessThan(p,hi)));
}
float homeTVEllipse(vec2 p,vec2 center,vec2 radius) {
    vec2 q=(p-center)/max(radius,vec2(.1));
    return float(dot(q,q)<=1.);
}
float homeTVHeart(vec2 p,vec2 center) {
    vec2 q=abs(p-center);
    return float(q.x<5.&&q.y<5.&&q.x+max(-p.y+center.y,0.)<6.&&
        !(p.y>center.y+2.&&q.x<1.));
}
float homeTVDigit(vec2 p,int digit) {
    // Conventional seven-segment readout. 0..9 have an explicit valid mask.
    int mask=63;
    if(digit==1)mask=6;else if(digit==2)mask=91;else if(digit==3)mask=79;
    else if(digit==4)mask=102;else if(digit==5)mask=109;else if(digit==6)mask=125;
    else if(digit==7)mask=7;else if(digit==8)mask=127;else if(digit==9)mask=111;
    float hit=0.;
    if((mask&1)!=0)hit=max(hit,homeTVBox(p,vec2(1,10),vec2(6,12)));
    if((mask&2)!=0)hit=max(hit,homeTVBox(p,vec2(5,6),vec2(7,11)));
    if((mask&4)!=0)hit=max(hit,homeTVBox(p,vec2(5,1),vec2(7,6)));
    if((mask&8)!=0)hit=max(hit,homeTVBox(p,vec2(1,0),vec2(6,2)));
    if((mask&16)!=0)hit=max(hit,homeTVBox(p,vec2(0,1),vec2(2,6)));
    if((mask&32)!=0)hit=max(hit,homeTVBox(p,vec2(0,6),vec2(2,11)));
    if((mask&64)!=0)hit=max(hit,homeTVBox(p,vec2(1,5),vec2(6,7)));
    return hit;
}
vec3 homeTVAdventure(vec2 uv,float absoluteTime) {
    vec2 p=floor(clamp(uv,vec2(0),vec2(.99999))*vec2(320,240));
    float t=mod(absoluteTime,8.);
    float pi=3.14159265359;
    vec3 colour=mix(vec3(.028,.075,.145),vec3(.082,.23,.29),p.y/240.);
    // A distant moon and tiled clouds move more slowly than the foreground.
    colour=mix(colour,vec3(.57,.55,.32),homeTVEllipse(p,vec2(252,178),vec2(15)));
    vec2 cloudPoint=vec2(mod(p.x+t*10.,80.)-40.,p.y-193.);
    float cloud=max(homeTVBox(cloudPoint,vec2(-21,-4),vec2(27,5)),
        max(homeTVBox(cloudPoint,vec2(-12,3),vec2(9,12)),
            homeTVBox(cloudPoint,vec2(4,2),vec2(20,8))));
    colour=mix(colour,vec3(.22,.35,.38),cloud);
    // Repeating triangular ridges maintain an exact seamless loop.
    float farX=mod(p.x+t*10.,80.);
    float farY=113.+(1.-abs(farX-40.)/40.)*46.;
    if(p.y<farY)colour=vec3(.062,.145,.235);
    float nearX=mod(p.x+t*8.+11.,64.);
    float nearY=77.+(1.-abs(nearX-32.)/32.)*42.;
    if(p.y<nearY)colour=vec3(.037,.095,.16);
    // Small animated water glints beneath the floating islands.
    if(p.y<30.) {
        colour=mix(vec3(.018,.065,.105),vec3(.035,.13,.17),p.y/30.);
        float wave=mod(p.x+floor(p.y/5.)*17.+floor(t*8.),32.);
        if(mod(p.y,5.)<1.&&wave<11.)colour+=vec3(.02,.055,.058);
    }
    float worldX=p.x+t*20.;
    float islandX=mod(worldX,160.);
    float rockBottom=20.+abs(islandX-63.)*.27;
    if(islandX<126.&&p.y>=rockBottom&&p.y<56.) {
        colour=vec3(.20,.12,.088);
        float seam=mod(islandX+floor(p.y/9.)*19.,35.);
        if(seam<2.)colour*=.69;
        if(islandX<9.||islandX>116.)colour*=.80;
    }
    if(islandX<126.&&p.y>=53.&&p.y<60.)colour=vec3(.11,.34,.15);
    if(islandX<126.&&p.y>=58.&&p.y<62.)colour=vec3(.23,.49,.20);
    if(islandX<126.&&p.y>=61.&&p.y<64.&&mod(islandX,11.)<3.)
        colour=vec3(.23,.49,.20);
    // Spinning pickups share the island scroll but rotate independently.
    for(int i=0;i<3;i++) {
        float coinX=mod(worldX-(28.+float(i)*38.)+80.,160.)-80.;
        float coinY=93.+(i==2?16.:0.);
        float spin=1.+3.*abs(cos(t*pi+float(i)));
        float coin=homeTVEllipse(vec2(coinX,p.y),vec2(0,coinY),vec2(spin,6));
        colour=mix(colour,vec3(.78,.46,.075),coin);
        float shine=homeTVBox(vec2(coinX,p.y),vec2(-1,coinY-3.),vec2(1,coinY+4.));
        colour=mix(colour,vec3(.92,.72,.23),shine*coin);
    }
    // The adventurer runs at screen X94. Jump before the island gap and land
    // after it; the camera scroll gives a clear platform-game demonstration.
    float heroPhase=mod(94.+t*20.,160.);
    float jumpPhase=mod(heroPhase-108.+160.,160.);
    bool jumping=jumpPhase<70.;
    float lift=jumping?sin(pi*jumpPhase/70.)*40.:0.;
    float stride=sin(t*8.*pi);
    float bob=jumping?0.:abs(stride)*2.;
    vec2 h=p-vec2(94.,64.+floor(lift+bob));
    float cloak=homeTVBox(h,vec2(-13,-1),vec2(-5,14));
    cloak=max(cloak,homeTVBox(h,vec2(-18.-floor(stride*2.),2),vec2(-10,10)));
    colour=mix(colour,vec3(.61,.12,.075),cloak);
    float legOffset=jumping?2.:floor(stride*3.);
    float boots=max(homeTVBox(h,vec2(-4.-legOffset,-2),vec2(1.-legOffset,6)),
                    homeTVBox(h,vec2(3.+legOffset,-2),vec2(8.+legOffset,6)));
    colour=mix(colour,vec3(.14,.10,.075),boots);
    float body=homeTVBox(h,vec2(-5,5),vec2(7,19));
    colour=mix(colour,vec3(.25,.48,.59),body);
    colour=mix(colour,vec3(.68,.47,.29),homeTVBox(h,vec2(-3,19),vec2(7,29)));
    float hair=max(homeTVBox(h,vec2(-5,25),vec2(7,31)),
                   homeTVBox(h,vec2(-5,21),vec2(-2,28)));
    colour=mix(colour,vec3(.075,.045,.033),hair);
    colour=mix(colour,vec3(.01,.02,.025),homeTVBox(h,vec2(5,23),vec2(7,25)));
    colour=mix(colour,vec3(.72,.50,.30),homeTVBox(h,vec2(7,10),vec2(14,14)));
    colour=mix(colour,vec3(.59,.12,.075),homeTVBox(h,vec2(-6,18),vec2(8,21)));
    // Sword flashes as the runner approaches the gap; still unmistakably a
    // tiny game character rather than a solid moving rectangle.
    float blade=homeTVBox(h,vec2(12,12),vec2(15,30));
    colour=mix(colour,vec3(.55,.71,.71),blade);
    colour=mix(colour,vec3(.45,.27,.08),homeTVBox(h,vec2(9,12),vec2(18,15)));
    // Stable HUD; no branding, downloaded footage or licensed game imagery.
    if(p.y>218.)colour*=.49;
    for(int i=0;i<3;i++)
        colour=mix(colour,vec3(.72,.15,.12),homeTVHeart(p,vec2(17.+float(i)*15.,228.)));
    colour=mix(colour,vec3(.88,.56,.10),homeTVEllipse(p,vec2(259,228),vec2(4,6)));
    float digits=max(homeTVDigit(p-vec2(272,222),0),
                 max(homeTVDigit(p-vec2(283,222),2),homeTVDigit(p-vec2(294,222),4)));
    colour=mix(colour,vec3(.72,.78,.67),digits);
    // Barely visible raster rows preserve the inexpensive older-TV character.
    colour*=.96+.04*mod(p.y,2.);
    return colour;
}
'''

TV_STORY_GLSL = r'''
// VALWOOD_OPENING_TV_STORY_V1
// x: enabled (zero preserves the original loop), y: mode 0..3,
// z: normalized transport progress, w: reserved. The CPU owns every state.
uniform vec4 uHomeGameState;
float homeTVLetter(vec2 p,vec2 origin,float scale,int bits) {
    ivec2 q=ivec2(floor((p-origin)/scale));
    if(q.x<0||q.x>2||q.y<0||q.y>4)return 0.;
    return float((bits>>(q.y*3+2-q.x))&1);
}
float homeTVTitle(vec2 p) {
    // Original 3x5 title: VALOIS. Letters are ordinary shader geometry, no font
    // texture upload or atlas. Readable at the seated title-screen framing.
    float t=0.;
    t=max(t,homeTVLetter(p,vec2(91,158),6.,23402)); // V
    t=max(t,homeTVLetter(p,vec2(115,158),6.,11245)); // A
    t=max(t,homeTVLetter(p,vec2(139,158),6.,18727)); // L
    t=max(t,homeTVLetter(p,vec2(163,158),6.,31599)); // O
    t=max(t,homeTVLetter(p,vec2(187,158),6.,29847)); // I
    t=max(t,homeTVLetter(p,vec2(211,158),6.,31183)); // S
    return t;
}
float homeTVNewGame(vec2 p) {
    float t=0.;
    t=max(t,homeTVLetter(p,vec2(114,83),3.,24573)); // N
    t=max(t,homeTVLetter(p,vec2(126,83),3.,31207)); // E
    t=max(t,homeTVLetter(p,vec2(138,83),3.,23549)); // W
    t=max(t,homeTVLetter(p,vec2(162,83),3.,31087)); // G
    t=max(t,homeTVLetter(p,vec2(174,83),3.,11245)); // A
    t=max(t,homeTVLetter(p,vec2(186,83),3.,24557)); // M
    t=max(t,homeTVLetter(p,vec2(198,83),3.,31207)); // E
    return t;
}
vec3 homeTVStory(vec2 uv,float absoluteTime) {
    if(uHomeGameState.x<.5||uHomeGameState.y>2.5)
        return homeTVAdventure(uv,absoluteTime);
    vec2 p=floor(clamp(uv,vec2(0),vec2(.99999))*vec2(320,240));
    if(uHomeGameState.y<.5) {
        // A truly quiet screen makes powering the game on a visible event.
        // Soft falloff resembles the older glass; no animated attraction loop.
        float edge=clamp(1.-length((uv-.5)*vec2(1.,1.25)),0.,1.);
        return vec3(.0018,.0028,.0045)+vec3(.001,.0015,.002)*edge;
    }
    float t=absoluteTime;
    vec3 colour=mix(vec3(.008,.021,.040),vec3(.035,.086,.115),p.y/240.);
    float moon=homeTVEllipse(p,vec2(272,174),vec2(13));
    colour=mix(colour,vec3(.34,.45,.43),moon);
    // A distant mountain keep and a road into a meadow establish the actual
    // destination rather than another unrelated game playing on the set.
    float farHeight=103.+31.*sin(p.x*.018)+14.*sin(p.x*.039+.6);
    if(p.y<farHeight)colour=vec3(.025,.063,.078);
    float nearHeight=61.+16.*sin(p.x*.024+1.2);
    if(p.y<nearHeight)colour=vec3(.020,.052,.047);
    float road=abs(p.x-(161.+sin(p.y*.035)*8.));
    if(p.y<74.&&road<(75.-p.y)*.26+2.)colour=vec3(.065,.086,.069);
    float keep=homeTVBox(p,vec2(144,101),vec2(175,130));
    keep=max(keep,homeTVBox(p,vec2(138,97),vec2(147,140)));
    keep=max(keep,homeTVBox(p,vec2(172,99),vec2(181,143)));
    for(int i=0;i<3;i++) {
        keep=max(keep,homeTVBox(p,vec2(137.+float(i)*4.,139),vec2(139.+float(i)*4.,144)));
        keep=max(keep,homeTVBox(p,vec2(171.+float(i)*4.,142),vec2(173.+float(i)*4.,147)));
    }
    colour=mix(colour,vec3(.015,.031,.035),keep);
    colour=mix(colour,vec3(.20,.15,.052),homeTVBox(p,vec2(155,104),vec2(161,116)));
    // Slow drifting fireflies give the title life without distracting from
    // the NEW GAME prompt. They are procedural and need no emitters.
    for(int i=0;i<12;i++) {
        float seed=float(i);
        vec2 dotAt=vec2(20.+mod(seed*47.+t*(1.+mod(seed,3.)),280.),
                        31.+mod(seed*19.,96.)+sin(t*.7+seed)*4.);
        float spark=homeTVEllipse(p,dotAt,vec2(.9));
        colour=mix(colour,vec3(.32,.37,.16),spark*(.55+.30*sin(t+seed)));
    }
    colour=mix(colour,vec3(.63,.47,.22),homeTVTitle(p));
    // Reserved dark selection strip isolates the primary action from the art.
    float panel=homeTVBox(p,vec2(104,77),vec2(217,104));
    colour=mix(colour,vec3(.013,.025,.030),panel*.90);
    float rule=max(homeTVBox(p,vec2(104,77),vec2(217,78)),
                   homeTVBox(p,vec2(104,103),vec2(217,104)));
    colour=mix(colour,vec3(.26,.20,.09),rule);
    colour=mix(colour,vec3(.69,.62,.43),homeTVNewGame(p));
    if(uHomeGameState.y>1.5) {
        float progress=clamp(uHomeGameState.z,0.,1.);
        float bar=homeTVBox(p,vec2(105,56),vec2(216,60));
        colour=mix(colour,vec3(.035,.065,.066),bar);
        colour=mix(colour,vec3(.31,.50,.44),
            homeTVBox(p,vec2(106,57),vec2(106.+109.*progress,59)));
        // The image becomes the same blue-green radiance as the portal, while
        // preserving enough title to connect its cause to the player's action.
        vec2 d=(uv-.5)*vec2(1.333,1.);
        float r=length(d);
        float glow=exp(-r*r/(.018+progress*.22))*progress;
        float ring=pow(max(0.,1.-abs(r-progress*.62)*12.),3.)*progress;
        colour+=vec3(.05,.25,.24)*glow+vec3(.09,.32,.35)*ring;
    }
    colour*=.97+.03*mod(p.y,2.);
    return colour;
}
'''


TV_MAIN = r'''
    // Only the separately authored console LED; the original owned console
    // shell retains its material. The story starts with this tiny light off.
    if(uHomeGameState.x>.5&&uVegetation==0&&!uGrass&&
       all(lessThan(abs(uEmissiveFactor-vec3(.08,.25,.10)),vec3(.00001)))) {
        vec3 powerWorld=vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y);
        if(all(greaterThan(powerWorld,vec3(-3501.92,600.76,-11000.65)))&&
           all(lessThan(powerWorld,vec3(-3501.90,600.79,-11000.63)))) {
            FragColor=vec4(uHomeGameState.y<.5?vec3(.003,.005,.004):
                vec3(.07,.25,.10),1.);
            IndirectColor=vec4(0.);
            return;
        }
    }
    // Only this dedicated opaque material inside the apartment's actual TV.
    // Returning before general BRDF work keeps animation cheaper than the old
    // many-layer still. Alpha is temporal confidence, not transparency: the
    // opaque quad still writes ordinary depth and occludes the old TV picture.
    if(uVegetation==0&&!uGrass&&
       all(lessThan(abs(uEmissiveFactor-vec3(.2137,.4271,.6419)),vec3(.00001)))) {
        vec3 tvWorld=vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y);
        if(all(greaterThan(tvWorld,vec3(-3503.4,600.8,-11000.9)))&&
           all(lessThan(tvWorld,vec3(-3502.2,601.8,-11000.6)))) {
            FragColor=vec4(homeTVStory(vUV,uTime),.2);
            IndirectColor=vec4(0.);
            return;
        }
    }
'''


def patch_home_tv(project: Path) -> dict:
    """Patch only the newly built project's private foliage fragment shader."""
    path = Path(project)/'shaders/world_foliage.fs.glsl'
    source = path.read_text()
    if 'VALWOOD_OPENING_TV_ANIMATION_V1' in source:
        raise ValueError('TV animation already installed; rebuild the private shader from source')
    marker = '// VALWOOD_GRAPH_SURFACE\nvoid main() {'
    if source.count(marker) != 1:
        raise ValueError('Missing unique production foliage main marker')
    # Keep the original reflection plane clip ahead of our early return.
    clip = '    if(uClipBelowWater&&vPos.y<uWaterClipLevel) discard;\n'
    if source.count(clip) != 1:
        raise ValueError('Missing unique production reflection clip')
    updated = source.replace(marker, TV_GLSL+'\n'+TV_STORY_GLSL+'\n'+marker, 1)
    updated = updated.replace(clip, clip+TV_MAIN, 1)
    path.write_text(updated)
    return dict(original_sha256=hashlib.sha256(source.encode()).hexdigest(),
                patched_sha256=hashlib.sha256(updated.encode()).hexdigest(),
                shader=path.name, loop_seconds=TV_LOOP_SECONDS,
                scope='Distinct material plus actual TV world bounds only; original shared shaders unchanged',
                per_frame_uploads=0, extra_textures=0, extra_render_targets=0,
                temporal_confidence=.2, time_uniform='uTime',
                story_state_uniform='uHomeGameState',
                story_state_semantics=dict(x='Story enabled; 0 retains legacy loop',
                    y='0 off, 1 NEW GAME title, 2 loading/transport, 3 legacy animation',
                    z='Normalized transport progress',w='Reserved'),
                story_console_led_gated=True)
