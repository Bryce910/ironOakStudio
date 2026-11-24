uniform vec3 uSunDir,uSunColor,uAmbient;
uniform float uSunElev;
uniform bool uToonShading;
uniform float uToonBands;
uniform int uNumLights;
uniform vec3 uLightPos[32],uLightColor[32];
uniform float uLightRadius[32];
uniform sampler2D uShadowMap;
uniform sampler2D uShadowMapWide;
uniform mat4 uLightVP;
uniform mat4 uLightVPWide;

uniform vec3 uLightDir[32];
uniform vec4 uLightParams[32];
