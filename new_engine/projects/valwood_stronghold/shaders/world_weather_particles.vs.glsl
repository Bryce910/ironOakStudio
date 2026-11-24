#version 330 core
uniform sampler2D uTerrain,uMacroHeight;
uniform vec2 uResolution,uNearFar,uJitterNDC,uWorldSize,uWind,uRainOrigin,uSplashOrigin;
uniform ivec2 uRainCell,uSplashCell;
uniform vec3 uRight,uUp,uForward,uCamera;
uniform vec4 uLocalBounds;
uniform vec4 uPoolBounds;
uniform float uPoolLevel;
uniform float uTan,uTime,uRain;
uniform bool uHasTerrain,uHasMacro,uHasCover;
uniform int uMode;
out vec2 vParticleUV;
out float vViewDepth,vOpacity,vAge;
out vec3 vRelative;
flat out float vVariation;
flat out int vMode;
uint hash(uint p) {p^=p>>16;p*=0x7feb352du;p^=p>>15;p*=0x846ca68bu;return p^(p>>16);}
float random(uint p) {return float(hash(p)>>8)*(1.0/16777216.0);}
vec2 surface_height(vec2 world) {
    vec2 local=(world-uLocalBounds.xy)*uLocalBounds.zw;
    bool cached=uHasTerrain&&all(greaterThanEqual(local,vec2(0)))&&all(lessThanEqual(local,vec2(1)));
    vec2 support=vec2(0);
    if(cached) {
        support=textureLod(uTerrain,(local*64.0+.5)/65.0,0).rg;
    } else if(uHasMacro) {
        vec2 size=vec2(textureSize(uMacroHeight,0));
        vec2 uv=clamp(world/uWorldSize+.5,0.0,1.0);
        support=vec2(textureLod(uMacroHeight,(uv*(size-1.0)+.5)/size,0).r);
    }
    support=max(support,vec2(0));
    // The ellipse is a conservative water cap. Its dry-bank part is below the
    // real ground and rejected by opaque depth, never a floating splash plane.
    if(all(greaterThan(uPoolBounds.zw,vec2(0)))) {
        vec2 q=(world-uPoolBounds.xy)/uPoolBounds.zw;
        if(dot(q,q)<1.0)support.x=max(0.0,uPoolLevel);
    }
    // G is only a conservative weather blocker. It is never a visible impact
    // plane. Unknown cover after a teleport waits for the bounded local cache.
    support.y=uHasCover?(cached?max(support.x,support.y):6000.0):support.x;
    return support;
}
float ground_height(vec2 world) {return surface_height(world).x;}
void main() {
    const vec2 corners[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
    // A splash has one ground ring and two crossed near-field crown cards.
    // The second and third cards are vertex-rejected outside the close range.
    int card=gl_VertexID/6;
    vec2 corner=corners[gl_VertexID%6];
    vParticleUV=corner;vMode=uMode;vOpacity=0.0;vAge=0.0;vViewDepth=1.0;
    vRelative=vec3(0);vVariation=0.0;
    uint id=uint(gl_InstanceID),seed;
    vec3 center;
    if(uMode==0) {
        ivec2 cell=ivec2(int(id%16u),int((id/16u)%16u));
        uint lane=id/256u;
        ivec2 world_cell=uRainCell+cell;
        seed=hash(uint(world_cell.x)*0x1f123bb5u^uint(world_cell.y)*0x5f356495u^lane*0x9e3779b9u);
        vec2 xz=uRainOrigin+(vec2(cell)+vec2(random(seed),random(seed+1u)))*6.0;
        float speed=1.15+random(seed+7u)*1.1;
        float half_length=.025+random(seed+4u)*.055;
        float phase=random(seed+2u)*3.0+float(lane)*3.0-uTime*speed;
        float low=floor(uCamera.y/16.0)*16.0-16.0;
        float y=low+mod(phase-low,48.0);
        float ground=surface_height(uCamera.xz+xz).y;
        if(random(seed+3u)>uRain||y<ground+half_length){gl_Position=vec4(2,2,2,1);return;}
        center=vec3(xz.x,y-uCamera.y,xz.y);
        float range=length(center.xz);
        // A14m vertical support fits every16m cache rebase. Global fall cycles
        // wrap outside it; height-cell changes cannot expose teleporting drops.
        float volume_fade=1.0-smoothstep(10.0,14.0,abs(y-uCamera.y));
        // The16x16 moving window always contains this40m support disk. Drops
        // reach zero before a6m cell is recycled, so translation cannot pop it.
        vOpacity=(1.0-smoothstep(24.0,40.0,range))*smoothstep(.8,2.0,length(center))*volume_fade*.25;
        vOpacity*=smoothstep(half_length,half_length+.35,y-ground);
        vec3 velocity=normalize(vec3(uWind.x,-speed,uWind.y));
        vec3 view_direction=normalize(center+vec3(.00001));
        vec3 across=cross(view_direction,velocity);
        across=dot(across,across)<.0001?uRight:normalize(across);
        float depth=max(dot(center,uForward),.1);
        float physical_width=half_length;
        float half_width=max(physical_width,depth*uTan/uResolution.y*.60);
        // Integrated subpixel coverage avoids a flashing one-pixel bright line.
        vOpacity*=clamp(physical_width/half_width,.30,1.0);
        center.x+=sin(uTime*.65+float(lane)+random(seed)*6.28)*.6;
        center+=uRight*(corner.x*half_width)+uUp*(corner.y*half_length);
        vOpacity*=2.;
    } else {
        gl_Position=vec4(2,2,2,1);return;
        ivec2 cell=ivec2(int(id%32u),int(id/32u));
        ivec2 world_cell=uSplashCell+cell;
        seed=hash(uint(world_cell.x)*0x1f123bb5u^uint(world_cell.y)*0x5f356495u^0x72a5d341u);
        float period=1.3+random(seed+1u)*1.7;
        float age=mod(uTime+random(seed+2u)*period,period)/.34;
        if(age>=1.0||random(seed+3u)>uRain){gl_Position=vec4(2,2,2,1);return;}
        vec2 xz=uSplashOrigin+(vec2(cell)+vec2(random(seed+4u),random(seed+5u)))*3.0;
        vec2 support=surface_height(uCamera.xz+xz);
        float ground=support.x;
        if(support.y>ground+.30){gl_Position=vec4(2,2,2,1);return;}
        float slope=abs(ground_height(uCamera.xz+xz+vec2(.5,0))-ground)+
                    abs(ground_height(uCamera.xz+xz+vec2(0,.5))-ground);
        if(slope>.45){gl_Position=vec4(2,2,2,1);return;}
        float range=length(xz);
        float radius=.035+age*(.19+random(seed+6u)*.10);
        // The short24-row axis has33m minimum coverage during3m rebases.
        vAge=age;vOpacity=sin(age*3.14159265)*(1.0-smoothstep(12.0,31.5,range))*.39;
        if(card==0) {
            center=vec3(xz.x+corner.x*radius,ground+.025-uCamera.y,xz.y+corner.y*radius);
        } else {
            if(range>=20.0){gl_Position=vec4(2,2,2,1);return;}
            float angle=random(seed+7u)*6.28318531+float(card-1)*1.57079633;
            vec2 across=vec2(cos(angle),sin(angle));
            float crown_height=.045+sin(age*3.14159265)*(.12+random(seed+8u)*.09);
            center=vec3(xz.x,ground+.026-uCamera.y,xz.y);
            center.xz+=across*(corner.x*radius*.90);
            center.y+=(corner.y+1.0)*.5*crown_height;
            vMode=2;
            vOpacity*=1.0-smoothstep(11.0,20.0,range);
        }
    }
    vVariation=random(seed+9u);
    vRelative=center;
    float depth=dot(center,uForward);
    if(depth<=uNearFar.x){gl_Position=vec4(2,2,2,1);vOpacity=0.0;return;}
    vec2 projected=vec2(dot(center,uRight)/(uTan*uResolution.x/uResolution.y),dot(center,uUp)/uTan);
    float a=(uNearFar.y+uNearFar.x)/(uNearFar.y-uNearFar.x);
    float b=-2.0*uNearFar.x*uNearFar.y/(uNearFar.y-uNearFar.x);
    gl_Position=vec4(projected+uJitterNDC*depth,a*depth+b,depth);
    vViewDepth=depth;
}
