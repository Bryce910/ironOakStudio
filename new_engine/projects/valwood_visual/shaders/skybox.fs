#version 330

in vec3 fragTexCoord;
uniform sampler2D skyboxTexture;
out vec4 finalColor;

#define PI 3.14159265358979323846

void main() {
    
    vec3 dir = normalize(fragTexCoord);
    
   
    float phi = atan(dir.z, -dir.x); 
    float theta = asin(dir.y);
    
    vec2 uv = vec2(phi / (2.0 * PI) + 0.5, theta / PI + 0.5);
    
    finalColor = texture(skyboxTexture, uv);
}