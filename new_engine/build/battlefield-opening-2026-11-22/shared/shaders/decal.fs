#version 330 core
in vec2 v_uv;
in float v_type;
in float v_alpha;
out vec4 frag_color;

void main(){
    int t=int(v_type+0.5);
    vec2 uv=v_uv;
    vec4 col=vec4(0.0);
    float d=length(uv-0.5);

    if(t==0){
        /* Scorch - black/dark burnt circle with ember edge */
        float mask=smoothstep(0.50,0.42,d);
        float inner=smoothstep(0.22,0.12,d);
        float edge=smoothstep(0.50,0.46,d)-smoothstep(0.46,0.42,d);
        col.rgb=vec3(0.10,0.07,0.04)*mask;
        col.rgb+=vec3(0.06,0.04,0.02)*inner;
        col.rgb+=vec3(0.42,0.18,0.04)*edge*3.0;
        col.a=mask*(0.78+0.22*inner);
    } else if(t==1){
        /* Moss - green organic splotch */
        float n=sin(uv.x*19.3+uv.y*13.7)*0.042+cos(uv.x*11.1-uv.y*17.3)*0.038;
        float mask=smoothstep(0.50+n,0.40+n,d);
        col=vec4(0.24,0.50,0.16,1.0)*mask;
        col.a=mask*0.72;
    } else if(t==2){
        /* Rune - glowing magic circle with spokes */
        float ring=abs(d-0.40);
        float mask=smoothstep(0.04,0.008,ring);
        float inner_ring=abs(d-0.24);
        mask+=smoothstep(0.03,0.006,inner_ring)*0.6;
        float a=atan(uv.y-0.5,uv.x-0.5);
        float spokes=abs(mod(a+3.14159*2.0,1.0472)-0.5236)*2.0;
        float spoke=smoothstep(0.7,0.98,spokes)*smoothstep(0.44,0.02,d)*0.5;
        mask+=spoke;
        col=vec4(0.38,0.62,1.0,1.0)*mask;
        col.a=mask*0.90;
    } else if(t==3){
        /* Crack - dark fracture lines */
        vec2 c=uv-0.5;
        float len=max(length(c),0.001);
        float line1=abs(c.x*0.82+c.y*0.57)/len;
        float line2=abs(c.x*0.28-c.y*0.96)/len;
        float crack=smoothstep(0.92,0.98,line1)+smoothstep(0.88,0.96,line2);
        float mask=smoothstep(0.50,0.42,d);
        crack*=mask;
        col=vec4(0.07,0.06,0.05,1.0)*crack;
        col.a=crack*0.88;
    } else if(t==4){
        /* Blood - red irregular puddle */
        float n=sin(uv.x*23.1+uv.y*19.7)*0.038+cos(uv.x*15.3-uv.y*11.1)*0.032;
        float mask=smoothstep(0.50+n,0.34+n,d);
        float sheen=smoothstep(0.18,0.04,d)*0.15;
        col=vec4(0.58+sheen,0.04,0.04,1.0)*mask;
        col.a=mask*0.80;
    } else {
        /* Mud - brown earthy splotch */
        float n=sin(uv.x*14.7+uv.y*11.3)*0.050+cos(uv.x*9.1-uv.y*13.7)*0.040;
        float mask=smoothstep(0.50+n,0.40+n,d);
        col=vec4(0.36,0.26,0.14,1.0)*mask;
        col.a=mask*0.62;
    }

    col.a*=v_alpha;
    if(col.a<0.008) discard;
    frag_color=col;
}
