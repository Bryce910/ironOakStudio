#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uColor,uDepth,uIndirect,uField;
uniform vec2 uHalfSize,uNearFar;
uniform float uAmbientStrength,uContactStrength;
float linear_depth(float d){return 2.*uNearFar.x*uNearFar.y/max(uNearFar.y+uNearFar.x-(d*2.-1.)*(uNearFar.y-uNearFar.x),.0001);}
void main(){
    vec4 scene=texture(uColor,vUV);float z=linear_depth(texture(uDepth,vUV).r);
    vec2 coordinate=vUV*uHalfSize-.5,base=floor(coordinate),f=fract(coordinate);
    vec2 sum=vec2(0);float total=0.;
    for(int y=0;y<2;y++)for(int x=0;x<2;x++){
        vec2 uv=(base+vec2(x,y)+.5)/uHalfSize;vec4 field=texture(uField,uv);
        float bilinear=(x==0?1.-f.x:f.x)*(y==0?1.-f.y:f.y);
        float weight=bilinear*max(0.,1.-abs(field.b-z)/(.04+z*.008));
        sum+=field.rg*weight;total+=weight;
    }
    vec2 visibility=total>.001?sum/total:vec2(1);
    vec4 indirectSample=texture(uIndirect,vUV);
    vec3 indirect=clamp(indirectSample.rgb,vec3(0),max(scene.rgb,vec3(0)));
    vec3 direct=max(scene.rgb-indirect,vec3(0));
    vec3 color=indirect*mix(1.,visibility.r,uAmbientStrength)+direct*mix(1.,visibility.g,uContactStrength*max(smoothstep(.30,.60,scene.a),indirectSample.a*.65));
    FragColor=vec4(max(color,vec3(0)),scene.a);
}
