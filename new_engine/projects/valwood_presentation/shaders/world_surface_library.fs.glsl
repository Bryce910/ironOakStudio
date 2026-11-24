// Inject after the host's existing sampler declarations. The host defines
// STUDIO_SURFACE_SAMPLER to its landscape/sky array (no new sampler unit).
#ifdef VALWOOD_IMPORTED_SURFACES
uniform int uSurfaceImageCount,uSurfaceLayerCount,uSurfaceBinding,uSurfacePaintCount,uSurfacePaintBase,uSurfaceProjectionCount;
uniform int uSurfaceImageRole[16];
uniform ivec4 uSurfaceLayerMaps[8],uSurfaceBindings[64];
uniform vec4 uSurfaceLayerTint[8],uSurfaceLayerParams[8],uSurfaceBlend[64];
uniform ivec4 uSurfaceMipRect[10];
uniform vec4 uSurfacePaintBounds;
uniform vec3 uSurfaceProjectionMinimum,uSurfaceProjectionMaximum;
uniform vec4 uSurfaceProjectionPosition[8],uSurfaceProjectionSize[8],uSurfaceProjectionParams[8];
uniform mat3 uSurfaceProjectionBasis[8];
vec3 surfaceLinear(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
vec4 surfaceImageLevel(int image,vec2 uv,int mip){
    ivec4 r=uSurfaceMipRect[mip];int layer=6+image*6+r.w;vec2 pixel;
    if(mip==0){ivec2 tile=ivec2(floor(uv*2.));layer+=tile.y*2+tile.x;pixel=uv*1020.-vec2(tile)*510.-.5;}
    else pixel=uv*float(r.z)-.5;
    return textureLod(STUDIO_SURFACE_SAMPLER,vec3((pixel+vec2(r.xy)+.5)/513.,float(layer)),0.);
}
vec4 studioImage(int image,vec2 uv,vec2 dx,vec2 dy){
    if(image<0||image>=uSurfaceImageCount)return vec4(1.);
    float footprint=max(dot(dx,dx),dot(dy,dy))*1040400.;
    float lod=clamp(.5*log2(max(footprint,1.)),0.,9.);int first=int(floor(lod));
    vec4 a=surfaceImageLevel(image,fract(uv),first),b=surfaceImageLevel(image,fract(uv),min(first+1,9));
    if(uSurfaceImageRole[image]==0){a.rgb=surfaceLinear(a.rgb);b.rgb=surfaceLinear(b.rgb);}
    return mix(a,b,fract(lod));
}
struct SurfaceSample {vec3 color,normal;float rough,metal,ao,alpha;};
SurfaceSample surfaceSource(vec3 color,float rough,float metal,vec3 normal,float ao){return SurfaceSample(color,normal,rough,metal,ao,1.);}
SurfaceSample surfaceMix(SurfaceSample a,SurfaceSample b,float t){
    vec3 n=mix(a.normal,b.normal,t);if(dot(n,n)<1e-8)n=a.normal;
    return SurfaceSample(mix(a.color,b.color,t),normalize(n),mix(a.rough,b.rough,t),mix(a.metal,b.metal,t),mix(a.ao,b.ao,t),mix(a.alpha,b.alpha,t));
}
SurfaceSample surfaceLayer(int layer,vec2 uv,vec2 dx,vec2 dy,mat3 basis,SurfaceSample source){
    if(layer<0||layer>=uSurfaceLayerCount)return source;
    ivec3 maps=uSurfaceLayerMaps[layer].xyz;vec4 tint=uSurfaceLayerTint[layer],params=uSurfaceLayerParams[layer];
    uv/=tint.w;dx/=tint.w;dy/=tint.w;
    vec4 color=maps.x>=0?studioImage(maps.x,uv,dx,dy):vec4(1.);
    vec3 arm=maps.z>=0?studioImage(maps.z,uv,dx,dy).rgb:vec3(1.);
    vec3 n=source.normal;float normalVariance=0.;
    if(maps.y>=0){vec3 detail=studioImage(maps.y,uv,dx,dy).xyz*2.-1.;normalVariance=clamp(1.-dot(detail,detail),0.,1.)*.35*params.z*params.z;detail.xy*=params.z;detail.y*=params.w;
        vec3 candidate=basis*detail;if(dot(candidate,candidate)>1e-8)n=normalize(candidate);}
    return SurfaceSample(color.rgb*tint.rgb,n,sqrt(min(1.,pow(clamp(arm.g*params.x,.04,1.),2.)+normalVariance)),clamp(arm.b*params.y,0.,1.),clamp(arm.r,0.,1.),color.a);
}
float surfaceNoise(vec2 p){vec2 q=floor(p),f=fract(p);f=f*f*(3.-2.*f);vec3 k=vec3(.1031,.1030,.0973);vec4 v;
    for(int i=0;i<4;i++){vec3 a=fract(vec3(q+vec2(i%2,i/2),q.x)*k);a+=dot(a,a.yzx+33.33);v[i]=fract((a.x+a.y)*a.z);}
    return mix(mix(v.x,v.y,f.x),mix(v.z,v.w,f.x),f.y);
}
void surfacePaint(vec3 world,vec2 dx,vec2 dy,mat3 basis,inout SurfaceSample result){
    if(uSurfacePaintCount==0||world.x<uSurfacePaintBounds.x||world.z<uSurfacePaintBounds.y||world.x>uSurfacePaintBounds.z||world.z>uSurfacePaintBounds.w)return;
    ivec2 tile=ivec2(floor(world.xz/128.));uint hash=(uint(tile.x)*0x8da6b343u^uint(tile.y)*0xd8163841u)&127u;int index=-1;
    for(int i=0;i<128;i++){
        ivec4 key_bytes=ivec4(round(texelFetch(STUDIO_SURFACE_SAMPLER,ivec3(int(hash)*2,512,uSurfacePaintBase),0)*255.));
        int id=int(round(texelFetch(STUDIO_SURFACE_SAMPLER,ivec3(int(hash)*2+1,512,uSurfacePaintBase),0).r*255.))-1;
        if(id<0)break;
        ivec2 key=ivec2(key_bytes.x+key_bytes.y*256,key_bytes.z+key_bytes.w*256)-32768;
        if(all(equal(key,tile))){index=id;break;}hash=(hash+1u)&127u;
    }
    if(index<0)return;int bank=index/32,local=index%32;ivec2 cell=ivec2(local%7,local/7)*65;
    vec2 uv=(vec2(cell)+(world.xz-vec2(tile)*128.)*.5+.5)/513.;
    vec4 a=textureLod(STUDIO_SURFACE_SAMPLER,vec3(uv,float(uSurfacePaintBase+bank*2)),0.);
    vec4 b=textureLod(STUDIO_SURFACE_SAMPLER,vec3(uv,float(uSurfacePaintBase+bank*2+1)),0.);
    SurfaceSample source=result;float total=dot(a+b,vec4(1.));
    vec3 color=result.color*(1.-total),normal=result.normal*(1.-total);float rough=result.rough*(1.-total),metal=result.metal*(1.-total),ao=result.ao*(1.-total);
    for(int i=0;i<8;i++){if(i>=uSurfaceLayerCount)break;float w=i<4?a[i]:b[i-4];if(w<=0.)continue;
        SurfaceSample layer=surfaceLayer(i,world.xz,dx,dy,basis,source);color+=layer.color*w;normal+=layer.normal*w;rough+=layer.rough*w;metal+=layer.metal*w;ao+=layer.ao*w;}
    if(dot(normal,normal)>1e-8)result.normal=normalize(normal);result.color=color;result.rough=rough;result.metal=metal;result.ao=ao;
}
void studioSurface(vec3 world,vec2 uv,mat3 basis,bool terrain,inout vec3 color,inout float rough,inout float metal,inout vec3 normal,inout float ao){
    // Derivatives must precede every spatial branch, including projections.
    vec3 wx=dFdx(world),wy=dFdy(world);vec2 ux=dFdx(uv),uy=dFdy(uv);
    SurfaceSample result=surfaceSource(color,rough,metal,normal,ao);
    if(uSurfaceBinding>=0&&uSurfaceBinding<64){ivec4 binding=uSurfaceBindings[uSurfaceBinding];vec4 p=uSurfaceBlend[uSurfaceBinding];
        float blend=0.;if(binding.z==1)blend=surfaceNoise(world.xz*p.y);else if(binding.z==2)blend=world.y;else if(binding.z==3)blend=1.-abs(basis[2].y);
        blend=binding.z==0?0.:smoothstep(p.z-p.w,p.z+p.w,blend);
        SurfaceSample first=surfaceLayer(binding.x,uv*p.y,ux*p.y,uy*p.y,basis,result),second=surfaceLayer(binding.y,uv*p.y,ux*p.y,uy*p.y,basis,result);
        result=surfaceMix(result,surfaceMix(first,second,blend),p.x);
    }
    if(terrain)surfacePaint(world,wx.xz,wy.xz,basis,result);
    if(uSurfaceProjectionCount>0&&all(greaterThanEqual(world,uSurfaceProjectionMinimum.xyz))&&all(lessThanEqual(world,uSurfaceProjectionMaximum.xyz))){
        for(int i=0;i<8;i++){if(i>=uSurfaceProjectionCount)break;
            mat3 frame=uSurfaceProjectionBasis[i];vec3 size=uSurfaceProjectionSize[i].xyz,q=transpose(frame)*(world-uSurfaceProjectionPosition[i].xyz)/size;
            float box=max(max(abs(q.x),abs(q.y)),abs(q.z));if(box>=1.)continue;
            vec4 params=uSurfaceProjectionParams[i];float face=smoothstep(params.y,1.,dot(basis[2],frame[1]));
            float coverage=(1.-smoothstep(1.-uSurfaceProjectionSize[i].w,1.,box))*face*uSurfaceProjectionPosition[i].w;
            vec2 du=(transpose(frame)*wx).xz/size.xz*.5,dv=(transpose(frame)*wy).xz/size.xz*.5;
            mat3 decalBasis=mat3(frame[0],frame[2],frame[1]);SurfaceSample image=surfaceLayer(int(params.x),q.xz*.5+.5,du,dv,decalBasis,result);
            result=surfaceMix(result,image,coverage*image.alpha);
        }
    }
    color=result.color;rough=result.rough;metal=result.metal;normal=result.normal;ao=result.ao;
}
#else
vec4 studioImage(int image,vec2 uv,vec2 dx,vec2 dy){return vec4(1.);}
void studioSurface(vec3 world,vec2 uv,mat3 basis,bool terrain,inout vec3 color,inout float rough,inout float metal,inout vec3 normal,inout float ao){}
#endif
