// Opt-in material finish. Untagged assets retain their existing PBR response.
void winter_material(vec3 world,vec3 geometricN,inout vec3 normal,
    inout vec3 base,inout float rough,inout float metal,inout float ao) {
    vec3 p=world-vec3(-43344.,1130.80872,-23987.82415651827);
    float coverage=uWinterSurface.x,ice=uWinterSurface.y;
    if(coverage>0.) {
        float patch=noise(p.xz*.47+vec2(p.y*.28,-p.y*.13));
        float cap=smoothstep(.40,.76,geometricN.y+(patch-.5)*.31)*coverage;
        vec3 powder=vec3(.74,.81,.88)*mix(.94,1.02,noise(p.xz*.16));
        base=mix(base,powder,cap);rough=mix(rough,.89,cap);metal*=1.-cap;
        normal=normalize(mix(normal,geometricN,cap*.76));ao=mix(ao,1.,cap*.6);
    }
    if(ice>0.) {
        float veins=noise(vec2(p.x*.71+p.z*.47,p.y*.13));
        float bubbles=noise(p.xy*19.1+p.z*3.7);
        float frost=smoothstep(.49,.82,veins*.55+bubbles*.45)*.42;
        frost=max(frost,smoothstep(.37,.85,geometricN.y)*.72);
        float fractures=0.;
        for(int i=0;i<3;i++) {
            float k=float(i);
            float line=abs(sin(dot(p,vec3(.83+k*.41,.16+k*.071,.37-k*.11))*4.1+sin(p.y*.73+k)*.4));
            float aa=max(fwidth(line),.008);
            fractures=max(fractures,(1.-smoothstep(.008,.018+aa,line))*(.35+.35*noise(p.xy*.7+k)));
        }
        frost=max(frost,fractures*.66);
        vec3 interior=mix(vec3(.23,.35,.39),vec3(.36,.49,.52),veins);
        base=mix(base,mix(interior,vec3(.66,.77,.81),frost),ice);
        normal=normalize(normal+vec3(sin(p.x*27.+p.y*.4),cos(p.y*19.+p.z*11.),sin(p.z*24.+p.y*.7))*.034*ice);
        rough=mix(rough,mix(.21,.57,frost),ice);metal*=1.-ice;
        ao=mix(ao,.91+.09*bubbles,ice);
    }
}
