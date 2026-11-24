#version 330 core
in vec2 vUV;
in vec4 vPreviousClip,vStaticClip;
out vec4 Motion;
uniform sampler2D uBase;
uniform bool uHasBase;
uniform float uAlphaCutoff;
uniform vec4 uBaseFactor;
void main() {
    if(uAlphaCutoff>0.) {
        float alpha=uBaseFactor.a*(uHasBase?texture(uBase,vUV).a:1.);
        if(alpha<uAlphaCutoff)discard;
    }
    // Previous-NDC offset of this surface point relative to where a static
    // point would reproject. a=1 marks valid object motion.
    Motion=vec4(vPreviousClip.xyz/vPreviousClip.w-vStaticClip.xyz/vStaticClip.w,1.);
}
