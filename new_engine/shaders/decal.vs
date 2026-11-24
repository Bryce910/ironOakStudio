#version 330 core
layout(location=0) in vec3 a_pos;
layout(location=1) in vec2 a_uv;
layout(location=2) in float a_type;
layout(location=3) in float a_alpha;
uniform mat4 u_proj;
uniform mat4 u_view;
out vec2 v_uv;
out float v_type;
out float v_alpha;
void main(){
    v_uv=a_uv;
    v_type=a_type;
    v_alpha=a_alpha;
    gl_Position=u_proj*u_view*vec4(a_pos,1.0);
}
