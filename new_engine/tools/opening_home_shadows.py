"""Bounded receiver-plane shadow correction for the isolated opening home.

The atlas shadow map spans an exterior landscape. Indoor slanted planes need
comparison depth at each PCF texel center, rather than at the shaded pixel's
center. No other receiver in the scene changes its comparison reference.
"""
from pathlib import Path
import hashlib


def patch_house_shadow(project: Path) -> dict:
    path=Path(project)/'shaders/world_foliage.fs.glsl'
    source=path.read_text()
    begin=source.index('float visibility(vec3 normal) {')
    end=source.index('\nvec3 brdf(',begin)
    original=source[begin:end]
    start='    vec2 texel=1./vec2(textureSize(uShadow,0));\n'
    addition='''    // VALWOOD_OPENING_HOUSE_RECEIVER_PLANE
    // Only the authored staging home receives this indoor precision policy.
    // At zero gradient every legacy compare is bit-for-bit unchanged.
    vec3 absolutePosition=vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y);
    bool houseReceiver=uVegetation==0&&!uGrass&&
        all(greaterThan(absolutePosition,vec3(-3506.,599.,-11006.)))&&
        all(lessThan(absolutePosition,vec3(-3494.,604.,-10994.)));
    vec2 receiverGradient=vec2(0.);
    if(houseReceiver) {
        vec3 dx=dFdx(p),dy=dFdy(p);
        float determinant=dx.x*dy.y-dx.y*dy.x;
        if(abs(determinant)>1.e-12)
            receiverGradient=clamp(vec2(dy.y*dx.z-dx.y*dy.z,
                dx.x*dy.z-dy.x*dx.z)/determinant,vec2(-8.),vec2(8.));
    }
'''
    assert original.count(start)==1
    updated=original.replace(start,start+addition,1)
    old='step(p.z-bias,row[y][x])'
    new='step(p.z-bias+dot((vec2(center+ivec2(x-1,y-1))+.5)*texel-p.xy,receiverGradient),row[y][x])'
    assert updated.count(old)==1;updated=updated.replace(old,new)
    old='step(p.z-bias,texelFetch(uShadow,clamp(center+ivec2(x-1,y-1),ivec2(0),limit),0).r)'
    new='step(p.z-bias+dot((vec2(clamp(center+ivec2(x-1,y-1),ivec2(0),limit))+.5)*texel-p.xy,receiverGradient),texelFetch(uShadow,clamp(center+ivec2(x-1,y-1),ivec2(0),limit),0).r)'
    assert updated.count(old)==1;updated=updated.replace(old,new)
    for offset in ['-.55,-.55','.55,-.55','-.55,.55','.55,.55']:
        old=f'step(p.z-bias,texture(uShadow,p.xy+texel*vec2({offset})).r)'
        uv=f'p.xy+texel*vec2({offset})'
        ref=f'p.z-bias+dot(texel*vec2({offset}),receiverGradient)'
        new=f'step({ref},texture(uShadow,{uv}).r)'
        assert updated.count(old)==1;updated=updated.replace(old,new)
    result=source[:begin]+updated+source[end:]
    path.write_text(result)
    return dict(original_sha256=hashlib.sha256(source.encode()).hexdigest(),
                patched_sha256=hashlib.sha256(result.encode()).hexdigest(),
                scope='Only home receiver world bounds x[-3506,-3494], y[599,604], z[-11006,-10994]; exterior gradient remains exactly0.')
