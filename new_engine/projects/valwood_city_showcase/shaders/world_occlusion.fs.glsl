#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uDepth,uColor;
uniform vec2 uResolution,uNearFar,uJitterNDC;
uniform vec3 uSunView;
uniform float uTan;
float linear_depth(float d){return 2.*uNearFar.x*uNearFar.y/max(uNearFar.y+uNearFar.x-(d*2.-1.)*(uNearFar.y-uNearFar.x),.0001);}
vec3 position(vec2 uv,float d){float z=linear_depth(d);return vec3((uv*2.-1.-uJitterNDC)*vec2(uResolution.x/uResolution.y,1)*uTan*z,-z);}
vec3 point(vec2 uv){return position(uv,texture(uDepth,uv).r);}
vec2 project(vec3 p){return (p.xy/max(-p.z,.001)/vec2(uResolution.x/uResolution.y,1)/uTan+uJitterNDC)*.5+.5;}
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void main(){
    float d=texture(uDepth,vUV).r;vec3 P=position(vUV,d);float z=-P.z;
    if(d>=.99998||z>100.){FragColor=vec4(1,1,min(z,16000.),0);return;}
    vec2 texel=1./uResolution;
    vec3 right=point(vUV+vec2(texel.x,0))-P,left=P-point(vUV-vec2(texel.x,0));
    vec3 up=point(vUV+vec2(0,texel.y))-P,down=P-point(vUV-vec2(0,texel.y));
    vec3 dx=abs(right.z)<abs(left.z)?right:left,dy=abs(up.z)<abs(down.z)?up:down;
    vec3 N=cross(dx,dy);N=dot(N,N)>1.e-10?normalize(N):normalize(-P);if(dot(N,-P)<0.)N=-N;
    float angle=hash(floor(gl_FragCoord.xy))*6.2831853;
    const float radius=1.15;float pixel_radius=min(radius*uResolution.y/(2.*uTan*max(z,.2)),104.);
    float sum=0.,valid=0.;
    for(int i=0;i<16;i++){
        float t=(float(i)+.5)/16.;float theta=angle+float(i)*2.39996323;
        vec2 uv=vUV+vec2(cos(theta),sin(theta))*pixel_radius*sqrt(t)/uResolution;
        if(any(lessThan(uv,vec2(.001)))||any(greaterThan(uv,vec2(.999))))continue;
        valid+=1.;float sample_depth=texture(uDepth,uv).r;if(sample_depth>=.99998)continue;
        vec3 delta=position(uv,sample_depth)-P;float dist=length(delta);
        float horizon=max(dot(N,delta/max(dist,.0001))-.075,0.);
        float range=1.-smoothstep(.03,radius,dist);
        sum+=horizon*range;
    }
    float ao=1.-clamp(sum/max(valid,1.)*1.85,0.,.74);
    float visibility=1.;vec3 L=normalize(uSunView+vec3(0,1.e-8,0));
    if(dot(N,L)>.045){
        for(int i=0;i<8;i++){
            float travel=.035+float(i)*.105;vec3 Q=P+N*.018+L*travel;vec2 uv=project(Q);
            if(Q.z>=-.2||any(lessThan(uv,vec2(.005)))||any(greaterThan(uv,vec2(.995))))break;
            float scene_depth=linear_depth(texture(uDepth,uv).r),gap=-Q.z-scene_depth;
            float bias=.012+z*.00035;
            if(gap>bias&&gap<.12+travel*.18){
                float edge=min(min(uv.x,uv.y),min(1.-uv.x,1.-uv.y));
                float hit=(1.-smoothstep(.40,.80,travel))*smoothstep(.005,.035,edge);
                visibility=min(visibility,1.-hit*.85);
            }
        }
    }
    float stability=texture(uColor,vUV).a;
    ao=mix(1.,ao,mix(.40,1.,smoothstep(.55,.95,stability)));
    float distance_fade=1.-smoothstep(55.,100.,z);
    FragColor=vec4(mix(1.,ao,distance_fade),mix(1.,visibility,distance_fade),z,1);
}
