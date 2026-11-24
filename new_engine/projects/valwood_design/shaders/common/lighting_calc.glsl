    float sunFade=clamp(uSunElev*10.0,0.0,1.0);
    float shadow=0.0;
    float NdotL=dot(shadow_n,-uSunDir);
    if(NdotL<=0.0){ shadow=1.0*sunFade; }
    else if(sunFade>0.0){
    float slopeBias=clamp(1.0-NdotL,0.0,1.0);
    vec3 shadowOff=shadow_n*(1.5+slopeBias*3.5);
    vec4 lsFrag=uLightVP*vec4(vPos+shadowOff,1.0);
    vec3 projC=lsFrag.xyz/lsFrag.w*0.5+0.5;
    vec2 pd[12]=vec2[12](
        vec2(-0.326,-0.406),vec2(-0.840,-0.074),
        vec2(-0.696, 0.457),vec2(-0.203, 0.621),
        vec2( 0.962,-0.195),vec2( 0.473,-0.480),
        vec2( 0.519, 0.767),vec2( 0.185,-0.893),
        vec2( 0.507, 0.064),vec2( 0.896, 0.412),
        vec2(-0.322,-0.933),vec2(-0.792,-0.598));
    if(projC.z<=1.0&&projC.x>0.02&&projC.x<0.98&&projC.y>0.02&&projC.y<0.98){
        vec2 ts=1.0/vec2(textureSize(uShadowMap,0));
        float bias=0.0015+slopeBias*0.0045;
        float bDist=0.0,bCnt=0.0;
        for(int i=0;i<5;i++){
            float d=texture(uShadowMap,projC.xy+pd[i]*ts*5.0).r;
            if(projC.z-bias>d){bDist+=d;bCnt+=1.0;}
        }
        float pen=(bCnt>0.0)?clamp((projC.z-bDist/bCnt)/(bDist/bCnt)*3.5,0.5,9.0):2.5;
        float pcf=0.0;
        for(int i=0;i<12;i++){
            float sd=texture(uShadowMap,projC.xy+pd[i]*ts*pen).r;
            pcf+=(projC.z-bias>sd)?1.0:0.0;}
        shadow=clamp(pcf/12.0,0.0,0.65)*sunFade;
    } else {
        vec3 shadowOffW=shadow_n*(3.0+slopeBias*6.0);
        vec4 lsW=uLightVPWide*vec4(vPos+shadowOffW,1.0);
        vec3 pW=lsW.xyz/lsW.w*0.5+0.5;
        if(pW.z<=1.0&&pW.x>0.0&&pW.x<1.0&&pW.y>0.0&&pW.y<1.0){
            vec2 tw=1.0/vec2(textureSize(uShadowMapWide,0));
            float pcfw=0.0; float biasW=0.0030+slopeBias*0.0060;
            for(int i=0;i<12;i++){
                float sd=texture(uShadowMapWide,pW.xy+pd[i]*tw*4.0).r;
                pcfw+=(pW.z-biasW>sd)?1.0:0.0;}
            shadow=clamp(pcfw/12.0,0.0,0.55)*sunFade*0.7;
        }
    }
    }
    float diff=max(dot(n,-uSunDir),0.0)*(1.0-shadow);
    if(uToonShading&&diff>0.0){ diff=ceil(diff*uToonBands)/uToonBands; }
    vec3 col=base*(uAmbient+uSunColor*diff);
    for(int i=0;i<uNumLights;i++){
        vec3 lv=uLightPos[i]-vPos;float d=length(lv);
        float att=1.0-clamp(d/uLightRadius[i],0,1);att*=att;
        float ldiff=max(dot(n,normalize(lv)),0.0);
        if(uToonShading&&ldiff>0.0){ ldiff=ceil(ldiff*uToonBands)/uToonBands; }
        col+=base*uLightColor[i]*ldiff*att;
}
