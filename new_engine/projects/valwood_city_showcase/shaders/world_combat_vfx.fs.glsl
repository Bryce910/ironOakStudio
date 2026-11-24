#version 330 core
in vec2 vUV;flat in vec4 vColor;flat in float vStyle;in float vDepth;
uniform sampler2D uDepth;uniform vec2 uResolution,uNearFar;
layout(location=0)out vec4 FragColor;
float linearDepth(float d){return uNearFar.x*uNearFar.y/max(uNearFar.y-d*(uNearFar.y-uNearFar.x),.00001);}
void main(){float soft=1.;if(vStyle<2.5){float clearance=linearDepth(texture(uDepth,gl_FragCoord.xy/uResolution).r)-vDepth;if(clearance<=0.)discard;soft=clamp(clearance/(vStyle>.5&&vStyle<1.5?.18:.04),0.,1.);}
 float shape;if(vStyle>1.5){shape=(1.-smoothstep(.50,1.,abs(vUV.x)))*(1.-abs(vUV.y)*.65);}else{float r=length(vUV);shape=exp(-r*r*(vStyle>.5?2.8:5.5))*(1.-smoothstep(.65,1.,r));if(vStyle>.5)shape*=.75+.25*sin(vUV.x*9.+sin(vUV.y*7.));}
 float alpha=clamp(shape*vColor.a*soft,0.,.8);if(alpha<.002)discard;FragColor=vec4(vColor.rgb*alpha,alpha);}
