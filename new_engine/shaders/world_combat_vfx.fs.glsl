#version 330 core
in vec2 vUV;flat in vec4 vColor;flat in float vStyle;in float vDepth;
uniform sampler2D uDepth;uniform vec2 uResolution,uNearFar;
layout(location=0)out vec4 FragColor;
float linearDepth(float d){return uNearFar.x*uNearFar.y/max(uNearFar.y-d*(uNearFar.y-uNearFar.x),.00001);}
void main(){bool blood=vStyle>3.5;bool viewmodelTrail=vStyle>2.5&&vStyle<3.5;float soft=1.;if(!viewmodelTrail){float clearance=linearDepth(texture(uDepth,gl_FragCoord.xy/uResolution).r)-vDepth;if(clearance<=0.)discard;soft=clamp(clearance/(vStyle>.5&&vStyle<1.5?.18:blood?.025:.04),0.,1.);}
 float shape;if(blood){float r=length(vec2(vUV.x*1.12,vUV.y));shape=vStyle>4.5?exp(-r*r*2.4)*(1.-smoothstep(.35,1.,r)):(1.-smoothstep(.40,1.,r))*(.88+.12*sin(vUV.y*11.+vUV.x*7.));}else if(vStyle>1.5){shape=(1.-smoothstep(.50,1.,abs(vUV.x)))*(1.-abs(vUV.y)*.65);}else{float r=length(vUV);shape=exp(-r*r*(vStyle>.5?2.8:5.5))*(1.-smoothstep(.65,1.,r));if(vStyle>.5)shape*=.75+.25*sin(vUV.x*9.+sin(vUV.y*7.));}
 float alpha=clamp(shape*vColor.a*soft,0.,blood?.96:.8);if(alpha<.002)discard;FragColor=vec4(vColor.rgb*alpha,alpha);}
