"""Private shader policy for the imported, fully baked Finward demo interior."""
from pathlib import Path
import hashlib


def vec(v):
    return 'vec3('+','.join(format(float(x),'.9f') for x in v)+')'


def patch_reference_shader(project, origin, minimum, maximum, window_boxes, television, console):
    path=Path(project)/'shaders/world_foliage.fs.glsl'
    source=path.read_text();before=hashlib.sha256(source.encode()).hexdigest()
    lo=[a+b for a,b in zip(origin,minimum)];hi=[a+b for a,b in zip(origin,maximum)]
    helper='''// VALWOOD_FINWARD_REFERENCE_INTERIOR_V1
uniform vec4 uHomeRoomLights=vec4(1.);
bool referenceInterior(vec3 p){
    return all(greaterThan(p,LO))&&all(lessThan(p,HI));
}
float referenceRoomWeight(vec3 p){
    if(!referenceInterior(p))return 1.;
    float x=p.x-(ORIGIN_X);
    if(x< -6.)return uHomeRoomLights.y;
    if(x>=13.&&x<=18.)return uHomeRoomLights.z;
    if(x>=27.&&x<=31.)return uHomeRoomLights.w;
    if(x<=5.)return uHomeRoomLights.x;
    return 1.;
}
bool homeArtInteriorReceiver(){
    return uVegetation==0&&!uGrass&&uActorReactive<=0.&&
        referenceInterior(vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y));
}
float homeFixtureEmissionWeight(){
    if(uVegetation!=0||uGrass)return 1.;
    vec3 p=vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y);
WINDOWS
    return referenceRoomWeight(p);
}
'''.replace('LO',vec(lo)).replace('HI',vec(hi)).replace('ORIGIN_X',format(origin[0],'.9f'))
    windows=[]
    for low,high in window_boxes:
        a=[x+y-.025 for x,y in zip(origin,low)];b=[x+y+.025 for x,y in zip(origin,high)]
        windows.append('    if(all(greaterThanEqual(p,'+vec(a)+'))&&all(lessThanEqual(p,'+vec(b)+')))return 1.;')
    helper=helper.replace('WINDOWS','\n'.join(windows))
    start=source.index('// VALWOOD_OPENING_FIXTURE_SWITCHES_V1')
    end=source.index('// VALWOOD_GRAPH_SURFACE',start)
    source=source[:start]+helper+'\n'+source[end:]
    # Interior captures have confidence within their authored rooms. Keeping
    # the outdoor 25% floor (and vertical edge fade) leaks the sunset sky into
    # polished cabinet fronts even when they face a solid interior wall.
    marker='vec3 local_reflection_direction(vec3 world,vec3 direction,int index) {'
    assert source.count(marker)==1
    source=source.replace(marker,'bool referenceInterior(vec3 p);\n'+marker)
    old='    return mix(fallback,max(radiance*uLocalProbeLightScale/total,vec3(0.)),clamp(total*.75,0.,.75));'
    assert source.count(old)==1
    source=source.replace(old,'''    vec3 captured=max(radiance*uLocalProbeLightScale/total,vec3(0.));
    if(referenceInterior(world))return captured;
    return mix(fallback,captured,clamp(total*.75,0.,.75));''')
    old='    radiance=local_reflection(world,reflected,rough,radiance);'
    assert source.count(old)==1
    source=source.replace(old,'''    // During capture/startup, no outdoor sky is substituted behind walls.
    // Once ready, source-room box-projected captures supply the reflection.
    if(referenceInterior(world))radiance=vec3(0.);
    radiance=local_reflection(world,reflected,rough,radiance);''')
    old='''    if(homeArtInteriorReceiver()) {
        indirect*=vec3(.45,.46,.48);
        sunlight*=.10;
    }'''
    assert source.count(old)==1
    source=source.replace(old,'''    // The supplied bake already includes static lamps and window area keys.
    if(homeArtInteriorReceiver())sunlight=vec3(0.);''')
    old='            indirect+=diffuseFactor*sourceIrradiance*bakedWeight;'
    assert source.count(old)==1
    source=source.replace(old,'''            if(homeArtInteriorReceiver()){
                // Full Unity bake replaces diffuse ambient, retaining native
                // specular. Room switches approximate a separate electric
                // bounce bank;30% retains the baked daylight baseline.
                float circuit=referenceRoomWeight(vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y));
                indirect+=diffuseFactor*(sourceIrradiance*mix(.30,1.,circuit)-ambient)*bakedWeight;
            }else indirect+=diffuseFactor*sourceIrradiance*bakedWeight;''')
    source=source.replace('if(uCastleLightsEnabled&&uVegetation==0&&!uGrass)\n        color+=castle_practicals',
                          'if(uCastleLightsEnabled&&uVegetation==0&&!uGrass&&!homeArtInteriorReceiver())\n        color+=castle_practicals')
    source=source.replace('float fogAmount=clamp(fog,0.,.96);','float fogAmount=homeArtInteriorReceiver()?0.:clamp(fog,0.,.96);')
    # Retain unique material tags and tighten them to the relocated story props.
    tvlo=[a+b-.01 for a,b in zip(origin,television['minimum'])]
    tvhi=[a+b+.01 for a,b in zip(origin,television['maximum'])]
    source=source.replace('vec3(-3503.4,600.8,-11000.9)',vec(tvlo)).replace('vec3(-3502.2,601.8,-11000.6)',vec(tvhi))
    ledlo=[a+b-.011 for a,b in zip(origin,console)];ledhi=[a+b+.011 for a,b in zip(origin,console)]
    source=source.replace('vec3(-3501.92,600.76,-11000.65)',vec(ledlo)).replace('vec3(-3501.90,600.79,-11000.63)',vec(ledhi))
    source=source.replace('vec3(-3506.,599.,-11006.)',vec(lo)).replace('vec3(-3494.,604.,-10994.)',vec(hi))
    path.write_text(source)
    return dict(original_sha256=before,sha256=hashlib.sha256(source.encode()).hexdigest(),
                scope='Private imported interior bounds only; legacy city bake behavior retained outdoors.',
                full_bake_replaces_ambient=True,baked_interior_sun_disabled=True,
                interior_reflections='Normalized room captures with no outdoor sky fallback; original outdoor blend retained elsewhere.',
                practical_limitation='Native practical highlights use 10% source intensity in addition to the source combined bake.',
                independent_window_emission_boxes=len(windows),
                switch_limitation='Combined source bake approximated as30% daylight baseline plus70% switched room light; no separately baked electric/daylight banks supplied.')
