#version 330 core
// Historical visual reference, frozen before the 2026-10-23 optimization pass.
// Test data only: the original 25-tap SSAO blur, not a runtime shader.
in vec2 vUV;
uniform sampler2D uAO;
out float FragColor;
void main(){
    vec2 ts=1.0/vec2(textureSize(uAO,0));
    float ao=0.0;
    for(int x=-2;x<=2;x++)for(int y=-2;y<=2;y++)
        ao+=texture(uAO,vUV+vec2(float(x),float(y))*ts).r;
    FragColor=ao/25.0;
}
